/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#pragma once

#include "creature_view.h"
#include "entity_set.h"
#include "collective_control.h"
#include "game_info.h"
#include "map_memory.h"
#include "position.h"
#include "event_listener.h"
#include "keeper_creature_info.h"
#include "workshop_type.h"

class Model;
class Technology;
class View;
class Collective;
class CollectiveTeams;
class MapMemory;
class VisibilityMap;
class ListElem;
class UserInput;
class MinionAction;
struct TaskActionInfo;
struct CreatureDropInfo;
struct EquipmentActionInfo;
struct TeamCreatureInfo;
class CostInfo;
struct WorkshopItem;
struct WorkshopQueuedItem;
class ScrollPosition;
class Tutorial;
struct BuildInfo;
class MoveInfo;
class UnknownLocations;
class AttackTrigger;
class ImmigrantInfo;
struct WorkshopOptionInfo;
class PlayerControl : public CreatureView, public CollectiveControl, public EventListener<PlayerControl> {
  public:
  static PPlayerControl create(Collective* col, vector<string> introText, TribeAlignment);
  ~PlayerControl() override;

  void processInput(View* view, UserInput);
  MoveInfo getMove(Creature* c);

  void render(View*);
  void loadImmigrationAndWorkshops(ContentFactory*, const KeeperCreatureInfo&);

  void leaveControl();
  void teamMemberAction(TeamMemberAction, UniqueEntity<Creature>::Id);
  void toggleControlAllTeamMembers();
  void onControlledKilled(const Creature* victim);
  void onSunlightVisibilityChanged();
  void setTutorial(STutorial);
  STutorial getTutorial() const;
  bool isEnemy(const Creature*) const;
  void addToMemory(Position);

  SERIALIZATION_DECL(PlayerControl)

  vector<Creature*> getTeam(const Creature*);
  optional<FurnitureType> getMissingTrainingDummy(const Creature*);
  bool canControlInTeam(const Creature*) const;
  bool canControlSingle(const Creature*) const;
  void addToCurrentTeam(Creature* c);
  void updateUnknownLocations();
  vector<Creature*> getConsumptionTargets(Creature* consumer) const;
  TribeAlignment getTribeAlignment() const;

  void onEvent(const GameEvent&);
  const vector<Creature*>& getControlled() const;

  optional<TeamId> getCurrentTeam() const;
  CollectiveTeams& getTeams();
  const CollectiveTeams& getTeams() const;
  WModel getModel() const;

  private:
  struct Private {};

  public:
  PlayerControl(Private, Collective*, TribeAlignment);

  protected:
  // from CreatureView
  virtual const MapMemory& getMemory() const override;
  virtual void getViewIndex(Vec2 pos, ViewIndex&) const override;
  virtual void refreshGameInfo(GameInfo&) const override;
  virtual Vec2 getScrollCoord() const override;
  virtual Level* getCreatureViewLevel() const override;
  virtual vector<Vec2> getVisibleEnemies() const override;
  virtual double getAnimationTime() const override;
  virtual CenterType getCenterType() const override;
  virtual const vector<Vec2>& getUnknownLocations(WConstLevel) const override;
  virtual optional<Vec2> getSelectionSize() const override;
  virtual vector<vector<Vec2>> getPathTo(UniqueEntity<Creature>::Id, Vec2, bool group) const override;
  virtual vector<vector<Vec2>> getTeamPathTo(TeamId, Vec2) const override;
  virtual vector<Vec2> getHighlightedPathTo(Vec2) const override;

  // from CollectiveControl
  virtual void addAttack(const CollectiveAttack&) override;
  virtual void addMessage(const PlayerMessage&) override;
  virtual void onMemberKilled(const Creature* victim, const Creature* killer) override;
  virtual void onConquered(Creature* victim, Creature* killer) override;
  virtual void onMemberAdded(Creature*) override;
  virtual void onConstructed(Position, FurnitureType) override;
  virtual void onDestructed(Position, FurnitureType, const DestroyAction&) override;
  virtual void onClaimedSquare(Position) override;
  virtual void tick() override;
  virtual void update(bool currentlyActive) override;

  private:

  WLevel getCurrentLevel() const;
  void considerNightfallMessage();
  void considerWarning();

  TribeId getTribeId() const;
  bool canSee(const Creature*) const;
  bool canSee(Position) const;
  bool isConsideredAttacking(const Creature*, const Collective* enemy);

  struct KeeperDangerInfo;
  optional<KeeperDangerInfo> checkKeeperDanger() const;
  static string getWarningText(CollectiveWarning);
  void updateSquareMemory(Position);
  void updateKnownLocations(const Position&);
  vector<Collective*> getKnownVillains() const;
  Collective* getVillain(UniqueEntity<Collective>::Id num);

  Creature* getConsumptionTarget(View*, Creature* consumer);
  Creature* getCreature(UniqueEntity<Creature>::Id id) const;
  void controlSingle(Creature*);
  void commandTeam(TeamId);
  void setScrollPos(Position);

  void handleSelection(Vec2 pos, const BuildInfo&, bool rectangle, bool deselectOnly = false);
  vector<CollectiveInfo::Button> fillButtons() const;
  VillageInfo::Village getVillageInfo(const Collective* enemy) const;
  string getTriggerLabel(const AttackTrigger&) const;
  void fillWorkshopInfo(CollectiveInfo&) const;
  vector<CollectiveInfo::QueuedItemInfo> getQueuedWorkshopItems() const;
  void fillImmigration(CollectiveInfo&) const;
  void fillImmigrationHelp(CollectiveInfo&) const;
  void fillLibraryInfo(CollectiveInfo&) const;
  void fillTechUnlocks(CollectiveInfo::LibraryInfo::TechInfo&) const;
  void fillCurrentLevelInfo(GameInfo&) const;

  void getEquipmentItem(View* view, ItemPredicate predicate);
  ItemInfo getWorkshopItem(const WorkshopItem&, int queuedCount) const;
  Item* chooseEquipmentItem(Creature* creature, vector<Item*> currentItems, ItemPredicate,
      ScrollPosition* scrollPos = nullptr);
  Item* chooseEquipmentItem(Creature* creature, vector<Item*> currentItems, vector<Item*> allItems,
      ScrollPosition* scrollPos = nullptr);

  int getNumMinions() const;
  void minionTaskAction(const TaskActionInfo&);
  void minionDragAndDrop(const CreatureDropInfo&, bool creatureGroup);
  void fillMinions(CollectiveInfo&) const;
  vector<Creature*> getMinionsLike(Creature*) const;
  vector<PlayerInfo> getPlayerInfos(vector<Creature*>, UniqueEntity<Creature>::Id chosenId) const;
  void sortMinionsForUI(vector<Creature*>&) const;
  vector<CollectiveInfo::CreatureGroup> getCreatureGroups(vector<Creature*>) const;
  void minionEquipmentAction(const EquipmentActionInfo&);
  void addEquipment(Creature*, EquipmentSlot);
  void addConsumableItem(Creature*);
  void handleEquipment(View* view, Creature* creature);
  void fillEquipment(Creature*, PlayerInfo&) const;
  void fillAutomatonParts(Creature*, PlayerInfo&) const;
  void handleTrading(Collective* ally);
  vector<Item*> getPillagedItems(Collective*) const;
  bool canPillage(const Collective*) const;
  void handlePillage(Collective* enemy);
  void handleRansom(bool pay);
  ViewObject getTrapObject(FurnitureType, bool built) const;
  void getSquareViewIndex(Position, bool canSee, ViewIndex&) const;
  void onSquareClick(Position);
  WGame getGame() const;
  View* getView() const;
  PController createMinionController(Creature*);

  mutable SMapMemory SERIAL(memory);
  vector<string> SERIAL(introText);
  struct SelectionInfo {
    Vec2 corner1;
    Vec2 corner2;
    bool deselect;
  };
  optional<SelectionInfo> rectSelection;
  void updateSelectionSquares();
  GlobalTime SERIAL(nextKeeperWarning) = GlobalTime(-1000);
  bool wasPausedForWarning = false;
  optional<UniqueEntity<Creature>::Id> chosenCreature;
  void setChosenCreature(optional<UniqueEntity<Creature>::Id>);
  optional<WorkshopType> chosenWorkshop;
  void setChosenWorkshop(optional<WorkshopType>);
  optional<TeamId> getChosenTeam() const;
  void setChosenTeam(optional<TeamId>, optional<UniqueEntity<Creature>::Id> = none);
  optional<TeamId> chosenTeam;
  void clearChosenInfo();
  string getMinionName(CreatureId) const;
  vector<PlayerMessage> SERIAL(messages);
  vector<PlayerMessage> SERIAL(messageHistory);
  vector<CollectiveAttack> SERIAL(newAttacks);
  vector<CollectiveAttack> SERIAL(notifiedAttacks);
  vector<CollectiveAttack> SERIAL(ransomAttacks);
  vector<string> SERIAL(hints);
  optional<PlayerMessage&> findMessage(PlayerMessage::Id);
  SVisibilityMap SERIAL(visibilityMap);
  bool firstRender = true;
  bool isNight = true;
  optional<UniqueEntity<Creature>::Id> draggedCreature;
  void updateMinionVisibility(const Creature*);
  STutorial SERIAL(tutorial);
  void acquireTech(TechId);
  SMessageBuffer SERIAL(controlModeMessages);
  unordered_set<int> dismissedNextWaves;
  vector<ImmigrantDataInfo> getPrisonerImmigrantData() const;
  void acceptPrisoner(int index);
  void rejectPrisoner(int index);
  struct StunnedInfo {
    vector<Creature*> creatures;
    Collective* collective = nullptr;
  };
  vector<StunnedInfo> getPrisonerImmigrantStack() const;
  vector<pair<Creature*, Collective*>> SERIAL(stunnedCreatures);
  ViewId getMinionGroupViewId(Creature*) const;
  SUnknownLocations SERIAL(unknownLocations);
  optional<LocalTime> lastWarningDismiss;
  set<pair<UniqueEntity<Collective>::Id, string>> SERIAL(dismissedVillageInfos);
  void considerTransferingLostMinions();
  vector<PItem> retrievePillageItems(Collective*, vector<Item*> items);
  TribeAlignment SERIAL(tribeAlignment);
  vector<BuildInfo> SERIAL(buildInfo);
  void loadBuildingMenu(const ContentFactory*, const KeeperCreatureInfo&);
  WLevel currentLevel = nullptr;
  void scrollStairs(int dir);
  CollectiveInfo::QueuedItemInfo getQueuedItemInfo(const WorkshopQueuedItem&, int cnt, int itemIndex, bool hasLegendarySkill) const;
  vector<pair<vector<Item*>, Position>> getItemUpgradesFor(const WorkshopItem&) const;
  void fillDungeonLevel(AvatarLevelInfo&) const;
  void fillResources(CollectiveInfo&) const;
  bool takingScreenshot = false;
  void exitAction();
  void addBodyPart(Creature*);
  void handleBanishing(Creature*);
  optional<pair<ViewId,int>> getCostObj(CostInfo) const;
  optional<pair<ViewId,int>> getCostObj(const optional<CostInfo>&) const;
  vector<WorkshopOptionInfo> getWorkshopOptions() const;
  ViewId getViewId(const BuildInfo&) const;
  EntityMap<Creature, LocalTime> leaderWoundedTime;
};

