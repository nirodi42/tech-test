#include "ScreenResultPrinter.h"
#include <iostream>

void ScreenResultPrinter::printResults(ScalarResults& results) {
    for (const auto& result : results) {
        // Write code here to print out the results such that we have:
        // TradeID : Result : Error
        // If there is no result then the output should be:
        // TradeID : Error
        // If there is no error the output should be:
        // TradeID : Result
        
        std::cout << result.getTradeId();

        // not call getResult etc twice
        const auto price = result.getResult();
        const auto error = result.getError();

        if (price) {
            std::cout << " : " << *price;
        }

        if (error) {
            std::cout << " : " << *error;
        }
        std::cout << '\n';
    }
}