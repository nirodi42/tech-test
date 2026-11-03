#include "PricingConfigLoader.h"
#include <stdexcept>
#include <fstream>
#include <sstream>

std::string PricingConfigLoader::getConfigFile() const {
    return configFile_;
}

void PricingConfigLoader::setConfigFile(const std::string& file) {

    configFile_ = file;
}

PricingEngineConfig PricingConfigLoader::loadConfig() {
    std::ifstream file(configFile_);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open pricing configuration file: " + configFile_
        );
    }

    // small file so far
    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        throw std::runtime_error(
            "Error reading pricing configuration file: " + configFile_
        );
    }

    return parseXml(buffer.str());
}

PricingEngineConfig PricingConfigLoader::parseXml(const std::string& content) {

    // could use proper xml parser for arbitrary file but fine for now
    // also validate attr values

    PricingEngineConfig config;

    std::size_t pos = 0;

    while ((pos = content.find("<Engine ", pos))
           != std::string::npos) {

        std::size_t end = content.find("/>", pos);

        if (end == std::string::npos) {
            throw std::runtime_error(
                "Invalid pricing engine configuration"
            );
        }

        std::string element = content.substr(
            pos, end - pos
        );

        auto getAttribute = [&](const std::string& name) {
            std::string key = name + "=\"";

            std::size_t start = element.find(key);

            if (start == std::string::npos) {
                throw std::runtime_error(
                    "Missing XML attribute: " + name
                );
            }

            start += key.size();

            std::size_t finish = element.find('"', start);

            if (finish == std::string::npos) {
                throw std::runtime_error(
                    "Invalid XML attribute: " + name
                );
            }

            return element.substr(start, finish - start);
        };

        PricingEngineConfigItem item;

        // pricingEngine in xml, not typeName
        item.setTradeType(getAttribute("tradeType"));
        item.setAssembly(getAttribute("assembly"));
        item.setTypeName(getAttribute("pricingEngine"));

        config.push_back(item);

        pos = end + 2;
    }

    return config;
}