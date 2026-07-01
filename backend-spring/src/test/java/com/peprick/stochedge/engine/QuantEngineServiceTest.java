package com.peprick.stochedge.engine;

import static org.assertj.core.api.Assertions.assertThat;

import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class QuantEngineServiceTest {

    @TempDir
    Path tempDir;

    @Test
    void resolvesConfiguredExecutableWhenItExists() throws Exception {
        Path executable = tempDir.resolve("stochedge_engine");
        Files.createFile(executable);

        Path resolvedExecutable = QuantEngineService.resolveEngineExecutable(executable);

        assertThat(resolvedExecutable).isEqualTo(executable);
    }

    @Test
    void keepsConfiguredPathWhenNoCandidateExists() {
        Path executable = tempDir.resolve("missing_engine");

        Path resolvedExecutable = QuantEngineService.resolveEngineExecutable(executable);

        assertThat(resolvedExecutable).isEqualTo(executable);
    }

    @Test
    void resolvesWindowsExecutableWhenExtensionlessPathIsConfigured() throws Exception {
        Path executable = tempDir.resolve("stochedge_engine");
        Path windowsExecutable = tempDir.resolve("stochedge_engine.exe");
        Files.createFile(windowsExecutable);

        Path resolvedExecutable = QuantEngineService.resolveEngineExecutable(executable, true);

        assertThat(resolvedExecutable).isEqualTo(windowsExecutable);
    }

    @Test
    void resolvesUnixExecutableWhenWindowsPathIsConfigured() throws Exception {
        Path executable = tempDir.resolve("stochedge_engine");
        Path windowsExecutable = tempDir.resolve("stochedge_engine.exe");
        Files.createFile(executable);

        Path resolvedExecutable = QuantEngineService.resolveEngineExecutable(windowsExecutable, false);

        assertThat(resolvedExecutable).isEqualTo(executable);
    }
}
