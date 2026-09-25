// A notebook open in the engine, and its saving: the files that
// ink_document_dirty_files returns, written 1 s after the last committed edit.
import type { Engine, InkDocument } from "../engine/engine.ts";
import { EngineError, Status } from "../engine/engine.ts";
import { readNotebook, writeFiles } from "../storage/folder.ts";

export const SAVE_DELAY_MS = 1000;

export interface OpenNotebook {
  engine: Engine;
  document: InkDocument;
  dir: FileSystemDirectoryHandle;
  name: string;
  saver: Saver;
}

export class Saver {
  private readonly document: InkDocument;
  private readonly dir: FileSystemDirectoryHandle;
  // Files taken from the engine and not yet written, newest bytes per path.
  private readonly pending = new Map<string, Uint8Array<ArrayBuffer>>();
  private timer: ReturnType<typeof setTimeout> | undefined;
  private writing: Promise<void> = Promise.resolve();

  constructor(document: InkDocument, dir: FileSystemDirectoryHandle) {
    this.document = document;
    this.dir = dir;
  }

  // After a committed edit: save once edits pause for SAVE_DELAY_MS.
  schedule(): void {
    clearTimeout(this.timer);
    this.timer = setTimeout(() => void this.save(), SAVE_DELAY_MS);
  }

  // Takes the dirty files and marks them saved in the same task, so no edit
  // falls between; a failed write keeps them pending for the next save.
  save(): Promise<void> {
    clearTimeout(this.timer);
    for (const file of this.document.dirtyFiles()) this.pending.set(file.path, file.bytes);
    this.document.markSaved();
    this.writing = this.writing.then(async () => {
      const files = [...this.pending].map(([path, bytes]) => ({ path, bytes }));
      if (files.length === 0) return;
      await writeFiles(this.dir, files);
      for (const file of files) {
        if (this.pending.get(file.path) === file.bytes) this.pending.delete(file.path);
      }
      window.mathNotesWrites?.push(...files);
    });
    return this.writing;
  }
}

function randomSeed(): bigint {
  const words = crypto.getRandomValues(new BigUint64Array(1));
  return words[0];
}

export async function createNotebook(engine: Engine, root: FileSystemDirectoryHandle, name: string): Promise<OpenNotebook> {
  const dir = await root.getDirectoryHandle(name, { create: true });
  const document = engine.createDocument(randomSeed());
  const saver = new Saver(document, dir);
  await saver.save();
  return { engine, document, dir, name, saver };
}

export async function openNotebook(engine: Engine, root: FileSystemDirectoryHandle, name: string): Promise<OpenNotebook> {
  const dir = await root.getDirectoryHandle(name);
  const files = await readNotebook(dir);
  const document = engine.createDocument(randomSeed());
  document.loadNotebook(files.notebookJson);
  for (const page of files.pages) {
    try {
      document.loadPage(page.path, page.bytes);
    } catch (e) {
      // A page that does not parse stays in the notebook as an error page.
      if (!(e instanceof EngineError && e.status === Status.parse)) throw e;
    }
  }
  for (const asset of files.assets) document.loadAsset(asset.path, asset.bytes);
  return { engine, document, dir, name, saver: new Saver(document, dir) };
}
