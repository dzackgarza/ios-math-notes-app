// Records the time of one 16-input stroke frame in the browser.
import { test, expect } from "@playwright/test";

test("one 16-input stroke frame", async ({ page }, info) => {
  await page.goto("/index.html");
  await page.waitForFunction(() => window.inkReady === true);
  const ms = await page.evaluate(() => Module.ccall("stroke_frame_ms", "number", [], []));
  console.log(`[${info.project.name}] 16-input frame: ${ms.toFixed(3)} ms mean`);
  expect(ms).toBeGreaterThan(0);
});
