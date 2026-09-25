// Records engine timings in the browser.
import { test, expect } from "@playwright/test";

test("one 16-sample ink_input event", async ({ page }, info) => {
  await page.goto("/index.html");
  await page.waitForFunction(() => window.inkReady === true);
  const ms = await page.evaluate(() => Module.ccall("stroke_frame_ms", "number", [], []));
  console.log(`[${info.project.name}] 16-sample ink_input event: ${ms.toFixed(3)} ms mean`);
  expect(ms).toBeGreaterThan(0);
});

test("zooming 20 pages of 400 strokes", async ({ page }, info) => {
  await page.goto("/index.html");
  await page.waitForFunction(() => window.inkReady === true);
  const { mean, worst } = await page.evaluate(() => {
    const canvas = document.getElementById("canvas");
    canvas.width = 1280;
    canvas.height = 800;
    const mean = Module.ccall("zoom_frame_ms", "number", ["number", "number"], [1280, 800]);
    return { mean, worst: Module.ccall("zoom_worst_ms", "number", [], []) };
  });
  console.log(`[${info.project.name}] zoom frame, 1280x800: ${mean.toFixed(2)} ms mean, ${worst.toFixed(2)} ms worst`);
  expect(mean).toBeGreaterThan(0);
  // The 16 ms budget is for Chrome on the GPU (issue #20).
  if (info.project.name === "chromium-gpu") expect(worst).toBeLessThan(16);
});
