// The library view of the notes root (docs/specs/tablet-ui.md, "Relation to
// the current model"): a folder is a directory without notebook.json, a note
// is a notebook directory (docs/FORMAT.md). Notes at the top level form the
// "My Notes" group. Dot directories (.templates, .trash, ...) are not folders.

export interface Note {
  // Path segments from the root, the note's own directory last.
  path: string[];
  name: string;
  template: string;
  // Latest lastModified of notebook.json and the page files, ms since epoch.
  modified: number;
}

export interface Folder {
  // Path segments from the root; [] for the "My Notes" group.
  path: string[];
  name: string;
  notes: Note[];
  // The latest note modification, or 0 for an empty folder.
  modified: number;
}

export const MY_NOTES = "My Notes";

export const pathKey = (path: readonly string[]): string => path.join("/");

export async function directoryAt(root: FileSystemDirectoryHandle, path: readonly string[]): Promise<FileSystemDirectoryHandle> {
  let dir = root;
  for (const part of path) dir = await dir.getDirectoryHandle(part);
  return dir;
}

async function fileIfPresent(dir: FileSystemDirectoryHandle, name: string): Promise<File | null> {
  try {
    return await (await dir.getFileHandle(name)).getFile();
  } catch (e) {
    if (e instanceof DOMException && (e.name === "NotFoundError" || e.name === "TypeMismatchError")) return null;
    throw e;
  }
}

async function readNote(dir: FileSystemDirectoryHandle, path: string[], json: File): Promise<Note> {
  const { template } = JSON.parse(await json.text()) as { template?: string };
  let modified = json.lastModified;
  try {
    for await (const [, handle] of (await dir.getDirectoryHandle("pages")).entries()) {
      if (handle.kind === "file") modified = Math.max(modified, (await handle.getFile()).lastModified);
    }
  } catch (e) {
    if (!(e instanceof DOMException && e.name === "NotFoundError")) throw e;
  }
  return { path, name: path[path.length - 1], template: template ?? "blank", modified };
}

function folder(path: string[], notes: Note[]): Folder {
  notes.sort((a, b) => a.name.localeCompare(b.name));
  return {
    path,
    name: path.length === 0 ? MY_NOTES : path.join(" / "),
    notes,
    modified: Math.max(0, ...notes.map((n) => n.modified)),
  };
}

// Every folder under `root`, nested ones included, with its notes. "My Notes"
// comes first, then the folders by path.
export async function scanLibrary(root: FileSystemDirectoryHandle): Promise<Folder[]> {
  const folders: Folder[] = [];
  const visit = async (dir: FileSystemDirectoryHandle, path: string[]): Promise<void> => {
    const notes: Note[] = [];
    for await (const [name, handle] of dir.entries()) {
      if (handle.kind !== "directory" || name.startsWith(".")) continue;
      const json = await fileIfPresent(handle, "notebook.json");
      if (json) notes.push(await readNote(handle, [...path, name], json));
      else await visit(handle, [...path, name]);
    }
    folders.push(folder(path, notes));
  };
  await visit(root, []);
  return folders.sort((a, b) => (a.path.length === 0 ? -1 : b.path.length === 0 ? 1 : a.name.localeCompare(b.name)));
}

// The notes moved to Notes/.trash/ (docs/FORMAT.md, Layout).
export async function scanTrash(root: FileSystemDirectoryHandle): Promise<Note[]> {
  let trash: FileSystemDirectoryHandle;
  try {
    trash = await root.getDirectoryHandle(TRASH);
  } catch (e) {
    if (e instanceof DOMException && e.name === "NotFoundError") return [];
    throw e;
  }
  const notes: Note[] = [];
  for await (const [name, handle] of trash.entries()) {
    if (handle.kind !== "directory") continue;
    const json = await fileIfPresent(handle, "notebook.json");
    if (json) notes.push(await readNote(handle, [TRASH, name], json));
  }
  return notes.sort((a, b) => b.modified - a.modified);
}

export const TRASH = ".trash";

export async function createFolder(root: FileSystemDirectoryHandle, parent: readonly string[], name: string): Promise<string[]> {
  await (await directoryAt(root, parent)).getDirectoryHandle(name, { create: true });
  return [...parent, name];
}

// Chromium's FileSystemHandle.move (File System Access, "move()"), which
// moves a directory with its contents. Not yet in the type definitions.
interface MovableHandle {
  move(parent: FileSystemDirectoryHandle, name: string): Promise<void>;
}

// Moves a note into Notes/.trash/, keeping its name; a name already in the
// trash gets " 2", " 3", ... appended.
export async function moveToTrash(root: FileSystemDirectoryHandle, note: Note): Promise<void> {
  const trash = await root.getDirectoryHandle(TRASH, { create: true });
  const taken = new Set<string>();
  for await (const name of trash.keys()) taken.add(name);
  let target = note.name;
  for (let i = 2; taken.has(target); i++) target = `${note.name} ${i}`;
  const handle = (await directoryAt(root, note.path)) as FileSystemDirectoryHandle & MovableHandle;
  await handle.move(trash, target);
}
