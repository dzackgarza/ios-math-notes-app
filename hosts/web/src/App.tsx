import { Button } from "@kobalte/core/button";
import { TextField } from "@kobalte/core/text-field";
import { For, Match, Show, Switch, createResource, createSignal } from "solid-js";

import type { Engine } from "./engine/engine.ts";
import { loadEngine } from "./engine/load.ts";
import { Editor } from "./editor/Editor.tsx";
import { createNotebook, openNotebook, type OpenNotebook } from "./editor/notebook.ts";
import { hasPermission, listNotebooks, pickRoot, requestPermission, savedRoot } from "./storage/folder.ts";

// `?root=opfs` uses the origin-private file system as the notes folder: the
// automated tests cannot drive the native folder picker.
async function initialRoot(): Promise<{ root?: FileSystemDirectoryHandle; needsGesture: boolean }> {
  if (new URLSearchParams(location.search).get("root") === "opfs") {
    window.mathNotesWrites = [];
    return { root: await navigator.storage.getDirectory(), needsGesture: false };
  }
  const root = await savedRoot();
  if (!root) return { needsGesture: false };
  return (await hasPermission(root)) ? { root, needsGesture: false } : { root, needsGesture: true };
}

export function App() {
  const [engine] = createResource<Engine>(loadEngine);
  const [start] = createResource(initialRoot);
  const [root, setRoot] = createSignal<FileSystemDirectoryHandle>();
  const [error, setError] = createSignal("");
  const [open, setOpen] = createSignal<OpenNotebook>();

  const current = () => root() ?? (start()?.needsGesture ? undefined : start()?.root);
  const [notebooks, { refetch }] = createResource(current, listNotebooks);

  const run = (action: () => Promise<void>) => () => {
    setError("");
    action().catch((e: unknown) => setError(e instanceof Error ? e.message : String(e)));
  };
  const choose = run(async () => {
    setRoot(await pickRoot());
  });
  const reconnect = run(async () => {
    const saved = start()?.root;
    if (saved && (await requestPermission(saved))) setRoot(saved);
  });
  const [name, setName] = createSignal("");
  const create = run(async () => {
    const dir = current();
    const e = engine();
    if (!dir || !e || !name().trim()) return;
    setOpen(await createNotebook(e, dir, name().trim()));
    setName("");
    refetch();
  });
  const openByName = (notebook: string) =>
    run(async () => {
      const dir = current();
      const e = engine();
      if (dir && e) setOpen(await openNotebook(e, dir, notebook));
    });

  return (
    <Switch>
      <Match when={open()}>
        {(notebook) => <Editor notebook={notebook()} onClose={() => setOpen(undefined)} />}
      </Match>
      <Match when={true}>
        <main class="library">
          <h1>Math Notes</h1>
          <Show when={engine.error}>
            <p class="error">The engine did not load: {String(engine.error)}</p>
          </Show>
          <Show when={error()}>
            <p class="error">{error()}</p>
          </Show>
          <Show
            when={current()}
            fallback={
              <div class="row">
                <Show when={start()?.needsGesture}>
                  <Button class="button" onClick={reconnect}>
                    Reconnect folder
                  </Button>
                </Show>
                <Button class="button" onClick={choose}>
                  Choose notes folder
                </Button>
              </div>
            }
          >
            <ul>
              <For each={notebooks()}>
                {(notebook) => (
                  <li>
                    <Button class="button" onClick={openByName(notebook)}>
                      {notebook}
                    </Button>
                  </li>
                )}
              </For>
            </ul>
            <form
              class="row"
              onSubmit={(e) => {
                e.preventDefault();
                create();
              }}
            >
              <TextField value={name()} onChange={setName}>
                <TextField.Input class="button" placeholder="New notebook" aria-label="New notebook name" />
              </TextField>
              <Button class="button" type="submit" disabled={!engine()}>
                Create
              </Button>
            </form>
          </Show>
        </main>
      </Match>
    </Switch>
  );
}
