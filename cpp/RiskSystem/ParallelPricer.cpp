#include "ParallelPricer.h"
#include <stdexcept>

#include "../Pricers/GovBondPricingEngine.h"
#include "../Pricers/CorpBondPricingEngine.h"
#include "../Pricers/FxPricingEngine.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

ParallelPricer::~ParallelPricer() {
    for (auto& entry : pricers_) {
        delete entry.second;
    }
}

namespace { std::unique_ptr<IPricingEngine> createEngine(const std::string& typeName) {
    if (typeName ==
        "HmxLabs.TechTest.Pricers.GovBondPricingEngine") {
        return std::make_unique<GovBondPricingEngine>();
    }

    if (typeName ==
        "HmxLabs.TechTest.Pricers.CorpBondPricingEngine") {
        return std::make_unique<CorpBondPricingEngine>();
    }

    if (typeName ==
        "HmxLabs.TechTest.Pricers.FxPricingEngine") {
        return std::make_unique<FxPricingEngine>();
    }

    throw std::runtime_error(
        "Unknown pricing engine: " + typeName);
    }
    
    class SynchronizedReceiver : public IScalarResultReceiver {
    public:
        SynchronizedReceiver(IScalarResultReceiver* receiver, std::mutex& mutex) : receiver_(receiver), mutex_(mutex) {}
    
        void addResult(const std::string& tradeId, double result) override {
            std::lock_guard<std::mutex> lock(mutex_); //locks mutex when function enters
            receiver_->addResult(tradeId, result);
        }
    
        void addError(const std::string& tradeId, const std::string& error) override {
            std::lock_guard<std::mutex> lock(mutex_);
            receiver_->addError(tradeId, error);
        }
    
    private:
        IScalarResultReceiver* receiver_;
        std::mutex& mutex_;
    };
    
    // separate engine instance for each worker
    std::unique_ptr<IPricingEngine> cloneEngineType(const IPricingEngine* engine) {
        if (dynamic_cast<const GovBondPricingEngine*>(engine)) {
            return std::make_unique<GovBondPricingEngine>();
        }
    
        if (dynamic_cast<const CorpBondPricingEngine*>(engine)) {
            return std::make_unique<CorpBondPricingEngine>();
        }
    
        if (dynamic_cast<const FxPricingEngine*>(engine)) {
            return std::make_unique<FxPricingEngine>();
        }
    
        throw std::runtime_error("Unsupported pricing engine class");
    }
}

void ParallelPricer::loadPricers() {

    if (!pricers_.empty()) {
        return;
    }

    PricingConfigLoader configLoader;
    configLoader.setConfigFile("./PricingConfig/PricingEngines.xml");

    auto config = configLoader.loadConfig();

    // own engines temporarily while building the map
    std::map<std::string, std::unique_ptr<IPricingEngine>> temporary;

    for (const auto& item : config) {

        auto engine = createEngine(item.getTypeName());

        auto [it, inserted] = temporary.emplace(
            item.getTradeType(),
            std::move(engine)
        );

        if (!inserted) {
            throw std::runtime_error(
                "Duplicate pricing configuration: " +
                item.getTradeType()
            );
        }
    }

    // transfer ownership into the existing raw-pointer map
    for (auto& entry : temporary) {
        pricers_.emplace(entry.first, entry.second.get());
    }

    for (auto& entry : temporary) {
        entry.second.release();
    }
}

void ParallelPricer::price(const std::vector<std::vector<ITrade*>>& tradeContainers, IScalarResultReceiver* resultReceiver) {
    if (!resultReceiver) {
        throw std::invalid_argument("Null result receiver");
    }

    loadPricers();

    // collect non-owning pointers to the existing trades
    std::vector<ITrade*> trades;

    for (const auto& container : tradeContainers) {
        trades.insert(
            trades.end(),
            container.begin(),
            container.end()
        );
    }

    if (trades.empty()) {
        return;
    }

    // determine number of worker threads
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();

    const std::size_t workerCount = std::min(trades.size(), static_cast<std::size_t>(hardwareThreads == 0 ? 4 : hardwareThreads));

    std::atomic<std::size_t> nextTrade{0};

    // capture the first exception raised by a worker
    std::exception_ptr workerError;
    std::mutex errorMutex;

    SynchronizedReceiver safeReceiver(resultReceiver, resultMutex_);

    // private engine collection for each worker
    using EngineMap = std::map<std::string, std::unique_ptr<IPricingEngine>>;

    std::vector<EngineMap> workerEngines(workerCount);

    for (auto& engines : workerEngines) {
        for (const auto& entry : pricers_) {
            engines.emplace(
                entry.first,
                cloneEngineType(entry.second)
            );
        }
    }

    std::vector<std::thread> workers;
    workers.reserve(workerCount);

    // join any started threads even if creating a later thread throws
    struct ThreadJoiner {
        std::vector<std::thread>& threads;

        ~ThreadJoiner() {
            for (auto& thread : threads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        }
    };

    ThreadJoiner joiner{workers};

    for (std::size_t workerId = 0; workerId < workerCount; ++workerId) {
        workers.emplace_back([&, workerId]() {

            try {
                auto& engines = workerEngines[workerId];

                while (true) {

                    const std::size_t index =
                        nextTrade.fetch_add(1);

                    if (index >= trades.size()) {
                        break;
                    }

                    ITrade* trade = trades[index];

                    const auto it = engines.find(
                        trade->getTradeType()
                    );

                    if (it == engines.end()) {
                        safeReceiver.addError(
                            trade->getTradeId(),
                            "No Pricing Engines available for this trade type"
                        );
                        continue;
                    }

                    it->second->price(
                        trade,
                        &safeReceiver
                    );
                }
            }
            catch (...) {
                std::lock_guard<std::mutex> lock(errorMutex);

                if (!workerError) {
                    workerError = std::current_exception();
                }
            }
        });
    }

    // wait for every worker to finish
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    // propagate worker exception only after joining
    if (workerError) {
        std::rethrow_exception(workerError);
    }
}