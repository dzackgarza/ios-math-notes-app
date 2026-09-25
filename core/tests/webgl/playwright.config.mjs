import { defineConfig, devices } from "@playwright/test";

const dir = process.env.WEBGL_CHECK_DIR;

export default defineConfig({
  testDir: ".",
  use: { baseURL: "http://127.0.0.1:8123" },
  webServer: { command: `python3 -m http.server 8123 -b 127.0.0.1 -d ${dir}`, url: "http://127.0.0.1:8123/index.html" },
  projects: [
    { name: "chromium", use: { ...devices["Desktop Chrome"] } },
    // Headless Firefox on a GPU-less runner finds no GL driver; CI runs it
    // headed under Xvfb, where Mesa supplies GL.
    { name: "firefox", use: { ...devices["Desktop Firefox"], headless: !process.env.CI } },
    { name: "webkit", use: { ...devices["Desktop Safari"] } },
  ],
});
