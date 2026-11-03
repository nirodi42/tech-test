#include "FxTradeLoader.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// parse a line using the UTF-8 for the ¬ delimiter
static std::vector<std::string> splitLine(const std::string& line) {

    const std::string delimiter = "\xC2\xAC";
    std::vector<std::string> fields;

    std::size_t start = 0;
    std::size_t pos;

    while ((pos = line.find(delimiter, start)) != std::string::npos) {
        fields.push_back(line.substr(start, pos - start));
        start = pos + delimiter.size();
    }

    fields.push_back(line.substr(start));
    return fields;
}

// convert string date into a c++ time_point
// FxTrade.h expects std::chrono::system_clock::time_point
static std::chrono::system_clock::time_point parseDate(const std::string& value) {

    std::tm tm = {};
    std::istringstream stream(value);

    stream >> std::get_time(&tm, "%Y-%m-%d");

    if (stream.fail()) {
        throw std::runtime_error("Invalid date: " + value); // TO-DO: also some sanity check for 2112 cases or trade later than value, etc
    }

    tm.tm_isdst = -1;
    std::time_t time = std::mktime(&tm); // TO-DO: timezones confuse this
    
    if (time == static_cast<std::time_t>(-1)) {
        throw std::runtime_error("Date conversion failed: " + value);
    }

    return std::chrono::system_clock::from_time_t(time);
}

std::vector<ITrade*> FxTradeLoader::loadTrades() {

    std::ifstream file(dataFile_);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open FX trade file: " + dataFile_);
    }

    // own objects locally in case of throws
    std::vector<std::unique_ptr<ITrade>> ownedTrades;

    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;

        // skip file header and column names
        if (lineNumber <= 2) {
            continue;
        }

        // remove windows return
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        auto items = splitLine(line);

        // stop at the footer
        if (items[0] == "END") {
            break;
        }

        if (items.size() != 9) {
            throw std::runtime_error("Invalid FX trade at line " + std::to_string(lineNumber));
        }

        auto trade = std::make_unique<FxTrade>(items[8], items[0]);

        // would prob check that all of these are the expected format
        trade->setTradeDate(parseDate(items[1]));
        trade->setInstrument(items[2] + items[3]);
        trade->setNotional(std::stod(items[4]));
        trade->setRate(std::stod(items[5]));
        trade->setValueDate(parseDate(items[6]));
        trade->setCounterparty(items[7]);

        ownedTrades.push_back(std::move(trade));
    }

    // existing interface requires std::vector<ITrade*>
    std::vector<ITrade*> result;
    result.reserve(ownedTrades.size());

    for (auto& trade : ownedTrades) {
        result.push_back(trade.get());
    }

    // transfer ownership to the caller
    for (auto& trade : ownedTrades) {
        trade.release();
    }

    return result;
}

std::string FxTradeLoader::getDataFile() const {
    return dataFile_;
}

void FxTradeLoader::setDataFile(const std::string& file) {
    dataFile_ = file;
}
