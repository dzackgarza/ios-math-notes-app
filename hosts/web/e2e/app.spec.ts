// The deployed web app in Chromium: pen input through CDP, saving to the
// origin-private file system (?root=opfs), reload, and offline start.
/// <reference path="../src/window.d.ts" />
import { expect, test, type Page } from "@playwright/test";

const APP = "?root=opfs";

// Opens the app on an empty origin-private file system. The clearing runs on
// a page of the origin without the app, which holds no file of it open.
async function startEmpty(page: Page): Promise<void> {
  await page.goto("favicon.svg");
  await page.evaluate(async () => {
    const root = await navigator.storage.getDirectory();
    for await (const name of root.keys()) await root.removeEntry(name, { recursive: true });
  });
  await page.goto(APP);
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

// New Note from the library: in the selected folder, "My Notes" by default.
async function newNote(page: Page, title: string, paper = "Plain Paper"): Promise<void> {
  await page.getByRole("button", { name: "New Note", exact: true }).click();
  await page.getByRole("textbox", { name: "Title" }).fill(title);
  await page.getByText(paper, { exact: true }).click(); // the tile's label
  await page.getByRole("button", { name: "Create Note" }).click();
  await expect(page.locator("#ink-canvas")).toBeVisible();
}

// Opens a note from the selected folder's note list.
async function openNote(page: Page, title: string): Promise<void> {
  await page.getByRole("list", { name: "Notes" }).getByRole("listitem").filter({ hasText: title }).getByRole("button").first().click();
  await expect(page.locator("#ink-canvas")).toBeVisible();
}

// The last saved write of `path` that has a stroke, as text.
async function savedStrokes(page: Page, path: string): Promise<string> {
  const written = await page.waitForFunction(
    (p) => window.mathNotesWrites?.findLast((f) => f.path === p && new TextDecoder().decode(f.bytes).includes('<path id="s-')),
    path,
    { timeout: 5000 },
  );
  return written.evaluate((f) => new TextDecoder().decode(f!.bytes));
}

test("a pen stroke is saved, byte for byte as the engine wrote it, and renders after a reload", async ({ page }) => {
  await startEmpty(page);
  await newNote(page, "Algebra");
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
  await openNote(page, "Algebra");
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
  await expect(page.getByRole("heading", { name: "Library" })).toBeVisible();
  await context.setOffline(true);
  await page.reload();
  await expect(page.getByRole("heading", { name: "Library" })).toBeVisible();
  await expect(page.getByRole("button", { name: "New Note", exact: true })).toBeEnabled(); // the library shows once the engine loaded from the cache
});

test("Ctrl+Z undoes a stroke and the save removes it from the page file", async ({ page }) => {
  await startEmpty(page);
  await newNote(page, "Undo");
  const box = (await page.locator("#ink-canvas").boundingBox())!;
  await drawWithPen(page, Array.from({ length: 10 }, (_, i) => ({ x: box.x + 150 + i * 8, y: box.y + 100 })));
  await page.keyboard.press("Control+z");
  await expect
    .poll(async () => Buffer.from(await readOpfsFile(page, "Undo/pages/0001.svg"), "base64").toString(), { timeout: 5000 })
    .not.toContain('<path id="s-');
  await page.keyboard.press("Control+Shift+z");
  await expect
    .poll(async () => Buffer.from(await readOpfsFile(page, "Undo/pages/0001.svg"), "base64").toString(), { timeout: 5000 })
    .toContain('<path id="s-');
});

test("a note created in a new folder with the dotted template is listed in its folder, before and after a reload", async ({ page }) => {
  await startEmpty(page);
  await page.getByRole("button", { name: "New Notebook" }).click();
  await page.getByRole("textbox", { name: "Notebook Title" }).fill("Topology");
  await page.getByRole("button", { name: "Create Notebook" }).click();
  await page.getByRole("button", { name: "New Note in Topology" }).click();
  await page.getByRole("textbox", { name: "Title" }).fill("Knots");
  await page.getByText("Dot Paper", { exact: true }).click();
  await page.getByRole("button", { name: "Create Note" }).click();
  const box = (await page.locator("#ink-canvas").boundingBox())!;
  await drawWithPen(page, Array.from({ length: 20 }, (_, i) => ({ x: box.x + 150 + i * 8, y: box.y + 100 })));
  await savedStrokes(page, "pages/0001.svg");
  await page.getByRole("button", { name: "Library" }).click();

  const card = page.getByRole("list", { name: "Notebooks" }).getByRole("listitem").filter({
    has: page.getByRole("button", { name: "Topology", exact: true }),
  });
  const notes = page.getByRole("list", { name: "Notes" });
  for (const phase of ["before reload", "after reload"]) {
    await expect(card, phase).toContainText("1 note");
    await card.getByRole("button", { name: "Topology", exact: true }).click();
    await expect(notes.getByRole("listitem"), phase).toHaveCount(1);
    await expect(notes, phase).toContainText("Knots");
    if (phase === "before reload") await page.reload();
  }
  const notebook = JSON.parse(Buffer.from(await readOpfsFile(page, "Topology/Knots/notebook.json"), "base64").toString());
  expect(notebook.template).toBe("dotted");
  const svg = Buffer.from(await readOpfsFile(page, "Topology/Knots/pages/0001.svg"), "base64").toString();
  expect(svg).toContain('mn:ruling="dotted"');
  expect(svg).toContain('<path id="s-');
});

test("the tool rail's pen, highlighter and color reach the saved strokes", async ({ page }) => {
  await startEmpty(page);
  await newNote(page, "Tools");
  const box = (await page.locator("#ink-canvas").boundingBox())!;
  const line = (y: number) => Array.from({ length: 20 }, (_, i) => ({ x: box.x + 150 + i * 8, y: box.y + y }));

  await page.getByRole("button", { name: "Highlighter", exact: true }).click();
  await page.getByRole("button", { name: "#2BB3C0" }).click();
  await drawWithPen(page, line(100));
  await page.getByRole("button", { name: "Pen", exact: true }).click();
  await page.getByRole("button", { name: "#D6455D" }).click();
  await drawWithPen(page, line(200));

  await expect
    .poll(async () => {
      const svg = Buffer.from(await readOpfsFile(page, "Tools/pages/0001.svg"), "base64").toString();
      return [...svg.matchAll(/<path id="s-[^>]*? fill="(#[0-9A-F]{6})"[^>]*? mn:brush="([a-z-]+)"/g)].map((m) => [m[2], m[1]]);
    }, { timeout: 5000 })
    .toEqual([
      ["highlighter", "#2BB3C0"],
      ["pressure-pen", "#D6455D"],
    ]);
});

test("pulling past the last page adds a page only past the threshold, and the view stops at the pages", async ({ page }) => {
  await startEmpty(page);
  await newNote(page, "Pull");
  const box = (await page.locator("#ink-canvas").boundingBox())!;
  const indicator = page.getByLabel("Page", { exact: true });
  const pull = page.locator(".pull-indicator");
  // Paper, not desk, at the canvas's top and bottom edges.
  const paperAt = async (y: number) => Math.min(...(await pixel(page, box.x + 5, y)).slice(0, 3)) > 240;
  // The A4 page fills the canvas width: this far down its end meets the canvas's.
  const toEnd = (box.width * 841.89) / 595.28 - box.height;

  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.wheel(0, -500);
  expect(await paperAt(box.y + 1)).toBe(true);

  await page.mouse.wheel(0, toEnd + 40); // 40 px of pull, under the threshold
  await expect(pull).toHaveText("Pull to add a page");
  await expect(pull).toHaveCSS("height", "0px"); // released: springs back
  await expect(indicator).toHaveText(/\/ 1$/);
  expect(await paperAt(box.y + box.height - 2)).toBe(true);

  await page.mouse.wheel(0, 150);
  await expect(pull).toHaveText("Release to add a page");
  await expect(indicator).toHaveText(/\/ 2$/);
  await expect(pull).toHaveCSS("height", "0px");
});
