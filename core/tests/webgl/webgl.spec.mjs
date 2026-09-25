// Playwright: the Skia WebGL2 surface draws a parsed SVG path.
// Served without COOP/COEP headers.
import { test, expect } from "@playwright/test";

test("Skia draws an SVG path on a WebGL2 surface", async ({ page }) => {
  await page.goto("/index.html");
  await page.waitForFunction(() => window.inkReady === true);
  const result = await page.evaluate(() => {
    const status = Module.ccall("webgl_draw", "number", ["number", "number"], [100, 100]);
    const px = (x, y) => (Module.ccall("webgl_pixel", "number", ["number", "number"], [x, y]) >>> 0)
      .toString(16).padStart(8, "0");
    return { status, inside: px(50, 30), apex: px(50, 60), outside: px(95, 95) };
  });
  expect(result).toEqual({ status: 0, inside: "ff0000ff", apex: "ff0000ff", outside: "ffffffff" });
});
