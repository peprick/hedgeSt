package com.peprick.stochedge.api;

import com.fasterxml.jackson.databind.JsonNode;
import java.time.Instant;

public record EngineRunResponse(
    Instant generatedAt,
    String engineExecutable,
    JsonNode result
) {
}
