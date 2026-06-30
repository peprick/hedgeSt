package com.peprick.stochedge.api;

import java.time.LocalDate;

public record MarketPricePoint(
    LocalDate date,
    double open,
    double high,
    double low,
    double close,
    long volume,
    Double logReturn
) {
}
