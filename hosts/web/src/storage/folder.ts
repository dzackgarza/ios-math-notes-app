// The notes folder in Chromium: a directory handle from the File System
// Access API, kept in IndexedDB so it survives reloads. Follows Chrome's
// articles "The File System Access API" and "Persistent permissions for the
// File System Access API". A notebook is a subdirectory with notebook.json
// (docs/FORMAT.md).
import { get, set } from "idb-keyval";

import type { NotebookFile } from "../engine/engine.ts";

const ROOT_KEY = "notes-root";

export async function pickRoot(): Promise<FileSystemDirectoryHandle> {
  const root = await window.showDirectoryPicker({ id: "notes", mode: "readwrite" });
  await set(ROOT_KEY, root);
  return root;
}

// The folder chosen before, if any.
export async function savedRoot(): Promise<FileSystemDirectoryHandle | undefined> {
  return get<FileSystemDirectoryHandle>(ROOT_KEY);
}

export async function hasPermission(root: FileSystemDirectoryHandle): Promise<boolean> {
  return (await root.queryPermission({ mode: "readwrite" })) === "granted";
}

// Needs a user gesture: call it from a click.
export async function requestPermission(root: FileSystemDirectoryHandle): Promise<boolean> {
  return (await root.requestPermission({ mode: "readwrite" })) === "granted";
}

export async function listNotebooks(root: FileSystemDirectoryHandle): Promise<string[]> {
  const names: string[] = [];
  for await (const [name, handle] of root.entries()) {
    if (handle.kind !== "directory") continue;
    try {
      await handle.getFileHandle("notebook.json");
      names.push(name);
    } catch {
      // A directory without notebook.json is not a notebook.
    }
  }
  return names.sort();
}

export interface NotebookFiles {
  notebookJson: Uint8Array;
  pages: NotebookFile[];
  assets: NotebookFile[];
}

async function readDirectory(dir: FileSystemDirectoryHandle, prefix: string): Promise<NotebookFile[]> {
  const files: NotebookFile[] = [];
  for await (const [name, handle] of dir.entries()) {
    if (handle.kind !== "file") continue;
    const file = await handle.getFile();
    files.push({ path: `${prefix}/${name}`, bytes: new Uint8Array(await file.arrayBuffer()) });
  }
  return files;
}

async function subdirectory(dir: FileSystemDirectoryHandle, name: string): Promise<FileSystemDirectoryHandle | null> {
  try {
    return await dir.getDirectoryHandle(name);
  } catch {
    return null;
  }
}

export async function readNotebook(dir: FileSystemDirectoryHandle): Promise<NotebookFiles> {
  const json = await (await dir.getFileHandle("notebook.json")).getFile();
  const pages = await subdirectory(dir, "pages");
  const assets = await subdirectory(dir, "assets");
  return {
    notebookJson: new Uint8Array(await json.arrayBuffer()),
    pages: pages ? (await readDirectory(pages, "pages")).filter((f) => f.path.endsWith(".svg")) : [],
    assets: assets ? await readDirectory(assets, "assets") : [],
  };
}

// Chromium's exclusive writer: no other writer of the file while this one is
// open. Not yet in the File System Access type definitions.
type ExclusiveWritableOptions = FileSystemCreateWritableOptions & { mode: "exclusive" };

// Writes each file under `dir`, creating directories on the way.
export async function writeFiles(dir: FileSystemDirectoryHandle, files: readonly NotebookFile[]): Promise<void> {
  for (const file of files) {
    const parts = file.path.split("/");
    let parent = dir;
    for (const part of parts.slice(0, -1)) parent = await parent.getDirectoryHandle(part, { create: true });
    const handle = await parent.getFileHandle(parts[parts.length - 1], { create: true });
    const options: ExclusiveWritableOptions = { keepExistingData: false, mode: "exclusive" };
    const writable = await handle.createWritable(options);
    await writable.write(file.bytes);
    await writable.close();
  }
}
