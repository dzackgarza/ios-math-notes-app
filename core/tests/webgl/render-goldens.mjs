// Writes core/tests/fixtures/render/<notebook>/<page>.png: Chromium's
// rendering of each page SVG the engine wrote (core/tests/fixtures/documents),
// at 1 device pixel per pt. Run by `just render-goldens`.
//
// Chromium draws the SVG as an image onto a canvas with an exact destination
// rectangle. Opened as a document, the root's mm size is snapped to whole CSS
// pixels (1122.52 px -> 1123 px), which scales the page by up to 0.05%. An
// SVG image loads no external files, so each asset href, resolved against
// the source notebook in tests/documents, is inlined as a data URI.
import { chromium } from "@playwright/test";
import { mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

const root = resolve(import.meta.dirname, "../../..");
// Written notebooks, the notebooks whose assets they reference, and where
// their goldens go.
const sets = [
  { written: join(root, "core/tests/fixtures/documents"), sources: join(root, "tests/documents"), out: join(root, "core/tests/fixtures/render") },
  { written: join(root, "core/tests/fixtures/templates"), sources: join(root, "core/tests/fixtures/templates"), out: join(root, "core/tests/fixtures/render/templates") },
];

const browser = await chromium.launch();
const page = await browser.newPage({ deviceScaleFactor: 1 });
for (const { written, sources, out } of sets) for (const notebook of readdirSync(written)) {
  const pages = join(written, notebook, "pages");
  // Listed pages only: the engine lays out and draws no others.
  const listed = JSON.parse(readFileSync(join(written, notebook, "notebook.json"), "utf8")).pages;
  for (const file of listed.map((p) => p.file.slice("pages/".length)).filter((f) => readdirSync(pages).includes(f))) {
    const svg = readFileSync(join(pages, file), "utf8").replace(/href="([^"#:]+\.png)"/g, (_, href) => {
      const png = readFileSync(resolve(join(sources, notebook, "pages"), href));
      return `href="data:image/png;base64,${png.toString("base64")}"`;
    });
    const [, width, height] = svg.match(/viewBox="0 0 ([\d.]+) ([\d.]+)"/).map(Number);
    const base64 = await page.evaluate(
      async ({ svg, width, height }) => {
        const image = new Image();
        image.src = `data:image/svg+xml;base64,${svg}`;
        await image.decode();
        const canvas = document.createElement("canvas");
        canvas.width = Math.floor(width);
        canvas.height = Math.floor(height);
        // CPU rasterization, as the engine's raster surface.
        canvas.getContext("2d", { willReadFrequently: true }).drawImage(image, 0, 0, width, height);
        return canvas.toDataURL("image/png").split(",")[1];
      },
      { svg: Buffer.from(svg).toString("base64"), width, height },
    );
    const target = join(out, notebook, file.replace(".svg", ".png"));
    mkdirSync(dirname(target), { recursive: true });
    writeFileSync(target, Buffer.from(base64, "base64"));
    console.log(target);
  }
}
await browser.close();
