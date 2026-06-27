package com.peprick.hedgest.api;

import com.peprick.hedgest.engine.QuantEngineService;
import java.util.Map;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api")
public class QuantExperimentController {

    private final QuantEngineService quantEngineService;

    public QuantExperimentController(QuantEngineService quantEngineService) {
        this.quantEngineService = quantEngineService;
    }

    @GetMapping("/health")
    public Map<String, String> health() {
        return Map.of(
            "status", "UP",
            "service", "hedgest-backend"
        );
    }

    @PostMapping("/experiments/run")
    public EngineRunResponse runExperiment() {
        return quantEngineService.runExperiment();
    }
}
