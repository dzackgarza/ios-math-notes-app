import { defineConfig, devices } from "@playwright/test";

const dir = process.env.WEBGL_CHECK_DIR;

export default defineConfig({
  testDir: ".",
  use: { baseURL: "http://127.0.0.1:8123" },
  webServer: { command: `python3 -m http.server 8123 -b 127.0.0.1 -d ${dir}`, url: "http://127.0.0.1:8123/index.html" },
  projects: [
    { name: "chromium", use: { ...devices["Desktop Chrome"] } },
    // Headless Firefox on a GPU-less runner blocks WebGL unless forced.
    { name: "firefox", use: { ...devices["Desktop Firefox"], launchOptions: { firefoxUserPrefs: { "webgl.force-enabled": true } } } },
    { name: "webkit", use: { ...devices["Desktop Safari"] } },
  ],
});
