package com.peprick.hedgest.api;

import com.peprick.hedgest.engine.EngineExecutionException;
import java.time.Instant;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

@RestControllerAdvice
public class GlobalExceptionHandler {

    @ExceptionHandler(EngineExecutionException.class)
    public ResponseEntity<ApiError> handleEngineExecution(EngineExecutionException exception) {
        return ResponseEntity.status(HttpStatus.BAD_GATEWAY)
            .body(new ApiError(
                Instant.now(),
                HttpStatus.BAD_GATEWAY.value(),
                "Quant engine execution failed",
                exception.getMessage()
            ));
    }
}
