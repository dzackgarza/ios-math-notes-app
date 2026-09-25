// The deployed web app in Chromium: pen input through CDP, saving to the
// origin-private file system (?root=opfs), reload, and offline start.
/// <reference path="../src/window.d.ts" />
import { expect, test, type Page } from "@playwright/test";

const APP = "?root=opfs";

async function clearOpfs(page: Page): Promise<void> {
  await page.evaluate(async () => {
    const root = await navigator.storage.getDirectory();
    for await (const name of root.keys()) await root.removeEntry(name, { recursive: true });
  });
}

// A pen stroke through CDP Input.dispatchMouseEvent, in page coordinates.
async function drawWithPen(page: Page, points: { x: number; y: number }[]): Promise<void> {
  const cdp = await page.context().newCDPSession(page);
  const pen = { pointerType: "pen" as const, force: 0.6, tiltX: 20, tiltY: -10 };
  await cdp.send("Input.dispatchMouseEvent", { type: "mousePressed", button: "left", clickCount: 1, ...points[0], ...pen });
  for (const p of points.slice(1)) {
    await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", button: "left", buttons: 1, ...p, ...pen });
  }
  const last = points[points.length - 1];
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseReleased", button: "left", clickCount: 1, ...last, ...pen });
}

// RGBA of page pixel (x, y), read from a screenshot.
async function pixel(page: Page, x: number, y: number): Promise<number[]> {
  const png = (await page.screenshot({ clip: { x, y, width: 1, height: 1 } })).toString("base64");
  return page.evaluate(async (data) => {
    const image = new Image();
    image.src = `data:image/png;base64,${data}`;
    await image.decode();
    const context = new OffscreenCanvas(1, 1).getContext("2d")!;
    context.drawImage(image, 0, 0);
    return Array.from(context.getImageData(0, 0, 1, 1).data);
  }, png);
}

async function readOpfsFile(page: Page, path: string): Promise<string> {
  return page.evaluate(async (parts) => {
    let dir = await navigator.storage.getDirectory();
    for (const part of parts.slice(0, -1)) dir = await dir.getDirectoryHandle(part);
    const file = await (await dir.getFileHandle(parts[parts.length - 1])).getFile();
    return btoa(String.fromCharCode(...new Uint8Array(await file.arrayBuffer())));
  }, path.split("/"));
}

test("a pen stroke is saved, byte for byte as the engine wrote it, and renders after a reload", async ({ page }) => {
  await page.goto(APP);
  await clearOpfs(page);
  await page.reload();
  await page.getByRole("textbox", { name: "New notebook name" }).fill("Algebra");
  await page.getByRole("button", { name: "Create" }).click();
  const canvas = page.locator("#ink-canvas");
  await expect(canvas).toBeVisible();

  const box = (await canvas.boundingBox())!;
  const y = box.y + 120;
  const points = Array.from({ length: 30 }, (_, i) => ({ x: box.x + 200 + i * 6, y: y + 4 * Math.sin(i / 3) }));
  await drawWithPen(page, points);

  // The save lands 1 s after the pen lifts.
  const written = await page.waitForFunction(
    () => window.mathNotesWrites?.findLast((f) => f.path === "pages/0001.svg" && new TextDecoder().decode(f.bytes).includes('<path id="s-')),
    undefined,
    { timeout: 5000 },
  );
  const engineBytes = await written.evaluate((f) => btoa(String.fromCharCode(...f!.bytes)));
  expect(await readOpfsFile(page, "Algebra/pages/0001.svg")).toBe(engineBytes);
  const svg = Buffer.from(engineBytes, "base64").toString();
  expect(svg).toContain('mn:brush="pressure-pen"');
  expect(svg).toMatch(/<inkml:trace contextRef="#[a-z]+">[^<]+<\/inkml:trace>/);

  await page.reload();
  await page.getByRole("button", { name: "Algebra" }).click();
  await expect(canvas).toBeVisible();
  const [r, g, b] = await pixel(page, points[15].x, points[15].y);
  expect(Math.max(r, g, b)).toBeLessThan(80); // ink, not paper
  const [pr, pg, pb] = await pixel(page, points[15].x, points[15].y + 40);
  expect(Math.min(pr, pg, pb)).toBeGreaterThan(240); // paper
});

test("nginx serves the engine as application/wasm", async ({ page }) => {
  const wasm = page.waitForResponse((r) => r.url().endsWith(".wasm"));
  await page.goto(APP);
  const response = await wasm;
  expect(response.headers()["content-type"]).toBe("application/wasm");
});

test("after one visit the app starts offline", async ({ page, context }) => {
  await page.goto(APP);
  await page.evaluate(async () => {
    await navigator.serviceWorker.ready;
  });
  await page.reload(); // now controlled by the service worker
  await expect(page.getByRole("heading", { name: "Math Notes" })).toBeVisible();
  await context.setOffline(true);
  await page.reload();
  await expect(page.getByRole("heading", { name: "Math Notes" })).toBeVisible();
  await expect(page.getByRole("button", { name: "Create" })).toBeEnabled(); // the engine loaded from the cache
});
