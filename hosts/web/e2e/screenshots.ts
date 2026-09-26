// Screenshots of the first screen, the library, New Notebook, New Note and
// editor screens at 1366 × 1024, for review against the mockups in
// docs/specs/ui/ (just screenshots).
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

// Clicks `name` and waits until the sheet it opens has finished sliding in.
async function openSheet(page: Page, name: string): Promise<void> {
  const presented = page.evaluate(() => new Promise((done) => document.addEventListener("ionModalDidPresent", done, { once: true })));
  await page.getByRole("button", { name, exact: true }).click();
  await presented;
}

// Waves of ink on page 1 of the open note, then waits for the save: a reload
// less than 1 s after the last stroke drops it (TRAPS.md).
async function scribble(page: Page, lines: number): Promise<void> {
  const before = await page.evaluate(() => window.mathNotesWrites?.length ?? 0);
  const box = (await page.locator("#ink-canvas").boundingBox())!;
  for (let line = 0; line < lines; line++) {
    const y = box.y + 150 + line * 40;
    await drawWithPen(page, Array.from({ length: 40 }, (_, i) => ({ x: box.x + 80 + i * 10, y: y + 6 * Math.sin(i / 2) })));
  }
  await page.waitForFunction(
    (n) =>
      window.mathNotesWrites
        ?.slice(n)
        .some((f) => f.path === "pages/0001.svg" && new TextDecoder().decode(f.bytes).includes('<path id="s-')),
    before,
  );
}

// Every cover image drawn, so no tile shows plain paper.
async function coversLoaded(page: Page, count: number): Promise<void> {
  await page.waitForFunction(
    (n) => Array.from(document.querySelectorAll<HTMLImageElement>('img[src^="blob:"]')).filter((i) => i.complete && i.naturalWidth > 0).length >= n,
    count,
  );
  await page.waitForTimeout(300);
}

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1366, height: 1024 } });

// The first screen: no notes folder chosen yet.
await page.goto(base);
await page.getByRole("button", { name: "Choose notes folder" }).waitFor();
await page.screenshot({ path: `${out}/first-screen.png` });

await page.goto(new URL("favicon.svg", base).href);
await page.evaluate(async () => {
  const root = await navigator.storage.getDirectory();
  for await (const name of root.keys()) await root.removeEntry(name, { recursive: true });
});
await page.goto(new URL("?root=opfs", base).href);

const toRoot = () => page.getByRole("navigation", { name: "Folder path" }).getByRole("button", { name: "My Notes", exact: true }).click();
const newFolder = async (name: string) => {
  await openSheet(page, "New Notebook");
  await page.getByRole("textbox", { name: "Notebook Title" }).fill(name);
  await page.getByRole("button", { name: "Create Notebook" }).click();
};
const newNote = async (folder: string, title: string) => {
  await openSheet(page, `New Note in ${folder}`);
  await page.getByRole("textbox", { name: "Title" }).fill(title);
  await page.getByText("Dot Paper", { exact: true }).click();
  await page.getByRole("button", { name: "Create Note" }).click();
};
// Three top-level folders with a note each, and Moduli inside Algebraic Geometry.
for (const [folder, note] of [
  ["Seminar Notes", "Riemann–Roch"],
  ["Derived Categories", "Exact Triangles"],
  ["Algebraic Geometry", "Cone Theorem"],
]) {
  await toRoot();
  await newFolder(folder);
  await newNote(folder, note);
  await scribble(page, 3);
  await page.getByRole("button", { name: "Library" }).click();
}
await toRoot();
await page.getByRole("button", { name: "Algebraic Geometry", exact: true }).click();
await newFolder("Moduli");
await toRoot();
await openSheet(page, "New Notebook");
await page.getByRole("textbox", { name: "Notebook Title" }).fill("Minimal Models");
await page.screenshot({ path: `${out}/new-notebook.png` });
await page.getByRole("button", { name: "Cancel" }).click();

await page.getByRole("button", { name: "Algebraic Geometry", exact: true }).click();
await openSheet(page, "New Note in Algebraic Geometry");
await page.getByRole("textbox", { name: "Title" }).fill("Minimal Models in Dimension Three");
await page.getByText("Dot Paper", { exact: true }).click();
await page.screenshot({ path: `${out}/new-note.png` });
await page.getByRole("button", { name: "Create Note" }).click();
await scribble(page, 4);
await page.screenshot({ path: `${out}/editor.png` });

// Tags and a favorite, written to the library metadata file (src/storage/metadata.ts).
await page.evaluate(async () => {
  const root = await navigator.storage.getDirectory();
  const notes: Record<string, { favorite: boolean; tags: string[]; description: string }> = {};
  const tagged: Record<string, string[]> = {
    "Algebraic Geometry": ["Research", "AG"],
    "Seminar Notes": ["Seminar", "Papers"],
    "Derived Categories": ["Category Theory"],
  };
  for (const [folder, tags] of Object.entries(tagged)) {
    for await (const [name, entry] of (await root.getDirectoryHandle(folder)).entries()) {
      if (entry.kind === "directory" && name !== "Moduli") notes[`${folder}/${name}`] = { favorite: folder === "Algebraic Geometry", tags, description: "" };
    }
  }
  const metadata = {
    format: "math-notes-library",
    version: 1,
    tags: [
      { name: "Research", color: "#2F6FEB" },
      { name: "AG", color: "#3FA35B" },
      { name: "Seminar", color: "#F08A24" },
      { name: "Papers", color: "#8B5CF6" },
      { name: "Category Theory", color: "#C084FC" },
    ],
    notes,
  };
  const writable = await (await root.getFileHandle(".library.json", { create: true })).createWritable();
  await writable.write(`${JSON.stringify(metadata, null, 2)}\n`);
  await writable.close();
});
await page.reload();
await coversLoaded(page, 3);
await page.screenshot({ path: `${out}/library-root.png` });
await page.getByRole("button", { name: "Algebraic Geometry", exact: true }).click();
// The covers are page 1 as the engine draws it, loaded after the scan.
await coversLoaded(page, 5);
await page.screenshot({ path: `${out}/library.png` });
await browser.close();
