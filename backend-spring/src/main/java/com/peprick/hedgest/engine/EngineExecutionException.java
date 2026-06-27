package com.peprick.hedgest.engine;

public class EngineExecutionException extends RuntimeException {

    public EngineExecutionException(String message) {
        super(message);
    }

    public EngineExecutionException(String message, Throwable cause) {
        super(message, cause);
    }
}
