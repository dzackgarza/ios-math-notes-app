// Screenshots of the library, New Notebook, New Note and editor screens at
// 1366 × 1024, for review against the mockups in docs/specs/ui/ (just screenshots).
// Run against a served app: `bun e2e/screenshots.ts [base URL] [output directory]`.
/// <reference path="../src/window.d.ts" />
import { chromium, type Page } from "playwright";

const base = process.argv[2] ?? process.env.MATH_NOTES_URL ?? "http://localhost/math-notes/";
const out = process.argv[3] ?? new URL("../../../docs/specs/ui/screenshots", import.meta.url).pathname;

async function drawWithPen(page: Page, points: { x: number; y: number }[]): Promise<void> {
  const cdp = await page.context().newCDPSession(page);
  const pen = { pointerType: "pen" as const, force: 0.6 };
  await cdp.send("Input.dispatchMouseEvent", { type: "mousePressed", button: "left", clickCount: 1, ...points[0], ...pen });
  for (const p of points.slice(1)) await cdp.send("Input.dispatchMouseEvent", { type: "mouseMoved", button: "left", buttons: 1, ...p, ...pen });
  const last = points[points.length - 1];
  await cdp.send("Input.dispatchMouseEvent", { type: "mouseReleased", button: "left", clickCount: 1, ...last, ...pen });
}

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1366, height: 1024 } });
await page.goto(new URL("favicon.svg", base).href);
await page.evaluate(async () => {
  const root = await navigator.storage.getDirectory();
  for await (const name of root.keys()) await root.removeEntry(name, { recursive: true });
});
await page.goto(new URL("?root=opfs", base).href);

const toRoot = () => page.getByRole("navigation", { name: "Folder path" }).getByRole("button", { name: "My Notes", exact: true }).click();
const newFolder = async (name: string) => {
  await page.getByRole("button", { name: "New Notebook" }).click();
  await page.getByRole("textbox", { name: "Notebook Title" }).fill(name);
  await page.getByRole("button", { name: "Create Notebook" }).click();
};
// Three top-level folders, and Moduli inside Algebraic Geometry.
for (const folder of ["Algebraic Geometry", "Seminar Notes", "Derived Categories"]) {
  await toRoot();
  await newFolder(folder);
}
await toRoot();
await page.getByRole("button", { name: "Algebraic Geometry", exact: true }).click();
await newFolder("Moduli");
await toRoot();
await page.getByRole("button", { name: "New Notebook" }).click();
await page.getByRole("textbox", { name: "Notebook Title" }).fill("Minimal Models");
await page.screenshot({ path: `${out}/new-notebook.png` });
await page.getByRole("button", { name: "Cancel" }).click();

await page.getByRole("button", { name: "Algebraic Geometry", exact: true }).click();
await page.getByRole("button", { name: "New Note in Algebraic Geometry" }).click();
await page.getByRole("textbox", { name: "Title" }).fill("Minimal Models in Dimension Three");
await page.getByText("Dot Paper", { exact: true }).click();
await page.screenshot({ path: `${out}/new-note.png` });
await page.getByRole("button", { name: "Create Note" }).click();

const box = (await page.locator("#ink-canvas").boundingBox())!;
for (let line = 0; line < 4; line++) {
  const y = box.y + 120 + line * 40;
  await drawWithPen(page, Array.from({ length: 40 }, (_, i) => ({ x: box.x + 80 + i * 10, y: y + 6 * Math.sin(i / 2) })));
}
await page.screenshot({ path: `${out}/editor.png` });
// The save, 1 s after the last stroke, before the reload below.
await page.waitForFunction(() =>
  window.mathNotesWrites?.some((f) => f.path === "pages/0001.svg" && new TextDecoder().decode(f.bytes).includes('<path id="s-')),
);

// Tags and a favorite, written to the library metadata file (src/storage/metadata.ts).
await page.evaluate(async () => {
  const root = await navigator.storage.getDirectory();
  const folder = await root.getDirectoryHandle("Algebraic Geometry");
  const notes: Record<string, { favorite: boolean; tags: string[]; description: string }> = {};
  for await (const name of folder.keys()) notes[`Algebraic Geometry/${name}`] = { favorite: true, tags: ["Research", "AG"], description: "" };
  const metadata = {
    format: "math-notes-library",
    version: 1,
    tags: [{ name: "Research", color: "#2F6FEB" }, { name: "AG", color: "#3FA35B" }],
    notes,
  };
  const writable = await (await root.getFileHandle(".library.json", { create: true })).createWritable();
  await writable.write(`${JSON.stringify(metadata, null, 2)}\n`);
  await writable.close();
});
await page.reload();
await page.waitForFunction(() => Array.from(document.querySelectorAll<HTMLImageElement>('img[src^="blob:"]')).filter((i) => i.complete && i.naturalWidth > 0).length === 1);
await page.evaluate(() => Promise.all(Array.from(document.images).map((i) => i.decode())));
await page.screenshot({ path: `${out}/library-root.png` });
await page.getByRole("button", { name: "Algebraic Geometry", exact: true }).click();
// The covers are page 1 as the engine draws it, loaded after the scan.
await page.waitForFunction(() => Array.from(document.querySelectorAll<HTMLImageElement>('img[src^="blob:"]')).filter((i) => i.complete && i.naturalWidth > 0).length === 3);
await page.evaluate(() => Promise.all(Array.from(document.images).map((i) => i.decode())));
await page.screenshot({ path: `${out}/library.png` });
await browser.close();
