package com.peprick.stochedge.engine;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.peprick.stochedge.api.EngineRunResponse;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

@Service
public class QuantEngineService {

    private final ObjectMapper objectMapper;
    private final Path engineExecutable;
    private final List<String> engineArguments;
    private final Duration timeout;

    public QuantEngineService(
        ObjectMapper objectMapper,
        @Value("${quant.engine.executable}") String engineExecutable,
        @Value("${quant.engine.arguments:--json}") String engineArguments,
        @Value("${quant.engine.timeout-seconds}") long timeoutSeconds
    ) {
        this.objectMapper = objectMapper;
        this.engineExecutable = Path.of(engineExecutable).toAbsolutePath().normalize();
        this.engineArguments = Arrays.stream(engineArguments.split("\\s+"))
            .filter(argument -> !argument.isBlank())
            .toList();
        this.timeout = Duration.ofSeconds(timeoutSeconds);
    }

    public EngineRunResponse runExperiment() {
        if (!engineExecutable.toFile().isFile()) {
            throw new EngineExecutionException("C++ engine executable was not found at " + engineExecutable);
        }

        List<String> command = new ArrayList<>();
        command.add(engineExecutable.toString());
        command.addAll(engineArguments);

        ProcessBuilder processBuilder = new ProcessBuilder(command);
        processBuilder.directory(engineExecutable.getParent().toFile());
        processBuilder.redirectErrorStream(true);

        try {
            Process process = processBuilder.start();
            CompletableFuture<String> outputFuture = CompletableFuture.supplyAsync(() -> readProcessOutput(process));
            boolean finished = process.waitFor(timeout.toSeconds(), TimeUnit.SECONDS);

            if (!finished) {
                process.destroyForcibly();
                outputFuture.cancel(true);
                throw new EngineExecutionException("C++ engine timed out after " + timeout.toSeconds() + " seconds");
            }

            String output = outputFuture.get();
            if (process.exitValue() != 0) {
                throw new EngineExecutionException("C++ engine exited with code " + process.exitValue() + ": " + output);
            }

            JsonNode result = objectMapper.readTree(output);
            return new EngineRunResponse(
                Instant.now(),
                engineExecutable.toString(),
                result
            );
        } catch (IOException exception) {
            throw new EngineExecutionException("Could not start or parse C++ engine output", exception);
        } catch (ExecutionException exception) {
            throw new EngineExecutionException("Could not read C++ engine output", exception);
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            throw new EngineExecutionException("Interrupted while waiting for C++ engine", exception);
        }
    }

    private static String readProcessOutput(Process process) {
        try {
            return new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
        } catch (IOException exception) {
            throw new IllegalStateException("Could not read C++ engine output", exception);
        }
    }
}
