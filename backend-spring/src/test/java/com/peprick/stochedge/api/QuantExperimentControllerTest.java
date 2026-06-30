package com.peprick.stochedge.api;

import static org.assertj.core.api.Assertions.assertThat;

import java.util.Map;
import org.junit.jupiter.api.Test;

class QuantExperimentControllerTest {

    @Test
    void healthReportsBackendUp() {
        QuantExperimentController controller = new QuantExperimentController(null, null);

        Map<String, String> health = controller.health();

        assertThat(health)
            .containsEntry("status", "UP")
            .containsEntry("service", "stochedge-backend");
    }
}
