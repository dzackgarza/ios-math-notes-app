// Screenshots of the library, New Notebook, New Note and editor screens at
// 1366 × 1024, for review against docs/specs/ui/ (#47). Run against a served
// app: `bun e2e/screenshots.ts [base URL] [output directory]`.
import { chromium, type Page } from "playwright";

const base = process.argv[2] ?? process.env.MATH_NOTES_URL ?? "http://localhost/math-notes/";
const out = process.argv[3] ?? "screenshots";

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

for (const folder of ["Algebraic Geometry", "Seminar Notes", "Derived Categories"]) {
  await page.getByRole("button", { name: "New Notebook" }).click();
  await page.getByRole("textbox", { name: "Notebook Title" }).fill(folder);
  await page.getByRole("button", { name: "Create Notebook" }).click();
}
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
await page.getByRole("button", { name: "Library" }).click();
await page.getByRole("button", { name: "Algebraic Geometry", exact: true }).click();
await page.screenshot({ path: `${out}/library.png` });
await browser.close();
