#pragma once

#include "util.h"
#include "position.h"
#include "fx_info.h"
#include "view_id.h"
#include "furniture_type.h"
#include "tech_id.h"

class Model;
class Technology;
class Collective;

namespace EventInfo {

  struct CreatureMoved {
    Creature* creature = nullptr;
  };

  struct CreatureKilled {
    Creature* victim = nullptr;
    Creature* attacker = nullptr;
  };

  struct ItemsPickedUp {
    Creature* creature = nullptr;
    vector<Item*> items;
  };

  struct ItemsDropped {
    Creature* creature = nullptr;
    vector<Item*> items;
  };

  struct ItemsAppeared {
    Position position;
    vector<Item*> items;
  };

  struct Projectile {
    optional<FXInfo> fx;
    optional<ViewId> viewId;
    Position begin;
    Position end;
    optional<SoundId> sound;
  };

  struct ConqueredEnemy {
    Collective* collective = nullptr;
    bool byPlayer;
  };

  struct WonGame {};

  struct RetiredGame {};

  struct TechbookRead {
    TechId technology;
  };

  struct Alarm {
    Position pos;
    bool silent = false;
  };

  struct CreatureTortured {
    Creature* victim = nullptr;
    Creature* torturer = nullptr;
  };

  struct CreatureStunned {
    Creature* victim = nullptr;
    Creature* attacker = nullptr;
  };

  struct CreatureAttacked {
    Creature* victim = nullptr;
    Creature* attacker = nullptr;
    AttrType damageAttr;
  };

  struct TrapDisarmed {
    Position pos;
    Creature* creature = nullptr;
  };

  struct FurnitureDestroyed {
    Position position;
    FurnitureType type;
    FurnitureLayer layer;
  };

  struct FX {
    Position position;
    FXInfo fx;
    optional<Vec2> direction = none;
  };

  struct ItemsEquipped {
    Creature* creature = nullptr;
    vector<Item*> items;
  };

  struct CreatureEvent {
    Creature* creature = nullptr;
    string message;
  };

  struct ItemStolen {
    Creature* creature = nullptr;
    Position shopPosition;
  };

  struct VisibilityChanged {
    Position pos;
  };

  struct MovementChanged {
    Position pos;
  };

  struct LeaderWounded {
    Creature* c = nullptr;
  };

#define VARIANT_TYPES_LIST\
  X(CreatureMoved, 0)\
  X(CreatureKilled, 1)\
  X(ItemsPickedUp, 2)\
  X(ItemsDropped, 3)\
  X(ItemsAppeared, 4)\
  X(Projectile, 5)\
  X(ConqueredEnemy, 6)\
  X(WonGame, 7)\
  X(TechbookRead, 8)\
  X(Alarm, 9)\
  X(CreatureTortured, 10)\
  X(CreatureStunned, 11)\
  X(MovementChanged, 12)\
  X(TrapDisarmed, 13)\
  X(FurnitureDestroyed, 14)\
  X(ItemsEquipped, 15)\
  X(CreatureEvent, 16)\
  X(VisibilityChanged, 17)\
  X(RetiredGame, 18)\
  X(CreatureAttacked, 19)\
  X(FX, 20)\
  X(ItemStolen, 21)\
  X(LeaderWounded, 22)

#define VARIANT_NAME GameEvent

#include "gen_variant.h"

#undef VARIANT_TYPES_LIST
#undef VARIANT_NAME

}

class GameEvent : public EventInfo::GameEvent {
  using EventInfo::GameEvent::GameEvent;
};
