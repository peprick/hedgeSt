package com.peprick.stochedge.market;

import com.peprick.stochedge.api.MarketHistoryResponse;
import com.peprick.stochedge.api.MarketPricePoint;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneOffset;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.SplittableRandom;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

@Service
public class MarketDataService {

    private static final int TRADING_DAYS_PER_YEAR = 252;
    private static final Duration MARKET_REQUEST_TIMEOUT = Duration.ofSeconds(4);

    private final HttpClient httpClient;
    private final ObjectMapper objectMapper;
    private final String yahooChartUrl;
    private final String stooqDownloadUrl;
    private final String twelveDataUrl;
    private final String twelveDataApiKey;

    public MarketDataService(
        ObjectMapper objectMapper,
        @Value("${market.data.yahoo-chart-url:https://query1.finance.yahoo.com/v8/finance/chart/}") String yahooChartUrl,
        @Value("${market.data.stooq-download-url:https://stooq.com/q/d/l/}") String stooqDownloadUrl,
        @Value("${market.data.twelve-data-url:https://api.twelvedata.com/time_series}") String twelveDataUrl,
        @Value("${market.data.twelve-data-api-key:demo}") String twelveDataApiKey
    ) {
        this.httpClient = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(3))
            .version(HttpClient.Version.HTTP_1_1)
            .build();
        this.objectMapper = objectMapper;
        this.yahooChartUrl = yahooChartUrl;
        this.stooqDownloadUrl = stooqDownloadUrl;
        this.twelveDataUrl = twelveDataUrl;
        this.twelveDataApiKey = twelveDataApiKey;
    }

    public MarketHistoryResponse getDailyHistory(String symbol, int days) {
        String displaySymbol = normalizeDisplaySymbol(symbol);
        int requestedDays = clamp(days, 30, 2000);

        RawPriceHistory rawHistory = fetchDailyPrices(displaySymbol, requestedDays);
        List<RawPricePoint> rawPoints = rawHistory.points();
        if (rawPoints.size() < 2) {
            throw new MarketDataException("Not enough daily prices found for " + displaySymbol);
        }

        rawPoints.sort(Comparator.comparing(RawPricePoint::date));
        List<RawPricePoint> selected = rawPoints.subList(
            Math.max(0, rawPoints.size() - requestedDays),
            rawPoints.size()
        );

        List<MarketPricePoint> points = withReturns(selected);
        List<Double> returns = points.stream()
            .map(MarketPricePoint::logReturn)
            .filter(value -> value != null)
            .toList();

        double latestClose = points.get(points.size() - 1).close();
        double firstClose = points.get(0).close();
        double totalReturn = latestClose / firstClose - 1.0;
        double averageDailyReturn = returns.stream()
            .mapToDouble(Double::doubleValue)
            .average()
            .orElse(0.0);
        double dailyVolatility = sampleStdDev(returns);
        double annualizedVolatility = dailyVolatility * Math.sqrt(TRADING_DAYS_PER_YEAR);
        double annualizedDrift = averageDailyReturn * TRADING_DAYS_PER_YEAR;
        double minClose = points.stream().mapToDouble(MarketPricePoint::close).min().orElse(latestClose);
        double maxClose = points.stream().mapToDouble(MarketPricePoint::close).max().orElse(latestClose);

        return new MarketHistoryResponse(
            displaySymbol,
            rawHistory.providerSymbol(),
            rawHistory.provider(),
            points.get(0).date(),
            points.get(points.size() - 1).date(),
            points.size(),
            latestClose,
            totalReturn,
            annualizedVolatility,
            annualizedDrift,
            minClose,
            maxClose,
            points
        );
    }

    private RawPriceHistory fetchDailyPrices(String displaySymbol, int requestedDays) {
        List<String> failures = new ArrayList<>();

        try {
            return fetchYahooPrices(displaySymbol, requestedDays);
        } catch (MarketDataException exception) {
            failures.add(exception.getMessage());
        }

        try {
            return fetchStooqPrices(displaySymbol);
        } catch (MarketDataException exception) {
            failures.add(exception.getMessage());
        }

        try {
            return fetchTwelveDataPrices(displaySymbol, requestedDays);
        } catch (MarketDataException exception) {
            failures.add(exception.getMessage());
        }

        return generateDemoFallback(displaySymbol, requestedDays, failures);
    }

    private RawPriceHistory fetchYahooPrices(String displaySymbol, int requestedDays) {
        String providerSymbol = toYahooSymbol(displaySymbol);
        String json = fetchText(
            buildYahooUri(providerSymbol, requestedDays),
            "Yahoo Finance",
            displaySymbol,
            "application/json"
        );
        return new RawPriceHistory(
            providerSymbol,
            "Yahoo Finance chart API",
            parseYahooChart(json, displaySymbol)
        );
    }

    private RawPriceHistory fetchTwelveDataPrices(String displaySymbol, int requestedDays) {
        String json = fetchText(
            buildTwelveDataUri(displaySymbol, requestedDays),
            "Twelve Data",
            displaySymbol,
            "application/json"
        );
        return new RawPriceHistory(
            displaySymbol,
            "Twelve Data daily time series",
            parseTimeSeries(json, displaySymbol)
        );
    }

    private RawPriceHistory fetchStooqPrices(String displaySymbol) {
        String providerSymbol = toStooqSymbol(displaySymbol);
        String csv = fetchText(
            buildStooqUri(providerSymbol),
            "Stooq",
            displaySymbol,
            "text/csv"
        );
        return new RawPriceHistory(
            providerSymbol,
            "Stooq daily CSV",
            parseDailyCsv(csv, displaySymbol)
        );
    }

    private String fetchText(URI uri, String provider, String displaySymbol, String acceptHeader) {
        HttpRequest request = HttpRequest.newBuilder(uri)
            .timeout(MARKET_REQUEST_TIMEOUT)
            .header("Accept", acceptHeader)
            .header("User-Agent", "StochEdge/0.1 learning dashboard")
            .GET()
            .build();

        try {
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            if (response.statusCode() != 200) {
                throw new MarketDataException(provider + " returned HTTP " + response.statusCode());
            }
            return response.body();
        } catch (IOException exception) {
            throw new MarketDataException("Could not fetch daily prices for " + displaySymbol + " from " + provider, exception);
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            throw new MarketDataException("Interrupted while fetching daily prices for " + displaySymbol + " from " + provider, exception);
        }
    }

    private URI buildYahooUri(String providerSymbol, int requestedDays) {
        String encodedSymbol = URLEncoder.encode(providerSymbol, StandardCharsets.UTF_8);
        return URI.create(yahooChartUrl
            + encodedSymbol
            + "?range=" + yahooRange(requestedDays)
            + "&interval=1d&includePrePost=false&events=history");
    }

    private URI buildStooqUri(String providerSymbol) {
        String encodedSymbol = URLEncoder.encode(providerSymbol, StandardCharsets.UTF_8);
        return URI.create(stooqDownloadUrl + "?s=" + encodedSymbol + "&i=d");
    }

    private URI buildTwelveDataUri(String displaySymbol, int requestedDays) {
        String encodedSymbol = URLEncoder.encode(displaySymbol, StandardCharsets.UTF_8);
        String encodedApiKey = URLEncoder.encode(twelveDataApiKey, StandardCharsets.UTF_8);
        return URI.create(twelveDataUrl
            + "?symbol=" + encodedSymbol
            + "&interval=1day"
            + "&outputsize=" + requestedDays
            + "&apikey=" + encodedApiKey);
    }

    private List<RawPricePoint> parseYahooChart(String json, String displaySymbol) {
        JsonNode root;
        try {
            root = objectMapper.readTree(json);
        } catch (IOException exception) {
            throw new MarketDataException("Yahoo Finance returned unreadable JSON for " + displaySymbol, exception);
        }

        JsonNode error = root.path("chart").path("error");
        if (!error.isMissingNode() && !error.isNull()) {
            String message = error.path("description").asText("provider did not return prices");
            throw new MarketDataException("Yahoo Finance rejected " + displaySymbol + ": " + message);
        }

        JsonNode result = root.path("chart").path("result").path(0);
        JsonNode timestamps = result.path("timestamp");
        JsonNode quote = result.path("indicators").path("quote").path(0);
        if (!timestamps.isArray() || quote.isMissingNode()) {
            throw new MarketDataException("Yahoo Finance did not return daily prices for " + displaySymbol);
        }

        List<RawPricePoint> points = new ArrayList<>();
        JsonNode opens = quote.path("open");
        JsonNode highs = quote.path("high");
        JsonNode lows = quote.path("low");
        JsonNode closes = quote.path("close");
        JsonNode volumes = quote.path("volume");

        for (int index = 0; index < timestamps.size(); ++index) {
            JsonNode closeNode = closes.path(index);
            if (closeNode.isMissingNode() || closeNode.isNull()) {
                continue;
            }

            double close = closeNode.asDouble();
            double open = optionalDouble(opens.path(index), close);
            double high = optionalDouble(highs.path(index), Math.max(open, close));
            double low = optionalDouble(lows.path(index), Math.min(open, close));
            long volume = optionalLong(volumes.path(index), 0L);
            LocalDate date = Instant.ofEpochSecond(timestamps.path(index).asLong())
                .atZone(ZoneOffset.UTC)
                .toLocalDate();

            points.add(new RawPricePoint(date, open, high, low, close, volume));
        }

        if (points.isEmpty()) {
            throw new MarketDataException("No parseable Yahoo Finance daily prices found for " + displaySymbol);
        }
        return points;
    }

    private List<RawPricePoint> parseTimeSeries(String json, String displaySymbol) {
        JsonNode root;
        try {
            root = objectMapper.readTree(json);
        } catch (IOException exception) {
            throw new MarketDataException("Market data provider returned unreadable JSON for " + displaySymbol, exception);
        }

        String status = root.path("status").asText("");
        if ("error".equalsIgnoreCase(status)) {
            String message = root.path("message").asText("provider did not return prices");
            throw new MarketDataException("Market data provider rejected " + displaySymbol + ": " + message);
        }

        JsonNode values = root.path("values");
        if (!values.isArray()) {
            throw new MarketDataException("Market data provider did not return a daily price list for " + displaySymbol);
        }

        List<RawPricePoint> points = new ArrayList<>();
        for (JsonNode value : values) {
            try {
                points.add(new RawPricePoint(
                    LocalDate.parse(requiredText(value, "datetime")),
                    requiredDouble(value, "open"),
                    requiredDouble(value, "high"),
                    requiredDouble(value, "low"),
                    requiredDouble(value, "close"),
                    optionalLong(value, "volume")
                ));
            } catch (RuntimeException ignoredBadRow) {
                // Market data files occasionally contain corrected or partial rows; skip rows that cannot be parsed.
            }
        }

        if (points.isEmpty()) {
            throw new MarketDataException("No parseable daily prices found for " + displaySymbol);
        }
        return points;
    }

    private List<RawPricePoint> parseDailyCsv(String csv, String displaySymbol) {
        List<RawPricePoint> points = new ArrayList<>();
        String[] lines = csv.split("\\R");

        for (int index = 1; index < lines.length; ++index) {
            String line = lines[index].trim();
            if (line.isEmpty()) {
                continue;
            }

            String[] cells = line.split(",");
            if (cells.length < 6) {
                continue;
            }

            try {
                points.add(new RawPricePoint(
                    LocalDate.parse(cells[0]),
                    Double.parseDouble(cells[1]),
                    Double.parseDouble(cells[2]),
                    Double.parseDouble(cells[3]),
                    Double.parseDouble(cells[4]),
                    Long.parseLong(cells[5])
                ));
            } catch (RuntimeException ignoredBadRow) {
                // Skip malformed fallback rows; the dashboard only needs clean daily candles.
            }
        }

        if (points.isEmpty()) {
            throw new MarketDataException("No parseable CSV daily prices found for " + displaySymbol);
        }
        return points;
    }

    private RawPriceHistory generateDemoFallback(String displaySymbol, int requestedDays, List<String> failures) {
        SplittableRandom random = new SplittableRandom(displaySymbol.hashCode());
        double startPrice = 35.0 + Math.floorMod(displaySymbol.hashCode(), 24_000) / 100.0;
        double annualDrift = 0.03 + random.nextDouble(-0.04, 0.10);
        double annualVolatility = 0.18 + random.nextDouble(0.00, 0.24);
        double dailyDrift = (annualDrift - 0.5 * annualVolatility * annualVolatility) / TRADING_DAYS_PER_YEAR;
        double dailyVolatility = annualVolatility / Math.sqrt(TRADING_DAYS_PER_YEAR);
        LocalDate date = lastBusinessDay(LocalDate.now().minusDays(1));

        List<RawPricePoint> reversed = new ArrayList<>(requestedDays);
        double close = startPrice;
        for (int count = 0; count < requestedDays; ++count) {
            date = previousBusinessDay(date);
        }

        for (int count = 0; count < requestedDays; ++count) {
            date = nextBusinessDay(date);
            double shock = random.nextGaussian();
            double nextClose = Math.max(1.0, close * Math.exp(dailyDrift + dailyVolatility * shock));
            double open = close * (1.0 + random.nextDouble(-0.008, 0.008));
            double high = Math.max(open, nextClose) * (1.0 + random.nextDouble(0.001, 0.018));
            double low = Math.min(open, nextClose) * (1.0 - random.nextDouble(0.001, 0.018));
            long volume = 1_000_000L + Math.floorMod(random.nextLong(), 90_000_000L);

            reversed.add(new RawPricePoint(date, open, high, low, nextClose, volume));
            close = nextClose;
        }

        String failureSummary = failures.isEmpty() ? "" : " after live providers failed";
        return new RawPriceHistory(
            displaySymbol + " generated",
            "Offline stochastic demo fallback" + failureSummary,
            reversed
        );
    }

    private List<MarketPricePoint> withReturns(List<RawPricePoint> rawPoints) {
        List<MarketPricePoint> points = new ArrayList<>(rawPoints.size());
        Double previousClose = null;

        for (RawPricePoint rawPoint : rawPoints) {
            Double logReturn = previousClose == null
                ? null
                : Math.log(rawPoint.close() / previousClose);
            points.add(new MarketPricePoint(
                rawPoint.date(),
                rawPoint.open(),
                rawPoint.high(),
                rawPoint.low(),
                rawPoint.close(),
                rawPoint.volume(),
                logReturn
            ));
            previousClose = rawPoint.close();
        }

        return points;
    }

    private double sampleStdDev(List<Double> values) {
        if (values.size() < 2) {
            return 0.0;
        }

        double average = values.stream()
            .mapToDouble(Double::doubleValue)
            .average()
            .orElse(0.0);
        double squaredDistanceSum = values.stream()
            .mapToDouble(value -> {
                double distance = value - average;
                return distance * distance;
            })
            .sum();

        return Math.sqrt(squaredDistanceSum / (values.size() - 1));
    }

    private String normalizeDisplaySymbol(String symbol) {
        String normalized = symbol == null ? "" : symbol.trim().toUpperCase(Locale.ROOT);
        if (!normalized.matches("[A-Z0-9.\\-]{1,20}")) {
            throw new MarketDataException("Symbol must use letters, numbers, dots, or hyphens");
        }
        return normalized;
    }

    private int clamp(int value, int minimum, int maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    private String yahooRange(int requestedDays) {
        if (requestedDays <= 126) {
            return "6mo";
        }
        if (requestedDays <= 252) {
            return "1y";
        }
        if (requestedDays <= 504) {
            return "2y";
        }
        return "5y";
    }

    private String toYahooSymbol(String displaySymbol) {
        return displaySymbol.replace('.', '-');
    }

    private String toStooqSymbol(String displaySymbol) {
        String lower = displaySymbol.toLowerCase(Locale.ROOT);
        if (lower.contains(".")) {
            return lower;
        }
        return lower + ".us";
    }

    private String requiredText(JsonNode node, String fieldName) {
        String value = node.path(fieldName).asText("");
        if (value.isBlank()) {
            throw new IllegalArgumentException("Missing " + fieldName);
        }
        return value;
    }

    private double requiredDouble(JsonNode node, String fieldName) {
        return Double.parseDouble(requiredText(node, fieldName));
    }

    private long optionalLong(JsonNode node, String fieldName) {
        String value = node.path(fieldName).asText("0");
        if (value.isBlank()) {
            return 0L;
        }
        return Math.round(Double.parseDouble(value));
    }

    private double optionalDouble(JsonNode node, double fallback) {
        if (node.isMissingNode() || node.isNull()) {
            return fallback;
        }
        return node.asDouble(fallback);
    }

    private long optionalLong(JsonNode node, long fallback) {
        if (node.isMissingNode() || node.isNull()) {
            return fallback;
        }
        return node.asLong(fallback);
    }

    private LocalDate previousBusinessDay(LocalDate date) {
        LocalDate cursor = date.minusDays(1);
        while (isWeekend(cursor)) {
            cursor = cursor.minusDays(1);
        }
        return cursor;
    }

    private LocalDate nextBusinessDay(LocalDate date) {
        LocalDate cursor = date.plusDays(1);
        while (isWeekend(cursor)) {
            cursor = cursor.plusDays(1);
        }
        return cursor;
    }

    private LocalDate lastBusinessDay(LocalDate date) {
        LocalDate cursor = date;
        while (isWeekend(cursor)) {
            cursor = cursor.minusDays(1);
        }
        return cursor;
    }

    private boolean isWeekend(LocalDate date) {
        int day = date.getDayOfWeek().getValue();
        return day == 6 || day == 7;
    }

    private record RawPriceHistory(
        String providerSymbol,
        String provider,
        List<RawPricePoint> points
    ) {
    }

    private record RawPricePoint(
        LocalDate date,
        double open,
        double high,
        double low,
        double close,
        long volume
    ) {
    }
}
