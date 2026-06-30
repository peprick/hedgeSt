package com.peprick.stochedge.api;

import java.time.LocalDate;
import java.util.List;

public record MarketHistoryResponse(
    String symbol,
    String providerSymbol,
    String provider,
    LocalDate startDate,
    LocalDate endDate,
    int observations,
    double latestClose,
    double totalReturn,
    double annualizedVolatility,
    double annualizedDrift,
    double minClose,
    double maxClose,
    List<MarketPricePoint> points
) {
}
