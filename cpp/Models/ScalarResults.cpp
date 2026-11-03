#include "ScalarResults.h"
#include <stdexcept>

ScalarResults::~ScalarResults() = default;

std::optional<ScalarResult> ScalarResults::operator[](const std::string& tradeId) const {
    if (!containsTrade(tradeId)) {
        return std::nullopt;
    }

    std::optional<double> priceResult = std::nullopt;
    std::optional<std::string> error = std::nullopt;

    auto resultIt = results_.find(tradeId);
    if (resultIt != results_.end()) {
        priceResult = resultIt->second;
    }

    auto errorIt = errors_.find(tradeId);
    if (errorIt != errors_.end()) {
        error = errorIt->second;
    }

    return ScalarResult(tradeId, priceResult, error);
}

bool ScalarResults::containsTrade(const std::string& tradeId) const {
    return results_.find(tradeId) != results_.end() || errors_.find(tradeId) != errors_.end();
}

void ScalarResults::addResult(const std::string& tradeId, double result) {
    results_[tradeId] = result;
}

void ScalarResults::addError(const std::string& tradeId, const std::string& error) {
    errors_[tradeId] = error;
}

ScalarResults::Iterator& ScalarResults::Iterator::operator++() {

    if (parent_ == nullptr) {
        throw std::runtime_error("Invalid iterator");
    }

    const auto resultEnd = parent_->results_.cend();
    const auto errorEnd = parent_->errors_.cend();

    if (resultIt_ == resultEnd && errorIt_ == errorEnd) {
        throw std::out_of_range("Cannot increment end iterator");
    }

    if (resultIt_ == resultEnd) {
        ++errorIt_;
    }
    else if (errorIt_ == errorEnd) {
        ++resultIt_;
    }
    else if (resultIt_->first < errorIt_->first) {
        ++resultIt_;
    }
    else if (errorIt_->first < resultIt_->first) {
        ++errorIt_;
    }
    else {
        ++resultIt_;
        ++errorIt_;
    }

    return *this;
}

ScalarResult ScalarResults::Iterator::operator*() const {

    if (parent_ == nullptr) {
        throw std::runtime_error("Invalid iterator");
    }

    const auto resultEnd = parent_->results_.cend();
    const auto errorEnd = parent_->errors_.cend();

    if (resultIt_ == resultEnd && errorIt_ == errorEnd) {
        throw std::out_of_range("Cannot dereference end iterator");
    }

    std::string tradeId;

    if (resultIt_ == resultEnd) {
        tradeId = errorIt_->first;
    }
    else if (errorIt_ == errorEnd) {
        tradeId = resultIt_->first;
    }
    else {
        tradeId = std::min(resultIt_->first, errorIt_->first);
    }

    return parent_->operator[](tradeId).value();
}

bool ScalarResults::Iterator::operator!=(const Iterator& other) const {
    return parent_ != other.parent_ || resultIt_ != other.resultIt_ || errorIt_ != other.errorIt_;
}

ScalarResults::Iterator ScalarResults::begin() const {
    return Iterator(this, false);
}

ScalarResults::Iterator ScalarResults::end() const {
    return Iterator(this, true);
}

ScalarResults::Iterator::Iterator(const ScalarResults* parent, bool atEnd) : parent_(parent) {
    if (atEnd) {
        resultIt_ = parent_->results_.cend();
        errorIt_ = parent_->errors_.cend();
    } else {
        resultIt_ = parent_->results_.cbegin();
        errorIt_ = parent_->errors_.cbegin();
    }
}