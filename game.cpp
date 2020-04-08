#include "stdafx.h"
#include "game.h"
#include "view.h"
#include "clock.h"
#include "tribe.h"
#include "music.h"
#include "player_control.h"
#include "village_control.h"
#include "model.h"
#include "creature.h"
#include "spectator.h"
#include "statistics.h"
#include "collective.h"
#include "options.h"
#include "territory.h"
#include "level.h"
#include "highscores.h"
#include "player.h"
#include "item_factory.h"
#include "item.h"
#include "map_memory.h"
#include "creature_attributes.h"
#include "name_generator.h"
#include "campaign.h"
#include "save_file_info.h"
#include "file_sharing.h"
#include "villain_type.h"
#include "attack_trigger.h"
#include "view_object.h"
#include "campaign.h"
#include "construction_map.h"
#include "campaign_builder.h"
#include "campaign_type.h"
#include "game_save_type.h"
#include "player_role.h"
#include "collective_config.h"
#include "attack_behaviour.h"
#include "village_behaviour.h"
#include "collective_builder.h"
#include "game_event.h"
#include "version.h"
#include "content_factory.h"
#include "collective_name.h"

template <class Archive> 
void Game::serialize(Archive& ar, const unsigned int version) {
  ar & SUBCLASS(OwnedObject<Game>);
  ar(villainsByType, collectives, lastTick, playerControl, playerCollective, currentTime);
  ar(musicType, statistics, tribes, gameIdentifier, players, contentFactory, sunlightTimeOffset);
  ar(gameDisplayName, models, visited, baseModel, campaign, localTime, turnEvents);
  if (Archive::is_loading::value)
    sunlightInfo.update(getGlobalTime() + sunlightTimeOffset);
}

SERIALIZABLE(Game);
SERIALIZATION_CONSTRUCTOR_IMPL(Game);

static string getGameId(SaveFileInfo info) {
  return info.filename.substr(0, info.filename.size() - 4);
}

Game::Game(Table<PModel>&& m, Vec2 basePos, const CampaignSetup& c, ContentFactory f)
    : models(std::move(m)), visited(models.getBounds(), false), baseModel(basePos),
      tribes(Tribe::generateTribes()), musicType(MusicType::PEACEFUL), campaign(c.campaign),
      contentFactory(std::move(f)) {
  gameIdentifier = c.gameIdentifier;
  gameDisplayName = c.gameDisplayName;
  for (Vec2 v : models.getBounds())
    if (WModel m = models[v].get()) {
      for (Collective* col : m->getCollectives()) {
        auto control = dynamic_cast<VillageControl*>(col->getControl());
        control->updateAggression(c.enemyAggressionLevel);
        addCollective(col);
      }
      m->updateSunlightMovement();
      for (auto c : m->getAllCreatures())
        c->setGlobalTime(getGlobalTime());
    }
  turnEvents = {0, 10, 50, 100, 300, 500};
  for (int i : Range(200))
    turnEvents.insert(1000 * (i + 1));
}

void Game::addCollective(Collective* col) {
  if (!collectives.contains(col)) {
    collectives.push_back(col);
    auto type = col->getVillainType();
    villainsByType[type].push_back(col);
  }
}

void Game::spawnKeeper(AvatarInfo avatarInfo, vector<string> introText) {
  auto model = getMainModel().get();
  WLevel level = model->getTopLevel();
  Creature* keeperRef = avatarInfo.playerCreature.get();
  CHECK(level->landCreature(StairKey::keeperSpawn(), keeperRef)) << "Couldn't place keeper on level.";
  model->addCreature(std::move(avatarInfo.playerCreature));
  auto keeperInfo = *avatarInfo.creatureInfo.getReferenceMaybe<KeeperCreatureInfo>();
  auto builder = CollectiveBuilder(CollectiveConfig::keeper(
          TimeInterval(keeperInfo.immigrantInterval), keeperInfo.maxPopulation, keeperInfo.populationString,
          keeperInfo.prisoners, ConquerCondition::KILL_LEADER),
      keeperRef->getTribeId())
      .setModel(model)
      .addCreature(keeperRef, keeperInfo.minionTraits);
  if (avatarInfo.chosenBaseName)
    builder.setLocationName(*avatarInfo.chosenBaseName);
  model->addCollective(builder.build(contentFactory.get()));
  playerCollective = model->getCollectives().back();
  CHECK(!!playerCollective->getName()->shortened);
  auto playerControlOwned = PlayerControl::create(playerCollective, introText, keeperInfo.tribeAlignment);
  playerControl = playerControlOwned.get();
  playerCollective->setControl(std::move(playerControlOwned));
  playerCollective->setVillainType(VillainType::PLAYER);
  addCollective(playerCollective);
  playerControl->loadImmigrationAndWorkshops(contentFactory.get(), keeperInfo);
  for (auto tech : keeperInfo.initialTech)
    playerCollective->acquireTech(tech, false);
}

Game::~Game() {}

PGame Game::campaignGame(Table<PModel>&& models, CampaignSetup& setup, AvatarInfo avatar,
    ContentFactory contentFactory) {
  auto ret = makeOwner<Game>(std::move(models), *setup.campaign.getPlayerPos(), setup, std::move(contentFactory));
  for (auto model : ret->getAllModels())
    model->setGame(ret.get());
  auto avatarCreature = avatar.playerCreature.get();
  if (avatarCreature->getAttributes().isAffectedPermanently(LastingEffect::SUNLIGHT_VULNERABLE))
    ret->sunlightTimeOffset = 1501_visible;
  // Remove sunlight vulnerability temporarily otherwise placing the creature anywhere without cover will fail.
  avatarCreature->getAttributes().removePermanentEffect(LastingEffect::SUNLIGHT_VULNERABLE, 1);
  ret->sunlightInfo.update(ret->getGlobalTime() + ret->sunlightTimeOffset);
  if (setup.campaign.getPlayerRole() == PlayerRole::ADVENTURER)
    ret->getMainModel()->landHeroPlayer(std::move(avatar.playerCreature));
  else
    ret->spawnKeeper(std::move(avatar), setup.introMessages);
  // Restore vulnerability. If the effect wasn't present in the first place then it will zero-out.
  avatarCreature->getAttributes().addPermanentEffect(LastingEffect::SUNLIGHT_VULNERABLE, 1);
  return ret;
}

PGame Game::splashScreen(PModel&& model, const CampaignSetup& s, ContentFactory f) {
  Table<PModel> t(1, 1);
  t[0][0] = std::move(model);
  auto game = makeOwner<Game>(std::move(t), Vec2(0, 0), s, std::move(f));
  for (auto model : game->getAllModels())
    model->setGame(game.get());
  game->spectator.reset(new Spectator(game->models[0][0]->getTopLevel()));
  game->turnEvents.clear();
  return game;
}

bool Game::isTurnBased() {
  return !getPlayerCreatures().empty();
}

GlobalTime Game::getGlobalTime() const {
  PROFILE;
  return GlobalTime((int) currentTime);
}

const vector<Collective*>& Game::getVillains(VillainType type) const {
  static vector<Collective*> empty;
  if (villainsByType.count(type))
    return villainsByType.at(type);
  else
    return empty;
}

WModel Game::getCurrentModel() const {
  if (!players.empty())
    return players[0]->getPosition().getModel();
  else
    return models[baseModel].get();
}

PModel& Game::getMainModel() {
  return models[baseModel];
}

vector<WModel> Game::getAllModels() const {
  vector<WModel> ret;
  for (Vec2 v : models.getBounds())
    if (models[v])
      ret.push_back(models[v].get());
  return ret;
}

bool Game::isSingleModel() const {
  return getAllModels().size() == 1;
}

int Game::getSaveProgressCount() const {
  int saveTime = 0;
  for (auto model : getAllModels())
    saveTime += model->getSaveProgressCount();
  return saveTime;
}

void Game::prepareSiteRetirement() {
  for (Vec2 v : models.getBounds())
    if (models[v] && v != baseModel)
      models[v]->discardForRetirement();
  for (Collective* col : models[baseModel]->getCollectives())
    if (col != playerCollective)
      col->setVillainType(VillainType::NONE);
  if (playerCollective->getVillainType() == VillainType::PLAYER) {
    // if it's not PLAYER then it's a conquered collective and villainType and VillageControl is already set up
    playerCollective->setVillainType(VillainType::MAIN);
    playerCollective->setControl(VillageControl::create(
        playerCollective, CONSTRUCT(VillageBehaviour,
            c.minPopulation = 24;
            c.minTeamSize = 5;
            c.triggers = makeVec<AttackTrigger>(
                RoomTrigger{FurnitureType("THRONE"), 0.0003},
                SelfVictims{},
                StolenItems{}
            );
            c.attackBehaviour = KillLeader{};
            c.ransom = make_pair(0.8, Random.get(500, 700));)));
  }
  playerCollective->retire();
  vector<Position> locationPos;
  for (auto f : contentFactory->furniture.getTrainingFurniture(ExperienceType::SPELL))
    for (auto pos : playerCollective->getConstructions().getBuiltPositions(f))
      locationPos.push_back(pos);
  if (locationPos.empty())
    locationPos = playerCollective->getTerritory().getAll();
  if (!locationPos.empty())
    playerCollective->getTerritory().setCentralPoint(
        Position(Rectangle::boundingBox(locationPos.transform([](Position p){ return p.getCoord();})).middle(),
            playerCollective->getModel()->getTopLevel()));
  for (auto c : playerCollective->getCreatures())
    c->retire();
  playerControl = nullptr;
  WModel mainModel = models[baseModel].get();
  mainModel->setGame(nullptr);
  for (Collective* col : models[baseModel]->getCollectives())
    for (Creature* c : copyOf(col->getCreatures()))
      if (c->getPosition().getModel() != mainModel)
        transferCreature(c, mainModel);
  for (Vec2 v : models.getBounds())
    if (models[v] && v != baseModel)
      for (Collective* col : models[v]->getCollectives())
        for (Creature* c : copyOf(col->getCreatures()))
          if (c->getPosition().getModel() == mainModel)
            transferCreature(c, models[v].get());
  mainModel->prepareForRetirement();
  TribeId::switchForSerialization(playerCollective->getTribeId(), TribeId::getRetiredKeeper());
  UniqueEntity<Item>::offsetForSerialization(Random.getLL());
  UniqueEntity<Creature>::offsetForSerialization(Random.getLL());
}

void Game::doneRetirement() {
  TribeId::clearSwitch();
  UniqueEntity<Item>::clearOffset();
  UniqueEntity<Creature>::clearOffset();
}

optional<ExitInfo> Game::updateInput() {
  if (spectator)
    while (1) {
      UserInput input = view->getAction();
      if (input.getId() == UserInputId::EXIT)
        return ExitInfo(ExitAndQuit());
      if (input.getId() == UserInputId::IDLE)
        break;
    }
  if (playerControl && !isTurnBased()) {
    while (1) {
      UserInput input = view->getAction();
      if (input.getId() == UserInputId::IDLE)
        break;
      else
        lastUpdate = none;
      playerControl->processInput(view, input);
      if (exitInfo)
        return exitInfo;
    }
  }
  return none;
}

static const TimeInterval initialModelUpdate = 2_visible;

void Game::initializeModels() {
  // Give every model a couple of turns so that things like shopkeepers can initialize.
  for (Vec2 v : models.getBounds())
    if (models[v] && getCurrentModel() != models[v].get()) {
      // Use top level's id as unique id of the model.
      auto id = models[v]->getTopLevel()->getUniqueId();
      if (!localTime.count(id)) {
        localTime[id] = (models[v]->getLocalTime() + initialModelUpdate).getDouble();
        updateModel(models[v].get(), localTime[id]);
      }
  }
}

void Game::increaseTime(double diff) {
  auto before = getGlobalTime();
  currentTime += diff;
  auto after = getGlobalTime();
  if (after > before)
    for (auto m : getAllModels())
      for (auto c : m->getAllCreatures())
        c->setGlobalTime(after);
}

optional<ExitInfo> Game::update(double timeDiff) {
  ScopeTimer timer("Game::update timer");
  if (auto exitInfo = updateInput())
    return exitInfo;
  considerRealTimeRender();
  WModel currentModel = getCurrentModel();
  auto currentId = currentModel->getTopLevel()->getUniqueId();
  while (!lastTick || currentTime >= *lastTick + 1) {
    if (!lastTick)
      lastTick = (int)currentTime;
    else
      *lastTick += 1;
    tick(GlobalTime(*lastTick));
  }
  considerRetiredLoadedEvent(getModelCoords(currentModel));
  if (!updateModel(currentModel, localTime[currentId] + timeDiff)) {
    localTime[currentId] += timeDiff;
    increaseTime(timeDiff);
  }
  return exitInfo;
}

void Game::considerRealTimeRender() {
  auto absoluteTime = view->getTimeMilliAbsolute();
  if (!lastUpdate || absoluteTime - *lastUpdate > milliseconds{10}) {
    if (playerControl)
      playerControl->render(view);
    if (spectator)
      view->updateView(spectator.get(), false);
    lastUpdate = absoluteTime;
  }
}

// Return true when the player has just left turn-based mode so we don't increase time in that case.
bool Game::updateModel(WModel model, double totalTime) {
  do {
    bool wasPlayer = !getPlayerCreatures().empty();
    if (!model->update(totalTime))
      return false;
    if (wasPlayer && getPlayerCreatures().empty())
      return true;
    if (wasTransfered) {
      wasTransfered = false;
      return false;
    }
    if (exitInfo)
      return true;
  } while (1);
}

bool Game::isVillainActive(const Collective* col) {
  const WModel m = col->getModel();
  return m == getMainModel().get() || campaign->isInInfluence(getModelCoords(m));
}

void Game::tick(GlobalTime time) {
  PROFILE_BLOCK("Game::tick");
  if (!turnEvents.empty() && time.getVisibleInt() > *turnEvents.begin()) {
    auto turn = *turnEvents.begin();
    if (turn == 0) {
      auto values = campaign->getParameters();
      values["current_mod"] = getOptions()->getStringValue(OptionId::CURRENT_MOD2);
      values["version"] = string(BUILD_DATE) + " " + string(BUILD_VERSION);
      uploadEvent("campaignStarted", values);
    } else
      uploadEvent("turn", {{"turn", toString(turn)}});
    turnEvents.erase(turn);
  }
  auto previous = sunlightInfo.getState();
  sunlightInfo.update(getGlobalTime() + sunlightTimeOffset);
  if (previous != sunlightInfo.getState())
    for (Vec2 v : models.getBounds())
      if (WModel m = models[v].get()) {
        m->updateSunlightMovement();
        if (playerControl)
          playerControl->onSunlightVisibilityChanged();
      }
  INFO << "Global time " << time;
  for (Collective* col : collectives) {
    if (isVillainActive(col))
      col->update(col->getModel() == getCurrentModel());
  }
}

void Game::setExitInfo(ExitInfo info) {
  exitInfo = std::move(info);
}

void Game::exitAction() {
  enum Action { SAVE, RETIRE, OPTIONS, ABANDON};
#ifdef RELEASE
  bool canRetire = playerControl && !playerControl->getTutorial() && gameWon() && players.empty() &&
      campaign->getType() != CampaignType::SINGLE_KEEPER;
#else
  bool canRetire = playerControl && !playerControl->getTutorial() && players.empty();
#endif
  vector<ListElem> elems { "Save and exit the game",
    {"Retire", canRetire ? ListElem::NORMAL : ListElem::INACTIVE} , "Change options", "Abandon the game" };
  auto ind = view->chooseFromList("Would you like to:", elems);
  if (!ind)
    return;
  switch (Action(*ind)) {
    case RETIRE:
      if (view->yesOrNoPrompt("Retire your dungeon and share it online?")) {
        addEvent(EventInfo::RetiredGame{});
        exitInfo = ExitInfo(GameSaveType::RETIRED_SITE);
        return;
      }
      break;
    case SAVE:
      if (!playerControl) {
        exitInfo = ExitInfo(GameSaveType::ADVENTURER);
        return;
      } else {
        exitInfo = ExitInfo(GameSaveType::KEEPER);
        return;
      }
    case ABANDON:
      if (view->yesOrNoPrompt("Do you want to abandon your game? This is permanent and the save file will be removed!")) {
        exitInfo = ExitInfo(ExitAndQuit());
        return;
      }
      break;
    case OPTIONS: options->handle(view, OptionSet::GENERAL); break;
  }
}

Position Game::getTransferPos(WModel from, WModel to) const {
  return to->getTopLevel()->getLandingSquare(StairKey::transferLanding(),
      getModelCoords(from) - getModelCoords(to));
}

void Game::transferCreature(Creature* c, WModel to, const vector<Position>& destinations) {
  WModel from = c->getLevel()->getModel();
  if (from != to) {
    auto transfer = [&] (Creature* c) {
      if (destinations.empty())
        to->transferCreature(from->extractCreature(c), getModelCoords(from) - getModelCoords(to));
      else
        to->transferCreature(from->extractCreature(c), destinations);
    };
    transfer(c);
    for (auto& summon : c->getShamanSummons())
      transfer(summon);
  }
}

bool Game::canTransferCreature(Creature* c, WModel to) {
  return to->canTransferCreature(c, getModelCoords(c->getLevel()->getModel()) - getModelCoords(to));
}

int Game::getModelDistance(const Collective* c1, const Collective* c2) const {
  return getModelCoords(c1->getModel()).dist8(getModelCoords(c2->getModel()));
}
 
Vec2 Game::getModelCoords(const WModel m) const {
  for (Vec2 v : models.getBounds())
    if (models[v].get() == m)
      return v;
  FATAL << "Model not found";
  return Vec2();
}

void Game::presentWorldmap() {
  view->presentWorldmap(*campaign);
}

void Game::transferAction(vector<Creature*> creatures) {
  if (auto dest = view->chooseSite("Choose destination site:", *campaign,
        getModelCoords(creatures[0]->getLevel()->getModel()))) {
    WModel to = NOTNULL(models[*dest].get());
    creatures = creatures.filter([&](const Creature* c) { return c->getPosition().getModel() != to; });
    vector<CreatureInfo> cant;
    for (Creature* c : copyOf(creatures))
      if (!canTransferCreature(c, to)) {
        cant.push_back(CreatureInfo(c));
        creatures.removeElement(c);
      }
    if (!cant.empty() && !view->creatureInfo("These minions will be left behind due to sunlight. Continue?", true, cant))
      return;
    if (!creatures.empty()) {
      for (Creature* c : creatures)
        transferCreature(c, models[*dest].get());
      wasTransfered = true;
    }
  }
}

void Game::considerRetiredLoadedEvent(Vec2 coord) {
  if (!visited[coord]) {
    visited[coord] = true;
    if (auto retired = campaign->getSites()[coord].getRetired())
        uploadEvent("retiredLoaded", {
            {"retiredId", getGameId(retired->fileInfo)},
            {"playerName", getPlayerName()}});
  }
}

Statistics& Game::getStatistics() {
  return *statistics;
}

const Statistics& Game::getStatistics() const {
  return *statistics;
}

Tribe* Game::getTribe(TribeId id) const {
  return tribes.at(id).get();
}

Collective* Game::getPlayerCollective() const {
  return playerCollective;
}

WPlayerControl Game::getPlayerControl() const {
  return playerControl;
}

MusicType Game::getCurrentMusic() const {
  return musicType;
}

void Game::setDefaultMusic() {
  if (sunlightInfo.getState() == SunlightState::NIGHT)
    musicType = MusicType::NIGHT;
  else
    musicType = getCurrentModel()->getDefaultMusic().value_or(MusicType::PEACEFUL);
}

void Game::setCurrentMusic(MusicType type) {
  musicType = type;
}

const SunlightInfo& Game::getSunlightInfo() const {
  return sunlightInfo;
}

string Game::getGameDisplayName() const {
  return gameDisplayName;
}

string Game::getGameIdentifier() const {
  return gameIdentifier;
}

View* Game::getView() const {
  return view;
}

ContentFactory* Game::getContentFactory() {
  return &*contentFactory;
}

ContentFactory Game::removeContentFactory() {
  return std::move(*contentFactory);
}

void Game::conquered(const string& title, int numKills, int points) {
  string text= "You have conquered this land. You killed " + toString(numKills) +
      " enemies and scored " + toString(points) +
      " points. Thank you for playing KeeperRL!\n \n";
  for (string stat : statistics->getText())
    text += stat + "\n";
  view->presentText("Victory", text);
  Highscores::Score score = CONSTRUCT(Highscores::Score,
        c.worldName = getWorldName();
        c.points = points;
        c.gameId = getGameIdentifier();
        c.playerName = title;
        c.gameResult = "achieved world domination";
        c.gameWon = true;
        c.turns = getGlobalTime().getVisibleInt();
        c.campaignType = campaign->getType();
        c.playerRole = campaign->getPlayerRole();
  );
  highscores->add(score);
  highscores->present(view, score);
}

void Game::retired(const string& title, int numKills, int points) {
  int turns = getGlobalTime().getVisibleInt();
  int dungeonTurns = campaign->getPlayerRole() == PlayerRole::ADVENTURER ? 0 :
      (getPlayerCollective()->getLocalTime() - initialModelUpdate).getVisibleInt();
  string text = "You have survived in this land for " + toString(turns) + " turns. You killed " +
      toString(numKills) + " enemies.\n";
  if (dungeonTurns > 0) {
    text += toString(dungeonTurns) + " turns defending the base.\n";
    text += toString(turns - dungeonTurns) + " turns spent adventuring and attacking.\n";
  }
  text += " Thank you for playing KeeperRL!\n \n";
  for (string stat : statistics->getText())
    text += stat + "\n";
  view->presentText("Survived", text);
  Highscores::Score score = CONSTRUCT(Highscores::Score,
        c.worldName = getWorldName();
        c.points = points;
        c.gameId = getGameIdentifier();
        c.playerName = title;
        c.gameResult = "retired";
        c.gameWon = false;
        c.turns = turns;
        c.campaignType = campaign->getType();
        c.playerRole = campaign->getPlayerRole();
  );
  highscores->add(score);
  highscores->present(view, score);
}

bool Game::isGameOver() const {
  return !!exitInfo;
}

void Game::gameOver(const Creature* creature, int numKills, const string& enemiesString, int points) {
  int turns = getGlobalTime().getVisibleInt();
  int dungeonTurns = campaign->getPlayerRole() == PlayerRole::ADVENTURER ? 0 :
      (getPlayerCollective()->getLocalTime() - initialModelUpdate).getVisibleInt();
  string text = "And so dies " + creature->getName().title();
  if (auto reason = creature->getDeathReason()) {
    text += ", " + *reason;
  }
  text += ". You killed " + toString(numKills) + " " + enemiesString + " and scored " + toString(points) + " points.\n";
  if (dungeonTurns > 0) {
    text += toString(dungeonTurns) + " turns defending the base.\n";
    text += toString(turns - dungeonTurns) + " turns spent adventuring and attacking.\n";
  }
  for (string stat : statistics->getText())
    text += stat + "\n";
  view->presentTextBelow("Game over", text);
  Highscores::Score score = CONSTRUCT(Highscores::Score,
        c.worldName = getWorldName();
        c.points = points;
        c.gameId = getGameIdentifier();
        c.playerName = creature->getName().firstOrBare();
        c.gameResult = creature->getDeathReason().value_or("");
        c.gameWon = false;
        c.turns = turns;
        c.campaignType = campaign->getType();
        c.playerRole = campaign->getPlayerRole();
  );
  highscores->add(score);
  highscores->present(view, score);
  exitInfo = ExitInfo(ExitAndQuit());
}

Options* Game::getOptions() {
  return options;
}

Encyclopedia* Game::getEncyclopedia() {
  return encyclopedia;
}

void Game::initialize(Options* o, Highscores* h, View* v, FileSharing* f, Encyclopedia* e) {
  options = o;
  highscores = h;
  view = v;
  fileSharing = f;
  encyclopedia = e;
}

const string& Game::getWorldName() const {
  return campaign->getWorldName();
}

const vector<Collective*>& Game::getCollectives() const {
  return collectives;
}

void Game::addPlayer(Creature* c) {
  if (!players.contains(c))
    players.push_back(c);
}

void Game::removePlayer(Creature* c) {
  players.removeElement(c);
}

const vector<Creature*>& Game::getPlayerCreatures() const {
  return players;
}

static SavedGameInfo::MinionInfo getMinionInfo(const Creature* c) {
  SavedGameInfo::MinionInfo ret;
  ret.level = (int)c->getBestAttack().value;
  ret.viewId = c->getViewObject().id();
  return ret;
}

string Game::getPlayerName() const {
  if (playerCollective) {
    return playerCollective->getName()->shortened.value_or("???"_s);
  } else // adventurer mode
    return players.getOnlyElement()->getName().firstOrBare();
}

SavedGameInfo Game::getSavedGameInfo(vector<string> spriteMods) const {
  if (Collective* col = getPlayerCollective()) {
    vector<Creature*> creatures = col->getCreatures();
    CHECK(!creatures.empty());
    Creature* leader = col->getLeaders()[0];
    CHECK(leader);
    sort(creatures.begin(), creatures.end(), [leader] (const Creature* c1, const Creature* c2) {
        return c1 == leader || (c2 != leader && c1->getBestAttack().value > c2->getBestAttack().value);});
    CHECK(creatures[0] == leader);
    creatures.resize(min<int>(creatures.size(), 4));
    vector<SavedGameInfo::MinionInfo> minions;
    for (Creature* c : creatures)
      minions.push_back(getMinionInfo(c));
    optional<SavedGameInfo::RetiredEnemyInfo> retiredInfo;
    if (auto id = col->getEnemyId()) {
      retiredInfo = SavedGameInfo::RetiredEnemyInfo{*id, col->getVillainType()};
      CHECK(retiredInfo->villainType == VillainType::LESSER || retiredInfo->villainType == VillainType::MAIN)
          << EnumInfo<VillainType>::getString(retiredInfo->villainType);
    }
    return SavedGameInfo{minions, retiredInfo, getPlayerName(), getSaveProgressCount(), std::move(spriteMods)};
  } else {
    auto player = players.getOnlyElement(); // adventurer mode
    return SavedGameInfo{{getMinionInfo(player)}, none, player->getName().bare(), getSaveProgressCount(), std::move(spriteMods)};
  }
}

void Game::uploadEvent(const string& name, const map<string, string>& m) {
  auto values = m;
  values["eventType"] = name;
  values["gameId"] = getGameIdentifier();
  fileSharing->uploadGameEvent(values);
}

void Game::handleMessageBoard(Position pos, Creature* c) {
  int boardId = pos.getHash();
  vector<ListElem> options;
  atomic<bool> cancelled(false);
  view->displaySplash(nullptr, "Fetching board contents...", [&] {
      cancelled = true;
      fileSharing->cancel();
      });
  vector<FileSharing::BoardMessage> messages;
  optional<string> error;
  thread t([&] {
    if (auto m = fileSharing->getBoardMessages(boardId))
      messages = *m;
    else
      error = m.error();
    view->clearSplash();
  });
  view->refreshView();
  t.join();
  if (error) {
    view->presentText("", *error);
    return;
  }
  for (auto message : messages) {
    options.emplace_back(message.author + " wrote:", ListElem::TITLE);
    options.emplace_back("\"" + message.text + "\"", ListElem::TEXT);
  }
  if (messages.empty())
    options.emplace_back("The board is empty.", ListElem::TITLE);
  options.emplace_back("", ListElem::TEXT);
  options.emplace_back("[Write something]");
  if (auto index = view->chooseFromList("", options))
    if (auto text = view->getText("Enter message", "", 80)) {
      if (text->size() >= 2) {
        if (!fileSharing->uploadBoardMessage(getGameIdentifier(), int(combineHash(pos, getGameIdentifier())), c->getName().title(), *text))
          view->presentText("", "Please enable online features in the settings.");
      } else
        view->presentText("", "The message was too short.");
    }
}

bool Game::gameWon() const {
  for (Collective* col : getCollectives())
    if (!col->isConquered() && col->getVillainType() == VillainType::MAIN)
      return false;
  return true;
}

void Game::addEvent(const GameEvent& event) {
  for (Vec2 v : models.getBounds())
    if (models[v])
      models[v]->addEvent(event);
  using namespace EventInfo;
  event.visit<void>(
      [&](const ConqueredEnemy& info) {
        Collective* col = info.collective;
        if (col->getVillainType() != VillainType::NONE) {
          Vec2 coords = getModelCoords(col->getModel());
          if (!campaign->isDefeated(coords)) {
            if (auto retired = campaign->getSites()[coords].getRetired())
              uploadEvent("retiredConquered", {
                  {"retiredId", getGameId(retired->fileInfo)},
                  {"playerName", getPlayerName()}});
            if (coords != campaign->getPlayerPos())
              campaign->setDefeated(coords);
          }
        }
        if (col->getVillainType() == VillainType::MAIN && gameWon()) {
          addEvent(WonGame{});
        }
      },
      [&](const auto&) {}
  );
}

