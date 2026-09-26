import { Button } from "@kobalte/core/button";
import { createResource, createSignal, Match, onCleanup, onMount, Show, Switch } from "solid-js";

import { Editor, type Tab } from "./editor/Editor.tsx";
import { createNotebook, openNotebook, type OpenNotebook } from "./editor/notebook.ts";
import type { Engine } from "./engine/engine.ts";
import { loadEngine } from "./engine/load.ts";
import { ensureTemplates, hasPermission, listTemplates, pickRoot, requestPermission, savedRoot } from "./storage/folder.ts";
import { createFolder, moveEntry, moveToTrash, type Note, pathKey, scanLibrary, scanTrash } from "./storage/library.ts";
import { type LibraryMetadata, moveNotes, readMetadata, writeMetadata } from "./storage/metadata.ts";
import { noteThumbnail, thumbnailStats } from "./storage/thumbnails.ts";
import { NewNote, NewNotebook } from "./ui/Create.tsx";
import { AppMark, Library, type Section } from "./ui/Library.tsx";

// `?root=opfs` uses the origin-private file system as the notes folder: the
// automated tests cannot drive the native folder picker.
async function initialRoot(): Promise<{ root?: FileSystemDirectoryHandle; needsGesture: boolean }> {
  if (new URLSearchParams(location.search).get("root") === "opfs") {
    window.mathNotesWrites = [];
    window.mathNotesThumbnails = thumbnailStats;
    return { root: await navigator.storage.getDirectory(), needsGesture: false };
  }
  const root = await savedRoot();
  if (!root) return { needsGesture: false };
  return (await hasPermission(root)) ? { root, needsGesture: false } : { root, needsGesture: true };
}

type Screen = { kind: "library" } | { kind: "new-notebook" } | { kind: "new-note"; folder: string[] } | { kind: "editor" };

// Everything the library screens show, read from the notes root.
async function readLibrary(args: { root: FileSystemDirectoryHandle; engine: Engine }) {
  await ensureTemplates(args.root, args.engine);
  const [folders, trash, metadata, templates] = await Promise.all([
    scanLibrary(args.root),
    scanTrash(args.root),
    readMetadata(args.root),
    listTemplates(args.root),
  ]);
  return { folders, trash, metadata, templates };
}

export function App() {
  const [engine] = createResource<Engine>(loadEngine);
  const [start] = createResource(initialRoot);
  const [root, setRoot] = createSignal<FileSystemDirectoryHandle>();
  const [error, setError] = createSignal("");
  const [screen, setScreen] = createSignal<Screen>({ kind: "library" });
  const [section, setSection] = createSignal<Section>("library");
  const [selected, setSelected] = createSignal<string[]>([]);
  const [open, setOpen] = createSignal<OpenNotebook>();
  const [tabs, setTabs] = createSignal<Tab[]>([]);

  const current = () => root() ?? (start()?.needsGesture ? undefined : start()?.root);
  const [library, { refetch, mutate }] = createResource(
    () => {
      const r = current(), e = engine();
      return r && e ? { root: r, engine: e } : undefined;
    },
    readLibrary,
  );

  const run = (action: () => Promise<void>) => {
    setError("");
    action().catch((e: unknown) => setError(e instanceof Error ? e.message : String(e)));
  };
  const choose = () =>
    run(async () => {
      setRoot(await pickRoot());
    });
  const reconnect = () =>
    run(async () => {
      const saved = start()?.root;
      if (saved && (await requestPermission(saved))) setRoot(saved);
    });

  const showNotebook = (notebook: OpenNotebook) => {
    if (!tabs().some((t) => pathKey(t.path) === pathKey(notebook.path))) {
      setTabs([...tabs(), { path: notebook.path, name: notebook.name }]);
    }
    setOpen(notebook);
    setScreen({ kind: "editor" });
  };
  const openPath = (path: string[]) =>
    run(async () => {
      const r = current(), e = engine();
      if (r && e) showNotebook(await openNotebook(e, r, path));
    });
  const toLibrary = () => {
    setOpen(undefined);
    setScreen({ kind: "library" });
    refetch();
  };
  const closeTab = (path: string[]) => {
    const rest = tabs().filter((t) => pathKey(t.path) !== pathKey(path));
    setTabs(rest);
    const active = open();
    if (active && pathKey(active.path) !== pathKey(path)) return;
    if (rest.length > 0) openPath(rest[rest.length - 1].path);
    else toLibrary();
  };

  const updateMetadata = (change: (m: LibraryMetadata) => LibraryMetadata) =>
    run(async () => {
      const r = current(), data = library();
      if (!r || !data) return;
      const metadata = change(data.metadata);
      mutate({ ...data, metadata });
      await writeMetadata(r, metadata);
    });
  // Changes made by other programs appear at the next scan: when the window
  // gets focus, and after each of the app's own writes.
  onMount(() => {
    const rescan = () => {
      if (screen().kind === "library" && library.state === "ready") refetch();
    };
    window.addEventListener("focus", rescan);
    onCleanup(() => window.removeEventListener("focus", rescan));
  });

  const inside = (path: readonly string[], dir: readonly string[]) =>
    path.length >= dir.length && dir.every((part, i) => path[i] === part);
  // Renames, moves or trashes the notebook or folder at `path`. Its notes
  // close first, as Write's DocumentList closes the documents it renames
  // (documentlist.cpp:576), and their metadata follows them.
  const relocate = (path: string[], move: (r: FileSystemDirectoryHandle) => Promise<string[] | null>) =>
    run(async () => {
      const r = current(), data = library();
      if (!r || !data) return;
      setTabs(tabs().filter((t) => !inside(t.path, path)));
      const to = await move(r);
      if (to) {
        const metadata = moveNotes(data.metadata, path, to);
        if (JSON.stringify(metadata) !== JSON.stringify(data.metadata)) await writeMetadata(r, metadata);
        if (inside(selected(), path)) setSelected([...to, ...selected().slice(path.length)]);
      } else if (inside(selected(), path)) {
        setSelected([]);
      }
      await refetch();
    });
  const rename = (path: string[], name: string) => relocate(path, (r) => moveEntry(r, path, path.slice(0, -1), name));
  const move = (path: string[], parent: string[]) => relocate(path, (r) => moveEntry(r, path, parent, path[path.length - 1]));
  const trash = (path: string[]) =>
    relocate(path, async (r) => {
      await moveToTrash(r, path);
      return null;
    });
  const thumbnail = (note: Note) => {
    const r = current(), e = engine();
    return r && e ? noteThumbnail(e, r, note) : Promise.resolve(null);
  };

  const folderName = (path: string[]) =>
    library()?.folders.find((f) => pathKey(f.path) === pathKey(path.slice(0, -1)))?.name ?? "";

  return (
    <>
      <Show when={engine.error}>
        <p class="error banner">The engine did not load: {String(engine.error)}</p>
      </Show>
      <Show when={error()}>
        <p class="error banner">{error()}</p>
      </Show>
      <Show
        when={current() && library()}
        fallback={
          <main class="welcome">
            <AppMark />
            <h1>Math Notes</h1>
            <Show when={!start.loading && !current()}>
              <div class="row">
                <Show when={start()?.needsGesture}>
                  <Button class="button" onClick={reconnect}>
                    Reconnect folder
                  </Button>
                </Show>
                <Button class="button primary" onClick={choose}>
                  Choose notes folder
                </Button>
              </div>
            </Show>
          </main>
        }
      >
        {(_) => {
          const data = () => library.latest!;
          const r = () => current()!;
          const sidebar = () => ({
            folders: data().folders,
            metadata: data().metadata,
            section: section(),
            onSection: (s: Section) => {
              setSection(s);
              setScreen({ kind: "library" });
            },
            onMetadata: updateMetadata,
          });
          return (
            <Switch>
              <Match when={screen().kind === "editor" && open()} keyed>
                {(notebook) => (
                  <Editor
                    notebook={notebook}
                    folderName={folderName(notebook.path)}
                    tabs={tabs()}
                    onLibrary={toLibrary}
                    onSelectTab={openPath}
                    onCloseTab={closeTab}
                  />
                )}
              </Match>
              <Match when={screen().kind === "new-notebook"}>
                <NewNotebook
                  {...sidebar()}
                  onCancel={() => setScreen({ kind: "library" })}
                  onCreate={(parent, title) =>
                    run(async () => {
                      const path = await createFolder(r(), parent, title);
                      await refetch();
                      setSection("library");
                      setSelected(path);
                      setScreen({ kind: "library" });
                    })
                  }
                />
              </Match>
              <Match when={screen().kind === "new-note" && (screen() as { folder: string[] }).folder}>
                {(folder) => (
                  <NewNote
                    root={r()}
                    folders={data().folders}
                    templates={data().templates}
                    folder={folder()}
                    onCancel={() => setScreen({ kind: "library" })}
                    onCreate={(parent, title, template) =>
                      run(async () => {
                        const e = engine();
                        if (!e) return;
                        setSelected(parent);
                        showNotebook(await createNotebook(e, r(), parent, title, template));
                      })
                    }
                  />
                )}
              </Match>
              <Match when={true}>
                <Library
                  {...sidebar()}
                  root={r()}
                  trash={data().trash}
                  selected={selected()}
                  onSelect={setSelected}
                  onOpen={(note) => openPath(note.path)}
                  onNewNotebook={() => setScreen({ kind: "new-notebook" })}
                  onNewNote={(folder) => setScreen({ kind: "new-note", folder })}
                  onTrash={trash}
                  onRename={rename}
                  onMove={move}
                  thumbnail={thumbnail}
                  onChooseFolder={choose}
                />
              </Match>
            </Switch>
          );
        }}
      </Show>
    </>
  );
}
