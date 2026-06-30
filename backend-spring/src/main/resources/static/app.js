const state = {
    response: null,
    marketHistory: null,
    sortMode: "cvar",
    activeTab: "overview"
};

const nodes = {
    healthPill: document.getElementById("healthPill"),
    healthText: document.getElementById("healthText"),
    refreshHealthButton: document.getElementById("refreshHealthButton"),
    runButton: document.getElementById("runButton"),
    runSummary: document.getElementById("runSummary"),
    errorPanel: document.getElementById("errorPanel"),
    emptyState: document.getElementById("emptyState"),
    pricingCards: document.getElementById("pricingCards"),
    marketAssumptions: document.getElementById("marketAssumptions"),
    pricingComparison: document.getElementById("pricingComparison"),
    leaderboard: document.getElementById("leaderboard"),
    strategyBars: document.getElementById("strategyBars"),
    strategyTable: document.getElementById("strategyTable"),
    learningFacts: document.getElementById("learningFacts"),
    agentCompare: document.getElementById("agentCompare"),
    onePathCards: document.getElementById("onePathCards"),
    marketForm: document.getElementById("marketForm"),
    marketSymbolInput: document.getElementById("marketSymbolInput"),
    marketDaysInput: document.getElementById("marketDaysInput"),
    loadMarketButton: document.getElementById("loadMarketButton"),
    marketCards: document.getElementById("marketCards"),
    marketChart: document.getElementById("marketChart"),
    marketChartTitle: document.getElementById("marketChartTitle"),
    marketRealFacts: document.getElementById("marketRealFacts"),
    marketRecentTable: document.getElementById("marketRecentTable"),
    jsonView: document.getElementById("jsonView"),
    copyJsonButton: document.getElementById("copyJsonButton")
};

function boot() {
    bindEvents();
    refreshHealth();
    activateTab("overview");
    refreshIcons();
}

function bindEvents() {
    nodes.refreshHealthButton.addEventListener("click", refreshHealth);
    nodes.runButton.addEventListener("click", runExperiment);
    nodes.marketForm.addEventListener("submit", (event) => {
        event.preventDefault();
        loadMarketHistory();
    });
    nodes.copyJsonButton.addEventListener("click", copyJson);

    document.querySelectorAll("[data-tab]").forEach((button) => {
        button.addEventListener("click", () => activateTab(button.dataset.tab));
    });

    document.querySelectorAll("[data-sort]").forEach((button) => {
        button.addEventListener("click", () => {
            state.sortMode = button.dataset.sort;
            document.querySelectorAll("[data-sort]").forEach((item) => {
                item.classList.toggle("is-active", item === button);
            });
            if (state.response) {
                renderStrategies(state.response.result.strategies || []);
            }
        });
    });
}

async function refreshHealth() {
    setHealth("Checking backend", "");

    try {
        const response = await fetch("/api/health");
        if (!response.ok) {
            throw new Error(`Health check failed with HTTP ${response.status}`);
        }
        const body = await response.json();
        setHealth(`${body.service || "Backend"} ${body.status || "UP"}`, "up");
    } catch (error) {
        setHealth("Backend unavailable", "down");
    }
}

async function runExperiment() {
    clearError();
    setLoading(true);

    try {
        const response = await fetch("/api/experiments/run", {
            method: "POST"
        });
        const body = await response.json();

        if (!response.ok) {
            throw new Error(body.message || `Experiment failed with HTTP ${response.status}`);
        }

        state.response = body;
        renderDashboard(body);
        nodes.emptyState.classList.add("is-hidden");
        nodes.runSummary.textContent = `Last run: ${formatDateTime(body.generatedAt)} from ${body.result.project || "C++ engine"}.`;
    } catch (error) {
        showError(error.message || "Could not run experiment.");
    } finally {
        setLoading(false);
        refreshIcons();
    }
}

async function loadMarketHistory() {
    clearError();
    setMarketLoading(true);

    const symbol = nodes.marketSymbolInput.value.trim() || "AAPL";
    const days = nodes.marketDaysInput.value || "252";

    try {
        const response = await fetch(`/api/market/history?symbol=${encodeURIComponent(symbol)}&days=${encodeURIComponent(days)}`);
        const body = await response.json();

        if (!response.ok) {
            throw new Error(body.message || `Market data failed with HTTP ${response.status}`);
        }

        state.marketHistory = body;
        renderMarketHistory(body);
    } catch (error) {
        showError(error.message || "Could not load market history.");
    } finally {
        setMarketLoading(false);
        refreshIcons();
    }
}

function renderDashboard(response) {
    const result = response.result || {};
    const pricing = result.pricing || {};
    const blackScholes = pricing.blackScholes || {};
    const monteCarlo = pricing.monteCarlo || {};
    const market = result.market || {};
    const strategies = result.strategies || [];

    renderMetricCards(nodes.pricingCards, [
        {
            label: "Black-Scholes Call",
            value: money(blackScholes.callPrice),
            note: "Closed-form formula price"
        },
        {
            label: "Monte Carlo Call",
            value: money(monteCarlo.discountedOptionPrice),
            note: `${formatNumber(monteCarlo.paths, 0)} random paths`
        },
        {
            label: "Formula Delta",
            value: formatNumber(blackScholes.callDelta, 4),
            note: "Stock units per option"
        },
        {
            label: "MC - Formula",
            value: signedMoney(pricing.monteCarloMinusFormula),
            note: pricing.withinTwoStandardErrors ? "Inside 2 standard errors" : "Outside 2 standard errors"
        }
    ]);

    renderAssumptions(market);
    renderPricingComparison(blackScholes, monteCarlo);
    renderLeaderboard(strategies);
    renderStrategies(strategies);
    renderLearning(result);
    renderOnePath(result.onePathDeltaHedge || {});
    nodes.jsonView.textContent = JSON.stringify(response, null, 2);
}

function renderMetricCards(target, cards) {
    target.replaceChildren();
    cards.forEach((card) => {
        const article = createElement("article", "metric-card");
        article.append(
            textElement("p", "metric-label", card.label),
            textElement("p", "metric-value", card.value),
            textElement("p", "metric-note", card.note)
        );
        target.append(article);
    });
}

function renderAssumptions(market) {
    const items = [
        ["Initial Stock", money(market.initialStockPrice)],
        ["Strike", money(market.strike)],
        ["Risk-Free Rate", percent(market.riskFreeRate)],
        ["Volatility", percent(market.volatility)],
        ["Maturity", `${formatNumber(market.maturityYears, 2)} year`],
        ["Trading Steps", formatNumber(market.steps, 0)],
        ["Transaction Cost", percent(market.transactionCostRate)],
        ["Model", "GBM"]
    ];

    nodes.marketAssumptions.replaceChildren();
    items.forEach(([label, value]) => {
        const item = createElement("div", "assumption-item");
        item.append(textElement("span", "", label), textElement("strong", "", value));
        nodes.marketAssumptions.append(item);
    });
}

function renderPricingComparison(blackScholes, monteCarlo) {
    const values = [
        ["Black-Scholes", blackScholes.callPrice, "var(--blue)"],
        ["Monte Carlo", monteCarlo.discountedOptionPrice, "var(--teal)"],
        ["Std Error", monteCarlo.discountedStandardError, "var(--amber)"]
    ];
    const max = Math.max(...values.map(([, value]) => safeAbs(value)), 1);

    nodes.pricingComparison.replaceChildren();
    values.forEach(([label, value, color]) => {
        nodes.pricingComparison.append(createBarRow(label, money(value), safeAbs(value) / max, color));
    });
}

function renderLeaderboard(strategies) {
    nodes.leaderboard.replaceChildren();
    const sorted = [...strategies].sort((a, b) => a.conditionalValueAtRisk95 - b.conditionalValueAtRisk95).slice(0, 3);

    sorted.forEach((strategy, index) => {
        const card = createElement("div", "leader-card");
        card.append(
            textElement("span", "leader-rank", `${index + 1}`),
            textElement("h3", "", strategy.label),
            textElement("p", "", `CVaR ${money(strategy.conditionalValueAtRisk95)} | Loss ${percent(strategy.lossProbability)}`)
        );
        nodes.leaderboard.append(card);
    });
}

function renderStrategies(strategies) {
    const sorted = sortStrategies(strategies);
    const maxCvar = Math.max(...sorted.map((item) => safeAbs(item.conditionalValueAtRisk95)), 1);
    const maxCost = Math.max(...sorted.map((item) => safeAbs(item.averageTransactionCosts)), 1);
    const maxPnl = Math.max(...sorted.map((item) => safeAbs(item.averagePnl)), 1);
    const bestCvar = Math.min(...sorted.map((item) => item.conditionalValueAtRisk95));

    nodes.strategyBars.replaceChildren();
    sorted.forEach((strategy) => {
        const card = createElement("div", "strategy-bar-card");
        card.append(textElement("div", "strategy-name", strategy.label));

        const bars = createElement("div", "strategy-mini-bars");
        bars.append(
            createBarRow("CVaR 95", money(strategy.conditionalValueAtRisk95), safeAbs(strategy.conditionalValueAtRisk95) / maxCvar, "var(--risk)"),
            createBarRow("Cost", money(strategy.averageTransactionCosts), safeAbs(strategy.averageTransactionCosts) / maxCost, "var(--cost)"),
            createBarRow("Avg P&L", signedMoney(strategy.averagePnl), safeAbs(strategy.averagePnl) / maxPnl, "var(--pnl)")
        );
        card.append(bars);
        nodes.strategyBars.append(card);
    });

    nodes.strategyTable.replaceChildren();
    sorted.forEach((strategy) => {
        const row = document.createElement("tr");
        row.classList.toggle("best-row", strategy.conditionalValueAtRisk95 === bestCvar);

        [
            strategy.label,
            signedMoney(strategy.averagePnl),
            money(strategy.pnlStdDev),
            money(strategy.valueAtRisk95),
            money(strategy.conditionalValueAtRisk95),
            percent(strategy.lossProbability),
            money(strategy.averageTransactionCosts),
            formatNumber(strategy.averageRebalances, 1)
        ].forEach((value) => row.append(textElement("td", "", value)));

        nodes.strategyTable.append(row);
    });
}

function renderLearning(result) {
    const learned = result.learnedHedger || {};
    const evaluation = learned.evaluation || {};
    const environment = result.rlEnvironment || {};
    const daily = (result.strategies || []).find((item) => item.label === "Daily rebalance") || {};

    const facts = [
        ["Algorithm", learned.algorithm],
        ["Training Episodes", formatNumber(learned.trainingEpisodes, 0)],
        ["Action Menu", learned.actionMenu],
        ["Evaluation Choices", formatActionCounts(learned.evaluationActionCounts)],
        ["State", environment.state],
        ["Action", environment.action || learned.actionMenu],
        ["Step Reward", environment.stepReward],
        ["Final Penalty", environment.finalRewardPenalty]
    ];

    nodes.learningFacts.replaceChildren();
    facts.forEach(([label, value]) => {
        const item = createElement("div", "fact-item");
        item.append(textElement("span", "", label), textElement("strong", "", value || "N/A"));
        nodes.learningFacts.append(item);
    });

    nodes.agentCompare.replaceChildren();
    nodes.agentCompare.append(
        createCompareCard("Daily Delta", daily),
        createCompareCard("Q-learning Hedger", evaluation)
    );
}

function renderOnePath(onePath) {
    renderMetricCards(nodes.onePathCards, [
        {
            label: "Premium",
            value: money(onePath.optionPremium),
            note: "Cash received at start"
        },
        {
            label: "Initial Delta",
            value: formatNumber(onePath.initialDelta, 4),
            note: "Starting hedge"
        },
        {
            label: "Final Stock",
            value: money(onePath.finalStockPrice),
            note: "Path endpoint"
        },
        {
            label: "Transaction Costs",
            value: money(onePath.totalTransactionCosts),
            note: `${formatNumber(onePath.rebalances, 0)} rebalances`
        },
        {
            label: "Hedging P&L",
            value: signedMoney(onePath.hedgingPnl),
            note: "After payoff and costs"
        }
    ]);
}

function renderMarketHistory(history) {
    const points = history.points || [];
    const latest = points[points.length - 1] || {};

    nodes.marketChartTitle.textContent = `${history.symbol} Close Price History`;
    renderMetricCards(nodes.marketCards, [
        {
            label: "Latest Close",
            value: money(history.latestClose),
            note: `${history.endDate || "latest"} close`
        },
        {
            label: "Total Return",
            value: signedPercent(history.totalReturn),
            note: `${history.observations || points.length} daily closes`
        },
        {
            label: "Realized Volatility",
            value: percent(history.annualizedVolatility),
            note: "Annualized from daily returns"
        },
        {
            label: "Annualized Drift",
            value: signedPercent(history.annualizedDrift),
            note: "Average daily log return x 252"
        }
    ]);

    renderMarketFacts(history);
    renderMarketChart(points);
    renderRecentMarketRows(points);

    if (latest.date) {
        nodes.marketSymbolInput.value = history.symbol || nodes.marketSymbolInput.value;
    }
}

function renderMarketFacts(history) {
    const facts = [
        ["Provider", history.provider],
        ["Provider Symbol", history.providerSymbol],
        ["Start Date", history.startDate],
        ["End Date", history.endDate],
        ["Min Close", money(history.minClose)],
        ["Max Close", money(history.maxClose)],
        ["Latest Close", money(history.latestClose)],
        ["Observation Count", formatNumber(history.observations, 0)]
    ];

    nodes.marketRealFacts.replaceChildren();
    facts.forEach(([label, value]) => {
        const item = createElement("div", "assumption-item");
        item.append(textElement("span", "", label), textElement("strong", "", value || "N/A"));
        nodes.marketRealFacts.append(item);
    });
}

function renderRecentMarketRows(points) {
    nodes.marketRecentTable.replaceChildren();
    points.slice(-8).reverse().forEach((point) => {
        const row = document.createElement("tr");
        [
            point.date,
            money(point.close),
            point.logReturn == null ? "N/A" : signedPercent(point.logReturn),
            formatVolume(point.volume)
        ].forEach((value) => row.append(textElement("td", "", value)));
        nodes.marketRecentTable.append(row);
    });
}

function renderMarketChart(points) {
    nodes.marketChart.replaceChildren();
    if (!Array.isArray(points) || points.length < 2) {
        nodes.marketChart.append(textElement("div", "metric-note", "Not enough prices to draw a chart."));
        return;
    }

    const width = 1000;
    const height = 320;
    const padding = { top: 24, right: 42, bottom: 34, left: 54 };
    const closes = points.map((point) => Number(point.close));
    const minClose = Math.min(...closes);
    const maxClose = Math.max(...closes);
    const range = Math.max(maxClose - minClose, 1e-9);

    const x = (index) => padding.left
        + (index / Math.max(1, points.length - 1)) * (width - padding.left - padding.right);
    const y = (close) => padding.top
        + (1 - ((close - minClose) / range)) * (height - padding.top - padding.bottom);

    const linePath = points.map((point, index) => {
        const command = index === 0 ? "M" : "L";
        return `${command}${x(index).toFixed(2)},${y(point.close).toFixed(2)}`;
    }).join(" ");
    const areaPath = `${linePath} L${x(points.length - 1).toFixed(2)},${height - padding.bottom} L${padding.left},${height - padding.bottom} Z`;

    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
    svg.setAttribute("role", "img");
    svg.setAttribute("aria-label", "Historical close price line chart");

    [0, 0.25, 0.50, 0.75, 1].forEach((tick) => {
        const tickY = padding.top + tick * (height - padding.top - padding.bottom);
        svg.append(svgLine(padding.left, tickY, width - padding.right, tickY, "grid-line"));
    });

    svg.append(svgLine(padding.left, padding.top, padding.left, height - padding.bottom, "axis-line"));
    svg.append(svgLine(padding.left, height - padding.bottom, width - padding.right, height - padding.bottom, "axis-line"));
    svg.append(svgPath(areaPath, "price-area"));
    svg.append(svgPath(linePath, "price-line"));

    svg.append(svgText(padding.left, 18, money(maxClose), "chart-label"));
    svg.append(svgText(padding.left, height - 8, money(minClose), "chart-label"));
    svg.append(svgText(padding.left, height - 16, points[0].date, "chart-label"));
    svg.append(svgText(width - padding.right - 96, height - 16, points[points.length - 1].date, "chart-label"));

    nodes.marketChart.append(svg);
}

function svgLine(x1, y1, x2, y2, className) {
    const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1);
    line.setAttribute("y1", y1);
    line.setAttribute("x2", x2);
    line.setAttribute("y2", y2);
    line.setAttribute("class", className);
    return line;
}

function svgPath(d, className) {
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", d);
    path.setAttribute("class", className);
    return path;
}

function svgText(x, y, text, className) {
    const label = document.createElementNS("http://www.w3.org/2000/svg", "text");
    label.setAttribute("x", x);
    label.setAttribute("y", y);
    label.setAttribute("class", className);
    label.textContent = text;
    return label;
}

function createCompareCard(title, strategy) {
    const card = createElement("div", "compare-item");
    card.append(textElement("h3", "", title));

    [
        ["Avg P&L", signedMoney(strategy.averagePnl)],
        ["CVaR 95", money(strategy.conditionalValueAtRisk95)],
        ["Loss", percent(strategy.lossProbability)],
        ["Avg Cost", money(strategy.averageTransactionCosts)],
        ["Rebalances", formatNumber(strategy.averageRebalances, 1)]
    ].forEach(([label, value]) => {
        const row = createElement("div", "compare-metric");
        row.append(textElement("span", "", label), textElement("strong", "", value));
        card.append(row);
    });

    return card;
}

function createBarRow(label, value, ratio, color) {
    const row = createElement("div", "bar-row");
    const track = createElement("div", "bar-track");
    const fill = createElement("div", "bar-fill");
    fill.style.setProperty("--bar-width", `${Math.max(3, Math.min(100, ratio * 100))}%`);
    fill.style.setProperty("--bar-color", color);
    track.append(fill);

    row.append(
        textElement("div", "bar-label", label),
        track,
        textElement("div", "bar-value", value)
    );

    return row;
}

function sortStrategies(strategies) {
    const sorted = [...strategies];
    const comparators = {
        cvar: (a, b) => a.conditionalValueAtRisk95 - b.conditionalValueAtRisk95,
        pnl: (a, b) => b.averagePnl - a.averagePnl,
        cost: (a, b) => a.averageTransactionCosts - b.averageTransactionCosts,
        loss: (a, b) => a.lossProbability - b.lossProbability
    };

    return sorted.sort(comparators[state.sortMode] || comparators.cvar);
}

function activateTab(tab) {
    state.activeTab = tab;
    document.querySelectorAll("[data-tab]").forEach((button) => {
        button.classList.toggle("is-active", button.dataset.tab === tab);
    });
    document.querySelectorAll("[data-panel]").forEach((panel) => {
        panel.classList.toggle("is-hidden", panel.dataset.panel !== tab);
    });
    if (tab === "market" && !state.marketHistory) {
        loadMarketHistory();
    }
}

async function copyJson() {
    if (!state.response) {
        return;
    }

    await navigator.clipboard.writeText(JSON.stringify(state.response, null, 2));
    const original = nodes.copyJsonButton.querySelector("span").textContent;
    nodes.copyJsonButton.querySelector("span").textContent = "Copied";
    setTimeout(() => {
        nodes.copyJsonButton.querySelector("span").textContent = original;
    }, 1200);
}

function setLoading(isLoading) {
    nodes.runButton.disabled = isLoading;
    const label = nodes.runButton.querySelector("span");
    label.textContent = isLoading ? "Running..." : "Run Experiment";
}

function setMarketLoading(isLoading) {
    nodes.loadMarketButton.disabled = isLoading;
    const label = nodes.loadMarketButton.querySelector("span");
    label.textContent = isLoading ? "Loading..." : "Load Stock";
}

function showError(message) {
    nodes.errorPanel.textContent = message;
    nodes.errorPanel.classList.remove("is-hidden");
}

function clearError() {
    nodes.errorPanel.textContent = "";
    nodes.errorPanel.classList.add("is-hidden");
}

function setHealth(text, status) {
    nodes.healthText.textContent = text;
    nodes.healthPill.classList.toggle("is-up", status === "up");
    nodes.healthPill.classList.toggle("is-down", status === "down");
}

function createElement(tagName, className) {
    const element = document.createElement(tagName);
    if (className) {
        element.className = className;
    }
    return element;
}

function textElement(tagName, className, text) {
    const element = createElement(tagName, className);
    element.textContent = text ?? "N/A";
    return element;
}

function formatNumber(value, digits = 2) {
    if (!Number.isFinite(Number(value))) {
        return "N/A";
    }
    return Number(value).toLocaleString("en-US", {
        minimumFractionDigits: digits,
        maximumFractionDigits: digits
    });
}

function money(value) {
    if (!Number.isFinite(Number(value))) {
        return "N/A";
    }
    const numeric = Number(value);
    const sign = numeric < 0 ? "-" : "";
    return `${sign}$${formatNumber(Math.abs(numeric), 2)}`;
}

function signedMoney(value) {
    if (!Number.isFinite(Number(value))) {
        return "N/A";
    }
    const sign = Number(value) > 0 ? "+" : "";
    return `${sign}${money(value)}`;
}

function percent(value) {
    if (!Number.isFinite(Number(value))) {
        return "N/A";
    }
    return `${formatNumber(Number(value) * 100, 2)}%`;
}

function signedPercent(value) {
    if (!Number.isFinite(Number(value))) {
        return "N/A";
    }
    const sign = Number(value) > 0 ? "+" : "";
    return `${sign}${percent(value)}`;
}

function formatVolume(value) {
    if (!Number.isFinite(Number(value))) {
        return "N/A";
    }
    return Number(value).toLocaleString("en-US", {
        maximumFractionDigits: 0
    });
}

function safeAbs(value) {
    return Number.isFinite(Number(value)) ? Math.abs(Number(value)) : 0;
}

function formatDateTime(value) {
    if (!value) {
        return "unknown time";
    }
    return new Intl.DateTimeFormat("en-IN", {
        dateStyle: "medium",
        timeStyle: "medium"
    }).format(new Date(value));
}

function formatActionCounts(actionCounts) {
    if (!Array.isArray(actionCounts) || actionCounts.length === 0) {
        return "N/A";
    }
    return actionCounts
        .map((item) => `${item.action}: ${formatNumber(item.count, 0)}`)
        .join(", ");
}

function refreshIcons() {
    if (window.lucide) {
        window.lucide.createIcons();
    }
}

document.addEventListener("DOMContentLoaded", boot);
