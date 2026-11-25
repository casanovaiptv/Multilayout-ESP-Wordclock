import { strict as assert } from "node:assert";
import { readFile, stat } from "node:fs/promises";
import { resolve, sep } from "node:path";
import { pathToFileURL } from "node:url";

const playwright = process.env.PLAYWRIGHT_MODULE
    ? await import(pathToFileURL(process.env.PLAYWRIGHT_MODULE))
    : await import("playwright");
const directory = resolve(process.argv[2] || "dist/development");
const browser = await playwright.chromium.launch({
    executablePath: process.env.CHROMIUM_PATH,
    args: ["--no-sandbox"]
});
const origin = "https://wordclock.test";
const contentTypes = {
    html: "text/html", js: "text/javascript", css: "text/css",
    json: "application/json", bin: "application/octet-stream", txt: "text/plain"
};
try {
    const page = await browser.newPage({ viewport: { width: 1366, height: 950 } });
    const errors = [];
    page.on("pageerror", error => errors.push(error.message));
    await page.route("https://unpkg.com/**", route => route.fulfill({
        contentType: "text/javascript",
        body: 'customElements.define("esp-web-install-button", class extends HTMLElement {});'
    }));
    await page.route(origin + "/**", async route => {
        const path = new URL(route.request().url()).pathname;
        const file = resolve(directory, "." + (path.endsWith("/") ? path + "index.html" : path));
        if (!file.startsWith(directory + sep)) return route.fulfill({ status: 403 });
        try {
            const info = await stat(file);
            const contentType = contentTypes[file.split(".").pop()] || "application/octet-stream";
            await route.fulfill({
                body: route.request().method() === "HEAD" ? "" : await readFile(file),
                headers: { "Content-Length": String(info.size) },
                contentType
            });
        } catch (error) {
            if (error.code !== "ENOENT") throw error;
            await route.fulfill({ status: 404 });
        }
    });
    const value = id => page.locator("#" + id).inputValue();
    const click = action => page.locator(`[data-hardware-action="${action}"]`).click();
    const open = async item => {
        if (page.viewportSize().width < 656) await page.locator(".hamburger").click();
        await page.locator(`.pure-menu-link[data-navigation="${item}"]`).click();
    };
    async function apply() {
        await page.locator("#hardware-apply").click();
        await page.waitForFunction(() => document.querySelector("#hardware-status").textContent === "Einstellungen gespeichert");
    }
    await page.goto(origin + "/demo/index.html?lng=de");
    assert.equal(await page.locator("#demo-clock").getAttribute("data-ring-leds"), "60");
    await open("frontoptions");
    assert.equal(await page.locator('#front-layout option[value="13"], #front-layout option[value="18"]').count(), 0);
    await page.locator("#front-layout").selectOption("24");
    assert.equal(await value("front-layout"), "24");

    await open("main");
    await page.locator('.swatch[data-color="#66a5d7"]').click();
    await page.locator("#brightness-24").selectOption("60");
    const stored = await page.evaluate(() => JSON.parse(localStorage.getItem("wordclock-development-demo-v1")));
    assert.equal(stored.h24, 60);
    assert.notEqual(stored.colors[0][0], 172);

    await open("hardware");
    assert.equal(await value("ring-offset"), "8");
    await page.locator("#ring-count").fill("48");
    await page.locator("#ring-offset").fill("47");
    await page.locator("#ring-reverse").check();
    await apply();
    assert.equal(await page.locator("#demo-clock").getAttribute("data-ring-leds"), "48");
    await click("start");
    await click("plus");
    assert.equal(await page.locator("#demo-clock").getAttribute("data-calibration-index"), "0");
    await click("minus");
    assert.equal(await page.locator("#demo-clock").getAttribute("data-calibration-index"), "47");
    await click("plus");
    await click("save");
    assert.equal(await value("ring-offset"), "0");
    await click("start");
    await click("plus");
    await click("cancel");
    assert.equal(await value("ring-offset"), "0");

    await page.locator("#ring-count").fill("120");
    await page.locator("#ring-enabled").uncheck();
    await page.locator("#power-limit").check();
    await page.locator("#power-supply").fill("0.5");
    await page.locator("#power-reserve").fill("0.49");
    await apply();
    await page.locator("#power-estimate.limiting").waitFor();
    assert.equal(await page.locator("#demo-clock").getAttribute("data-ring-leds"), "0");
    await page.locator("#wps-enabled").check();
    await page.locator("#wps-pin").fill("3");
    await page.locator("#hardware-apply").click();
    await page.waitForFunction(() => document.querySelector("#hardware-status").textContent.includes("bereits belegt"));
    await page.locator("#wps-pin").fill("4");
    await apply();
    await page.locator(".demo-panel summary").click();
    await page.locator("#demo-wps-outcome").selectOption("timeout");
    await page.locator("#demo-wps-start").click();
    await page.waitForFunction(() => document.querySelector("#demo-wps-status").textContent.includes("Zeitüberschreitung"));
    await page.locator("#demo-reload").click();
    await page.waitForLoadState();
    await open("hardware");
    assert.equal(await value("ring-count"), "120");
    assert.equal(await value("wps-pin"), "4");
    await open("frontoptions");
    assert.equal(await value("front-layout"), "24");

    for (const width of [390, 768, 1366]) {
        await page.setViewportSize({ width, height: 900 });
        await open("hardware");
        const overflow = await page.evaluate(() => document.documentElement.scrollWidth - innerWidth);
        if (overflow > 0) {
            const overflowing = await page.evaluate(() => [...document.querySelectorAll("*")]
                .filter(element => element.getBoundingClientRect().right > innerWidth + 1)
                .map(element => `${element.tagName}.${element.className}#${element.id}`).slice(0, 12));
            assert.fail(`page overflows by ${overflow}px at ${width}px: ${overflowing.join(", ")}`);
        }
        await page.locator("#hardware-apply").waitFor({ state: "visible" });
    }

    await page.goto(origin + "/");
    for (const [board, name, parts] of [["esp8266", "ESP8266", 2], ["esp32", "ESP32", 5], ["esp32c3", "ESP32-C3", 5]]) {
        await page.locator("#board").selectOption(board);
        await page.waitForFunction(expected => document.querySelectorAll("#downloads a").length === expected, parts);
        assert.ok((await page.locator("#version").textContent()).includes("-dev."));
        await page.waitForFunction(() => document.querySelector("#status").textContent.includes("USB"));
        const manifest = await page.locator("#installer").getAttribute("manifest");
        if (manifest) assert.ok(manifest.includes(board), name + " manifest selected");
    }
    await page.route(origin + "/firmware/esp32/firmware.bin", route => route.fulfill({ status: 404 }));
    await page.locator("#board").selectOption("esp32");
    await page.waitForFunction(() => document.querySelector("#status").textContent.includes("Firmware-Datei fehlt"));
    assert.equal(await page.locator("#installer").isVisible(), false);
    await page.route(origin + "/firmware/esp32c3/manifest.json", async route => {
        const manifest = JSON.parse(await readFile(resolve(directory, "firmware/esp32c3/manifest.json"), "utf-8"));
        manifest.new_install_prompt_erase = false;
        await route.fulfill({ json: manifest });
    });
    await page.locator("#board").selectOption("esp32c3");
    await page.waitForFunction(() => document.querySelector("#status").textContent.includes("passen nicht"));
    assert.equal(await page.locator("#installer").isVisible(), false);
    assert.deepEqual(errors, []);
    console.log("Demo, responsive layouts, persistence, calibration, limit, WPS and installer error handling passed.");
} finally {
    await browser.close();
}
