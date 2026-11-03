#ifndef PARALLELPRICER_H
#define PARALLELPRICER_H

#include "../Models/IPricingEngine.h"
#include "../Models/ITrade.h"
#include "../Models/IScalarResultReceiver.h"
#include "PricingConfigLoader.h"
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <future>

class ParallelPricer {
private:
    std::map<std::string, IPricingEngine*> pricers_;
    std::mutex resultMutex_;
    
    void loadPricers();
    
public:
    ~ParallelPricer();
    
    void price(const std::vector<std::vector<ITrade*>>& tradeContainers, IScalarResultReceiver* resultReceiver);
    ParallelPricer(const ParallelPricer&) = delete;
    ParallelPricer& operator=(const ParallelPricer&) = delete;
    ParallelPricer() = default;
};

#endif // PARALLELPRICER_H
