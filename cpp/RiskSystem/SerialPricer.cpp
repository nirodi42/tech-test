#include "SerialPricer.h"
#include <stdexcept>

#include "../Pricers/GovBondPricingEngine.h"
#include "../Pricers/CorpBondPricingEngine.h"
#include "../Pricers/FxPricingEngine.h"

// was empty so engines don't get deleted; use unique_ptr instead
SerialPricer::~SerialPricer() {
    for (auto& entry : pricers_) {
        delete entry.second;
    }
}

void SerialPricer::loadPricers() {

    // avoid creating the same engines and overwriting pointers if multiple price()
    // but if throw (for supra), it won't start over.. better to store temp until compl, then transfer to procers_
    if (!pricers_.empty()) {
        return;
    }

    PricingConfigLoader pricingConfigLoader;
    pricingConfigLoader.setConfigFile("./PricingConfig/PricingEngines.xml");
    PricingEngineConfig pricerConfig = pricingConfigLoader.loadConfig();

    for (const auto& configItem : pricerConfig) {

        const std::string& typeName = configItem.getTypeName();
        const std::string& tradeType = configItem.getTradeType();

        // to interface, to use ->price() on all of them
        IPricingEngine* engine = nullptr;

        // can't make class from string..
        if (typeName == "HmxLabs.TechTest.Pricers.GovBondPricingEngine") {
            engine = new GovBondPricingEngine();

        } else if (typeName == "HmxLabs.TechTest.Pricers.CorpBondPricingEngine") {
            engine = new CorpBondPricingEngine();

        } else if (typeName == "HmxLabs.TechTest.Pricers.FxPricingEngine") {
            // two of these will be created, could maybe share
            engine = new FxPricingEngine();

        } else {
            throw std::runtime_error("Unknown pricing engine: " + typeName);
        }

        // add checks in case doubly-defined in xml etc
        pricers_[tradeType] = engine;
    }
}

void SerialPricer::price(const std::vector<std::vector<ITrade*>>& tradeContainers, 
                         IScalarResultReceiver* resultReceiver) {
    loadPricers();
    
    for (const auto& tradeContainer : tradeContainers) {
        for (ITrade* trade : tradeContainer) {
            std::string tradeType = trade->getTradeType();
            if (pricers_.find(tradeType) == pricers_.end()) {
                resultReceiver->addError(trade->getTradeId(), "No Pricing Engines available for this trade type");
                continue;
            }
            
            IPricingEngine* pricer = pricers_[tradeType];
            pricer->price(trade, resultReceiver);
        }
    }
}
