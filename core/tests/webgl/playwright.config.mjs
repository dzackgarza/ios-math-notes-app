import { defineConfig, devices } from "@playwright/test";

const dir = process.env.WEBGL_CHECK_DIR;

export default defineConfig({
  testDir: ".",
  use: { baseURL: "http://127.0.0.1:8123" },
  webServer: { command: `python3 -m http.server 8123 -b 127.0.0.1 -d ${dir}`, url: "http://127.0.0.1:8123/index.html" },
  projects: [
    { name: "chromium", use: { ...devices["Desktop Chrome"] } },
    // The host GPU, as desktop Chrome uses it; headless Chromium otherwise
    // renders WebGL with SwiftShader. For timings on a machine with a GPU.
    {
      name: "chromium-gpu",
      use: { ...devices["Desktop Chrome"], launchOptions: { args: ["--use-angle=gl", "--ignore-gpu-blocklist"] } },
    },
  ],
});
