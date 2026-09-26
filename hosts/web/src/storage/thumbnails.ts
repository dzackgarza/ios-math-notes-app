// Note thumbnails: the engine renders page 1 to a PNG 240 px wide
// (ink_document_page_png), cached in the origin-private file system and keyed
// by the note's path and the name, modification time and size of page 1's
// file, so a thumbnail is rendered again only when that file changes. Caches
// are disposable (docs/FORMAT.md, invariant 3).
import type { Engine } from "../engine/engine.ts";
import { EngineError, Status } from "../engine/engine.ts";
import { directoryAt, type Note, pathKey } from "./library.ts";

export const THUMBNAIL_WIDTH = 240;

// A dot directory: with ?root=opfs the origin-private file system is also the
// notes root, whose scan skips dot directories.
const CACHE = ".thumbnail-cache";

// Cache reads and engine renders since the page loaded.
export const thumbnailStats = { hits: 0, renders: 0 };

async function hex(text: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, "0")).join("");
}

function notFound(e: unknown): boolean {
  return e instanceof DOMException && (e.name === "NotFoundError" || e.name === "TypeMismatchError");
}

async function filesOf(dir: FileSystemDirectoryHandle, name: string): Promise<{ path: string; bytes: Uint8Array<ArrayBuffer> }[]> {
  let sub: FileSystemDirectoryHandle;
  try {
    sub = await dir.getDirectoryHandle(name);
  } catch (e) {
    if (notFound(e)) return [];
    throw e;
  }
  const files = [];
  for await (const [entry, handle] of sub.entries()) {
    if (handle.kind === "file") files.push({ path: `${name}/${entry}`, bytes: new Uint8Array(await (await handle.getFile()).arrayBuffer()) });
  }
  return files;
}

// Page 1 of the notebook at `dir`, drawn by the engine from notebook.json,
// that page and the assets.
function render(engine: Engine, json: Uint8Array<ArrayBuffer>, page: { path: string; bytes: Uint8Array<ArrayBuffer> }, assets: { path: string; bytes: Uint8Array<ArrayBuffer> }[]): Uint8Array<ArrayBuffer> {
  const document = engine.createDocument(1n);
  try {
    document.loadNotebook(json);
    try {
      document.loadPage(page.path, page.bytes);
    } catch (e) {
      // A page that does not parse is drawn as the engine's error page.
      if (!(e instanceof EngineError && e.status === Status.parse)) throw e;
    }
    for (const asset of assets) document.loadAsset(asset.path, asset.bytes);
    return document.pagePng(0, THUMBNAIL_WIDTH);
  } finally {
    document.free();
  }
}

const pending = new Map<string, Promise<Blob | null>>();

async function load(engine: Engine, root: FileSystemDirectoryHandle, note: Note): Promise<Blob | null> {
  const dir = await directoryAt(root, note.path);
  const json = new Uint8Array(await (await (await dir.getFileHandle("notebook.json")).getFile()).arrayBuffer());
  const { pages } = JSON.parse(new TextDecoder().decode(json)) as { pages?: { file: string }[] };
  const first = pages?.[0]?.file;
  if (!first) return null;
  let page: File;
  try {
    const [folder, name] = first.split("/");
    page = await (await (await dir.getDirectoryHandle(folder)).getFileHandle(name)).getFile();
  } catch (e) {
    if (notFound(e)) return null;
    throw e;
  }

  const cache = await (await navigator.storage.getDirectory()).getDirectoryHandle(CACHE, { create: true });
  const entry = await cache.getDirectoryHandle(await hex(pathKey(note.path)), { create: true });
  const key = `${first.replace("/", "_")}-${page.lastModified}-${page.size}.png`;
  try {
    const cached = await (await entry.getFileHandle(key)).getFile();
    thumbnailStats.hits++;
    return cached;
  } catch (e) {
    if (!notFound(e)) throw e;
  }

  const png = render(engine, json, { path: first, bytes: new Uint8Array(await page.arrayBuffer()) }, await filesOf(dir, "assets"));
  thumbnailStats.renders++;
  for await (const old of entry.keys()) await entry.removeEntry(old);
  const writable = await (await entry.getFileHandle(key, { create: true })).createWritable();
  await writable.write(png);
  await writable.close();
  return new Blob([png], { type: "image/png" });
}

// The thumbnail of `note`'s page 1, or null for a note without one. Requests
// for one note while its thumbnail loads share the load.
export function noteThumbnail(engine: Engine, root: FileSystemDirectoryHandle, note: Note): Promise<Blob | null> {
  const id = `${pathKey(note.path)}@${note.modified}`;
  let request = pending.get(id);
  if (!request) {
    request = load(engine, root, note).finally(() => pending.delete(id));
    pending.set(id, request);
  }
  return request;
}
