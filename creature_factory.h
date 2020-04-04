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

#include "util.h"
#include "tribe.h"
#include "experience_type.h"
#include "spell_school.h"
#include "creature_id.h"
#include "spell_school_id.h"
#include "creature_inventory.h"

class Creature;
class MonsterAIFactory;
class Tribe;
class ItemType;
class CreatureAttributes;
class ControllerFactory;
class NameGenerator;
class GameConfig;
struct CreatureInventoryElem;
using CreatureInventory = vector<CreatureInventoryElem>;
class SpellMap;
class ContentFactory;

class CreatureFactory {
  public:
  PCreature fromId(CreatureId, TribeId, const MonsterAIFactory&);
  PCreature fromId(CreatureId, TribeId, const MonsterAIFactory&, const vector<ItemType>& inventory);
  PCreature fromId(CreatureId, TribeId);
  PCreature makeCopy(Creature*, const MonsterAIFactory&);
  PCreature makeCopy(Creature*);
  static PController getShopkeeper(Rectangle shopArea, Creature*);
  static PCreature getRollingBoulder(TribeId, Vec2 direction);
  PCreature getAnimatedItem(PItem, TribeId, int attrBonus);
  static PCreature getHumanForTests();
  static PCreature getAutomatonForTests();
  PCreature getGhost(Creature*);
  static PCreature getIllusion(Creature*);

  static CreatureAttributes getKrakenAttributes(ViewId, const char* name);
  ViewId getViewId(CreatureId) const;
  string getName(CreatureId) const;
  string getNamePlural(CreatureId) const;
  const Gender& getGender(CreatureId);
  vector<CreatureId> getAllCreatures() const;

  NameGenerator* getNameGenerator();

  const map<SpellSchoolId, SpellSchool>& getSpellSchools() const;
  const vector<Spell>& getSpells() const;
  const Spell* getSpell(SpellId) const;
  CreatureFactory(NameGenerator, map<CreatureId, CreatureAttributes>,
      map<SpellSchoolId, SpellSchool>, vector<Spell>);
  ~CreatureFactory();
  CreatureFactory(const CreatureFactory&) = delete;
  CreatureFactory(CreatureFactory&&) noexcept;
  CreatureFactory& operator = (CreatureFactory&&);

  void merge(CreatureFactory);
  void setContentFactory(const ContentFactory*) const;

  SERIALIZATION_DECL(CreatureFactory)

  struct SpecialParams {
    bool humanoid;
    bool large;
    bool living;
    bool wings;
    CreatureInventory inventory;
  };
  static const map<CreatureId, SpecialParams>& getSpecialParams();
  void initializeAttributes(optional<CreatureId>, CreatureAttributes&);

  private:
  void initSplash(TribeId);
  static PCreature getSokobanBoulder(TribeId);
  PCreature getSpecial(CreatureId, TribeId, SpecialParams, const ControllerFactory&);
  PCreature get(CreatureId, TribeId, MonsterAIFactory);
  static PCreature get(CreatureAttributes, TribeId, const ControllerFactory&, SpellMap);
  CreatureAttributes getAttributesFromId(CreatureId);
  HeapAllocated<NameGenerator> SERIAL(nameGenerator);
  map<CreatureId, CreatureAttributes> SERIAL(attributes);
  vector<ItemType> getDefaultInventory(CreatureId) const;
  map<SpellSchoolId, SpellSchool> SERIAL(spellSchools);
  vector<Spell> SERIAL(spells);
  void addInventory(Creature*, const vector<ItemType>& items);
  mutable const ContentFactory* contentFactory = nullptr;
  SpellMap getSpellMap(const CreatureAttributes&);
};

static_assert(std::is_nothrow_move_constructible<CreatureFactory>::value, "T should be noexcept MoveConstructible");
