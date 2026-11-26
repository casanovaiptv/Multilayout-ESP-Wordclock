/* This adapter is injected only into the hosted demo. It never opens a socket. */
(() => {
    "use strict";
    const key = "wordclock-development-demo-v1";
    const hours = [6, 8, 12, 16, 18, 20, 22, 24];
    const defaults = {
        clockTypeDef: 11, prog: 1, bitmapSymbol: 7, colortype: 0, buildtype: 0,
        minuteVariant: 1, supportedMinuteVariants: 15, itIsVariant: 0,
        ssid: "Wordclock Demo", hostname: "wordclock-demo", timeserver: "pool.ntp.org",
        timezone: "CET-1CEST,M3.5.0,M10.5.0/3", scrollingText: "WORDCLOCK DEMO",
        effectBri: 70, effectSpeed: 5, autoBrightEnabled: 0,
        ledPin: 3, powerButtonPin: 255, modeButtonPin: 255, speedButtonPin: 255,
        i2cSdaPin: 255, i2cSclPin: 255,
        colors: [[172, 65, 90], [215, 80, 0], [172, 70, 75]],
        enabled: true, count: 60, offset: 8, reverse: false, animation: 1,
        limit: false, supply: 2000, reserve: 300, wps: false, pin: 19
    };
    let state = structuredClone(defaults);
    try { Object.assign(state, JSON.parse(localStorage.getItem(key))); } catch { /* storage is optional */ }
    hours.forEach(hour => { if (state["h" + hour] == null) state["h" + hour] = 70; });
    let preview = null;
    let wps = "";
    let demoSocket;
    let ledMa = 0;
    let limiting = false;
    let lastWps = 0;

    function persist() {
        try { localStorage.setItem(key, JSON.stringify(state)); } catch { /* session stays interactive */ }
    }

    function rgb([hue, saturation, value], brightness) {
        const sat = saturation / 100;
        const val = value * brightness / 10000;
        return [5, 3, 1].map(channel => {
            const position = (channel + hue / 60) % 6;
            return Math.floor(255 * val * (1 - sat * Math.max(0, Math.min(position, 4 - position, 1))));
        });
    }

    function render() {
        const canvas = document.getElementById("demo-clock");
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        const now = new Date();
        const brightnessHour = hours.find(hour => now.getHours() < hour);
        const brightness = [4, 5].includes(state.prog) ? state.effectBri : state["h" + brightnessHour];
        const shapes = { 0: [8, 9], 3: [8, 8], 5: [11, 12], 6: [8, 8], 9: [8, 8],
            10: [8, 8], 20: [11, 11], 21: [11, 11], 22: [11, 11], 23: [11, 11],
            24: [13, 13], 25: [8, 16], 26: [18, 16], 27: [22, 11], 28: [10, 10] };
        const [rows, cols] = shapes[state.clockTypeDef] || [10, 11];
        const variant = Number(state.buildtype);
        const letterLeds = variant >= 2 ? variant : 1;
        const ringLeds = state.enabled ? state.count : 0;
        const minuteColors = state.minuteVariant && preview == null && !wps ? now.getMinutes() % 5 : 0;
        const foreground = rgb(state.colors[0], brightness);
        const background = rgb(state.colors[1], brightness);
        const frame = rgb(state.colors[2], brightness);
        const black = [0, 0, 0];
        const matrix = [];
        const minute = [];
        const ring = [];
        const phrase = [4, 5].includes(state.clockTypeDef) ? "ITISTENPASTTWELVE" : "ESISTZEHNNACHZWÖLF";
        for (let index = 0; index < rows * cols; index++) {
            const active = state.prog === 6 || index < 5 || (index >= cols * 2 && index < cols * 2 + 9);
            const color = preview != null ? black : wps === "success" ? [0, 51, 0]
                : wps === "failed" ? [51, 0, 0] : wps ? [0, 0, 51] : active ? foreground : background;
            matrix.push({ color, char: phrase[index % phrase.length] });
        }
        for (let index = 0; index < 4; index++) minute.push(index < minuteColors ? foreground : black);
        const sector = Math.floor((now.getSeconds() + now.getMilliseconds() / 1000) * ringLeds / 60);
        for (let index = 0; index < ringLeds; index++) {
            const logical = ((index - state.offset + ringLeds) * (state.reverse ? -1 : 1) + ringLeds) % ringLeds;
            const lit = state.animation === 1 ? logical === sector
                : state.animation === 2 ? logical <= sector
                    : state.animation === 3 && ((logical <= sector) !== (now.getMinutes() % 2 === 1));
            ring.push(preview != null ? (preview === index ? [0, 64, 57] : black) : lit && !wps ? frame : black);
        }
        const sum = color => color[0] + color[1] + color[2];
        const matrixCount = rows * (variant === 1 ? cols * 2 - 1 : cols) * letterLeds;
        const idle = matrixCount + 7 + ring.length;
        const total = matrix.reduce((result, entry) => result + sum(entry.color) * letterLeds, 0)
            + minute.reduce((result, color) => result + sum(color), 0)
            + ring.reduce((result, color) => result + sum(color), 0);
        const budget = state.supply - state.reserve;
        const gain = state.limit && total ? Math.max(0, Math.min(1, 255 * (budget - idle) / (20 * total))) : 1;
        limiting = state.limit && (gain < 1 || idle > budget);
        const scaled = color => color.map(channel => Math.floor(channel * gain));
        ledMa = idle + Math.ceil(20 * (
            matrix.reduce((result, entry) => result + sum(scaled(entry.color)) * letterLeds, 0)
            + minute.reduce((result, color) => result + sum(scaled(color)), 0)
            + ring.reduce((result, color) => result + sum(scaled(color)), 0)) / 255);
        const css = (color, off) => sum(color) ? "rgb(" + scaled(color).join(",") + ")" : off;
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.font = "600 " + Math.min(22, 325 / rows) + "px system-ui";
        matrix.forEach((entry, index) => {
            ctx.fillStyle = css(entry.color, "#23384b");
            ctx.fillText(entry.char, 95 + (index % cols) * 290 / (cols - 1),
                95 + Math.floor(index / cols) * 290 / (rows - 1));
        });
        const point = (x, y, color, size) => {
            ctx.fillStyle = css(color, "#203347");
            ctx.beginPath();
            ctx.arc(x, y, size, 0, Math.PI * 2);
            ctx.fill();
        };
        minute.forEach((color, index) => point(215 + index * 16, 414, color, 3));
        ring.forEach((color, index) => {
            const position = index * 4 / ring.length;
            const side = Math.floor(position);
            const fraction = position - side;
            const corners = [[32, 32], [448, 32], [448, 448], [32, 448]];
            const current = corners[side];
            const next = corners[(side + 1) % 4];
            point(current[0] + fraction * (next[0] - current[0]),
                current[1] + fraction * (next[1] - current[1]), color, Math.min(4, 650 / ring.length));
        });
        canvas.dataset.ringLeds = ringLeds;
        canvas.dataset.calibrationIndex = preview == null ? "" : preview;
    }

    function hardware(socket, message) {
        const action = message.hardware;
        let error = null;
        if (action === "apply") {
            const { enabled, count, offset, reverse, limit, supply, reserve, wps: enableWps, pin } = message;
            if (!Number.isInteger(count) || count < 1 || count > 200 ||
                !Number.isInteger(offset) || offset < 0 || offset >= count ||
                !Number.isInteger(supply) || supply < 1 || supply > 60000 || reserve < 0 || reserve > supply) {
                error = "invalid";
            } else if (enableWps && (![4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33].includes(pin) ||
                [state.ledPin, state.powerButtonPin, state.modeButtonPin, state.speedButtonPin, state.i2cSdaPin, state.i2cSclPin].includes(pin))) {
                error = "gpio";
            } else {
                if (enabled && !state.enabled && !state.animation) state.animation = 1;
                Object.assign(state, { enabled, count, offset, reverse, limit, supply, reserve, wps: enableWps, pin });
                persist();
            }
        } else if (action === "start" && state.enabled) {
            preview = state.offset;
        } else if ((action === "plus" || action === "minus") && preview != null) {
            preview = (preview + state.count + (action === "plus" ? 1 : -1)) % state.count;
        } else if (action === "save" && preview != null) {
            state.offset = preview;
            preview = null;
            persist();
        } else if (action === "cancel") {
            preview = null;
        }
        document.getElementById("demo-wps-start").disabled = !state.wps;
        render();
        socket.reply("hardware", Object.assign({}, state, {
            error, telemetry: action === "status", calibrating: preview != null,
            preview: preview == null ? state.offset : preview, ledMa, limiting
        }));
    }

    class DemoSocket {
        static OPEN = 1;
        constructor() {
            this.readyState = DemoSocket.OPEN;
            demoSocket = this;
            queueMicrotask(() => this.onopen?.({}));
        }

        reply(command, data) {
            queueMicrotask(() => this.onmessage?.({ data: JSON.stringify(Object.assign({}, data, { command })) }));
        }

        close() {
            this.readyState = 3;
            preview = null;
            this.onclose?.({ code: 1000 });
        }

        send(raw) {
            if (raw.startsWith("{")) {
                hardware(this, JSON.parse(raw));
                return;
            }
            const command = Number(raw.slice(0, 3));
            const values = (raw.slice(3).match(/.{3}/g) || []).map(Number);
            if (command === 200) {
                this.reply("config", Object.assign({}, state, { hasSecondsFrame: state.enabled, hasTwenty: true, supportedMinuteVariants: 15 }));
                return;
            }
            if (command === 201) {
                const colors = {};
                state.colors.forEach((color, index) => color.forEach((channel, i) => { colors["hsb" + index + i] = channel; }));
                this.reply("set", Object.assign({}, state, colors, { hasSecondsFrame: state.enabled }));
                return;
            }
            if (command === 203) {
                this.reply("autoBright", { autoBrightEnabled: state.autoBrightEnabled, autoBrightSensor: 160,
                    autoBrightSource: "Demo", autoBrightGain: 70, autoBrightMin: 10, autoBrightMax: 80, autoBrightPeak: 750 });
                return;
            }
            if (command === 204) {
                this.reply("transition", { transitionType: 0, transitionDuration: 2, transitionSpeed: 30, transitionColorize: 0 });
                return;
            }
            if (command <= 10) {
                state.prog = command;
                if (values.length >= 6 && values[0] < 3) {
                    state.colors[values[0]] = values.slice(1, 4);
                    state.effectBri = values[4];
                    state.effectSpeed = values[5];
                }
            } else if (command === 87) state.buildtype = values[0];
            else if (command === 89) state.clockTypeDef = values[0];
            else if (command === 93) state.animation = values[0];
            else if (command === 94) state.minuteVariant = values[0];
            else if (command === 95) {
                hours.forEach((hour, index) => { state["h" + hour] = values[index]; });
                state.effectBri = values[8];
            } else if (command === 102) state.autoBrightEnabled = values[0];
            else return;
            render();
            persist();
        }
    }

    window.WebSocket = DemoSocket;
    document.addEventListener("DOMContentLoaded", () => {
        document.getElementById("demo-wps-start").addEventListener("click", () => {
            const id = ++lastWps;
            const result = document.getElementById("demo-wps-outcome").value;
            const output = document.getElementById("demo-wps-status");
            wps = "pairing";
            output.textContent = "Simulation · WPS läuft";
            render();
            setTimeout(() => {
                if (id !== lastWps) return;
                wps = result === "success" ? "success" : "failed";
                output.textContent = { success: "Simulation · verbunden", failed: "Simulation · Fehler, Captive Portal",
                    timeout: "Simulation · Zeitüberschreitung, Captive Portal" }[result];
                render();
                setTimeout(() => { if (id === lastWps) wps = ""; }, 1800);
            }, 1700);
        });
        document.getElementById("demo-reload").addEventListener("click", () => location.reload());
        document.getElementById("demo-reset").addEventListener("click", () => {
            try { localStorage.removeItem(key); } catch { /* storage is optional */ }
            location.reload();
        });
        setInterval(() => {
            if (demoSocket?.readyState === DemoSocket.OPEN) render();
        }, 100);
    });
})();
