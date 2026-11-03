#include "BondTradeLoader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <iomanip>
#include <chrono>

// BondTrade* BondTradeLoader::createTradeFromLine(std::string line) {
// pass by const reference instead of copying, for large objects so not really necessary but yeh
BondTrade* BondTradeLoader::createTradeFromLine(const std::string& line) {
    std::vector<std::string> items;
    std::stringstream ss(line);
    std::string item;
    
    while (std::getline(ss, item, separator)) {
        items.push_back(item);
    }
    
    // remove a \r at the end of the line if it's windows-style \r\n
    if (!items.empty() &&
        !items.back().empty() &&
        items.back().back() == '\r') {
        items.back().pop_back();
    }
    
    if (items.size() < 7) {
        throw std::runtime_error("Invalid line format");
    }
    
//     BondTrade* trade = new BondTrade(items[6]);
    // provide the trade type to the constructor expecting (id, type)
    // could be unique pointer instead, to not end up with leaks, but requires changes across the project
    // TO-DO: final one is Supra (not Gov or Corp) -- check what should happen with these
    BondTrade* trade = new BondTrade(items[6], items[0]);
    
    std::tm tm = {};
    std::istringstream dateStream(items[1]);
    dateStream >> std::get_time(&tm, "%Y-%m-%d");
    auto timePoint = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    trade->setTradeDate(timePoint);
    
    trade->setInstrument(items[2]);
    trade->setCounterparty(items[3]);
    trade->setNotional(std::stod(items[4]));
    trade->setRate(std::stod(items[5]));
    
    return trade;
}

void BondTradeLoader::loadTradesFromFile(std::string filename, BondTradeList& tradeList) {
    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be null");
    }
    
    std::ifstream stream(filename);
    if (!stream.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    int lineCount = 0;
    std::string line;
    while (std::getline(stream, line)) {
        if (lineCount == 0) {
        } else {
            tradeList.add(createTradeFromLine(line));
        }
        lineCount++;
    }
}

std::vector<ITrade*> BondTradeLoader::loadTrades() {
    BondTradeList tradeList;
    loadTradesFromFile(dataFile_, tradeList);
    
    std::vector<ITrade*> result;
    for (size_t i = 0; i < tradeList.size(); ++i) {
        result.push_back(tradeList[i]);
    }
    return result;
}

std::string BondTradeLoader::getDataFile() const {
    return dataFile_;
}

void BondTradeLoader::setDataFile(const std::string& file) {
    dataFile_ = file;
}

void BondTradeLoader::streamTrades(const std::function<void(ITrade*)>& processTrade) {

    std::ifstream file(dataFile_);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + dataFile_);
    }

    std::string line;

    // skip header
    if (!std::getline(file, line)) {
        return;
    }

    while (std::getline(file, line)) {

        if (line.empty()) {
            continue;
        }

        std::unique_ptr<BondTrade> trade(createTradeFromLine(line)); //trade by trade

        processTrade(trade.get()); //non-owning
    }
}