#pragma once

#include "resource_id.h"
#include "workshop_type.h"
#include "workshop_array.h"

class WorkshopItem;
class WorkshopQueuedItem;
struct WorkshopItemCfg;
class Collective;
class CostInfo;
class ContentFactory;

class Workshops {
  public:
  typedef WorkshopItem Item;
  typedef WorkshopQueuedItem QueuedItem;

  static double getLegendarySkillThreshold();

  class Type {
    public:
    Type(const vector<Item>& options);
    const vector<Item>& getOptions() const;
    const vector<QueuedItem>& getQueued() const;
    struct WorkshopResult {
      vector<PItem> items;
      bool wasUpgraded;
    };
    WorkshopResult addWork(Collective*, double workAmount, double skillAmount, double morale);
    void queue(int index, int count = 1);
    vector<PItem> unqueue(int index);
    void changeNumber(int index, int number);
    bool isIdle(const Collective*, double skillAmount, double morale) const;
    void addUpgrade(int index, PItem);
    PItem removeUpgrade(int itemIndex, int runeIndex);

    SERIALIZATION_DECL(Type)

    private:
    friend class Workshops;
    void stackQueue();
    void addDebt(CostInfo);
    void checkDebtConsistency() const;
    vector<Item> SERIAL(options);
    vector<QueuedItem> SERIAL(queued);
    unordered_map<CollectiveResourceId, int, CustomHash<CollectiveResourceId>> SERIAL(debt);
  };

  SERIALIZATION_DECL(Workshops)
  Workshops(WorkshopArray, const ContentFactory*);
  Workshops(const Workshops&) = delete;
  int getDebt(CollectiveResourceId) const;
  vector<WorkshopType> getWorkshopsTypes() const;
  map<WorkshopType, Type> SERIAL(types);
};
