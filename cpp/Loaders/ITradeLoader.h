#ifndef ITRADELOADER_H
#define ITRADELOADER_H

#include "../Models/ITrade.h"
#include <vector>
#include <string>
#include <functional>

class ITradeLoader {
public:
    virtual ~ITradeLoader() = default;
    virtual std::vector<ITrade*> loadTrades() = 0; // all trades in memory
    virtual std::string getDataFile() const = 0;
    virtual void setDataFile(const std::string& file) = 0;
    virtual void streamTrades(const std::function<void(ITrade*)>& processTrade) = 0;
};

#endif // ITRADELOADER_H
