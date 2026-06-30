package com.peprick.stochedge.api;

import com.peprick.stochedge.engine.QuantEngineService;
import com.peprick.stochedge.market.MarketDataService;
import java.util.Map;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api")
public class QuantExperimentController {

    private final QuantEngineService quantEngineService;
    private final MarketDataService marketDataService;

    public QuantExperimentController(
        QuantEngineService quantEngineService,
        MarketDataService marketDataService
    ) {
        this.quantEngineService = quantEngineService;
        this.marketDataService = marketDataService;
    }

    @GetMapping("/health")
    public Map<String, String> health() {
        return Map.of(
            "status", "UP",
            "service", "stochedge-backend"
        );
    }

    @PostMapping("/experiments/run")
    public EngineRunResponse runExperiment() {
        return quantEngineService.runExperiment();
    }

    @GetMapping("/market/history")
    public MarketHistoryResponse marketHistory(
        @RequestParam(defaultValue = "AAPL") String symbol,
        @RequestParam(defaultValue = "252") int days
    ) {
        return marketDataService.getDailyHistory(symbol, days);
    }
}
