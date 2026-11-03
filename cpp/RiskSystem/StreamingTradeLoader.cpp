#include "StreamingTradeLoader.h"
#include "../Loaders/BondTradeLoader.h"
#include "../Loaders/FxTradeLoader.h"
#include "PricingConfigLoader.h"
#include <stdexcept>

#include "../Pricers/GovBondPricingEngine.h"
#include "../Pricers/CorpBondPricingEngine.h"
#include "../Pricers/FxPricingEngine.h"

std::vector<ITradeLoader*> StreamingTradeLoader::getTradeLoaders() {
    std::vector<ITradeLoader*> loaders;
    
    BondTradeLoader* bondLoader = new BondTradeLoader();
    bondLoader->setDataFile("TradeData/BondTrades.dat");
    loaders.push_back(bondLoader);
    
    FxTradeLoader* fxLoader = new FxTradeLoader();
    fxLoader->setDataFile("TradeData/FxTrades.dat");
    loaders.push_back(fxLoader);
    
    return loaders;
}

void StreamingTradeLoader::loadPricers() {

    if (!pricers_.empty()) {
        return;
    }

    PricingConfigLoader pricingConfigLoader;
    pricingConfigLoader.setConfigFile("./PricingConfig/PricingEngines.xml");

    PricingEngineConfig pricerConfig =
        pricingConfigLoader.loadConfig();

    for (const auto& configItem : pricerConfig) {

        const std::string typeName = configItem.getTypeName();
        const std::string tradeType = configItem.getTradeType();

        IPricingEngine* engine = nullptr;

        if (typeName == "HmxLabs.TechTest.Pricers.GovBondPricingEngine") {
            engine = new GovBondPricingEngine();
        }
        else if (typeName == "HmxLabs.TechTest.Pricers.CorpBondPricingEngine") {
            engine = new CorpBondPricingEngine();
        }
        else if (typeName == "HmxLabs.TechTest.Pricers.FxPricingEngine") {
            engine = new FxPricingEngine();
        }
        else {
            throw std::runtime_error("Unknown pricing engine: " + typeName);
        }

        auto [it, inserted] = pricers_.emplace(tradeType, engine);

        if (!inserted) {
            delete engine;
            throw std::runtime_error("Duplicate pricing engine for trade type: " + tradeType);
        }
    }
}

StreamingTradeLoader::~StreamingTradeLoader() {
    for (auto& entry : pricers_) {
        delete entry.second;
    }
}

void StreamingTradeLoader::loadAndPrice(IScalarResultReceiver* resultReceiver) {

    loadPricers();

    auto rawLoaders = getTradeLoaders();

    std::vector<std::unique_ptr<ITradeLoader>> loaders;
    loaders.reserve(rawLoaders.size());

    for (auto* loader : rawLoaders) {
        loaders.emplace_back(loader);
    }

    for (const auto& loader : loaders) {

        loader->streamTrades([&](ITrade* trade) {

                const std::string tradeType = trade->getTradeType();
                auto it = pricers_.find(tradeType);

                if (it == pricers_.end()) {
                    resultReceiver->addError(trade->getTradeId(), "No Pricing Engines available for this trade type");
                    return;
                }

                it->second->price(trade, resultReceiver);
            }
        );
    }
}