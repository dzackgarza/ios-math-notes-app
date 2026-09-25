// The library screen of docs/specs/tablet-ui.md: sidebar, folder cards with
// sort and grid/list toggle, and the detail pane of the selected folder. The
// sidebar's Search, Recent, Favorites and tag views are views of one table of
// notes (spec, "Relation to the current model", item 5).
import { Button } from "@kobalte/core/button";
import { Dialog } from "@kobalte/core/dialog";
import { DropdownMenu } from "@kobalte/core/dropdown-menu";
import { TextField } from "@kobalte/core/text-field";
import { ToggleGroup } from "@kobalte/core/toggle-group";
import {
  BookOpen,
  ChevronDown,
  Clock,
  Ellipsis,
  FolderOpen,
  LayoutGrid,
  List,
  Plus,
  Search,
  Settings,
  Star,
  Trash2,
} from "lucide-solid";
import { createMemo, createSignal, For, type JSX, Match, Show, Switch } from "solid-js";

import { type Folder, type Note, pathKey } from "../storage/library.ts";
import { emptyNote, type LibraryMetadata, type NoteMetadata, TAG_COLORS } from "../storage/metadata.ts";
import { PaperTile } from "./paper.tsx";

export type Section = "library" | "search" | "recent" | "favorites" | "trash" | "settings" | `tag:${string}`;

export interface LibraryProps {
  root: FileSystemDirectoryHandle;
  folders: Folder[];
  trash: Note[];
  metadata: LibraryMetadata;
  section: Section;
  onSection: (section: Section) => void;
  selected: string[];
  onSelect: (folder: string[]) => void;
  onOpen: (note: Note) => void;
  onNewNotebook: () => void;
  onNewNote: (folder: string[]) => void;
  onMetadata: (change: (metadata: LibraryMetadata) => LibraryMetadata) => void;
  onTrash: (note: Note) => void;
  onChooseFolder: () => void;
}

const relative = new Intl.RelativeTimeFormat("en", { numeric: "auto" });
const UNITS: [Intl.RelativeTimeFormatUnit, number][] = [
  ["year", 365 * 86400e3],
  ["month", 30 * 86400e3],
  ["week", 7 * 86400e3],
  ["day", 86400e3],
  ["hour", 3600e3],
  ["minute", 60e3],
];

// "2 hours ago", "yesterday", "now".
export function ago(time: number): string {
  const elapsed = Date.now() - time;
  for (const [unit, ms] of UNITS) if (elapsed >= ms) return relative.format(-Math.floor(elapsed / ms), unit);
  return relative.format(0, "second");
}

export const noteCount = (n: number) => `${n} ${n === 1 ? "note" : "notes"}`;

export function AppMark() {
  return (
    <div class="app-mark" aria-hidden="true">
      𝒩
    </div>
  );
}

export type SidebarProps = Pick<LibraryProps, "folders" | "metadata" | "section" | "onSection" | "onMetadata">;

export function Sidebar(props: SidebarProps) {
  const notes = () => props.folders.flatMap((f) => f.notes);
  const tagCount = (tag: string) => notes().filter((n) => props.metadata.notes[pathKey(n.path)]?.tags.includes(tag)).length;
  const item = (section: Section, icon: JSX.Element, label: string, count?: number) => (
    <Button class="nav-item" aria-current={props.section === section ? "page" : undefined} onClick={() => props.onSection(section)}>
      {icon}
      <span>{label}</span>
      <Show when={count !== undefined}>
        <span class="nav-count">{count}</span>
      </Show>
    </Button>
  );
  const [adding, setAdding] = createSignal(false);
  const [tagName, setTagName] = createSignal("");
  const addTag = () => {
    const name = tagName().trim();
    if (!name || props.metadata.tags.some((t) => t.name === name)) return;
    props.onMetadata((m) => ({ ...m, tags: [...m.tags, { name, color: TAG_COLORS[m.tags.length % TAG_COLORS.length] }] }));
    setTagName("");
    setAdding(false);
  };
  return (
    <nav class="sidebar" aria-label="Library">
      <div class="brand">
        <AppMark />
        <div class="brand-name">Math Notes</div>
      </div>
      {item("library", <BookOpen size={18} />, "Library")}
      {item("search", <Search size={18} />, "Search")}
      {item("recent", <Clock size={18} />, "Recent")}
      {item("favorites", <Star size={18} />, "Favorites")}
      {item("trash", <Trash2 size={18} />, "Trash")}
      <div class="sidebar-heading">
        <span>Tags</span>
        <Button class="icon-button" aria-label="Add tag" onClick={() => setAdding(true)}>
          <Plus size={16} />
        </Button>
      </div>
      <For each={props.metadata.tags}>
        {(tag) =>
          item(
            `tag:${tag.name}`,
            <span class="tag-dot" style={{ background: tag.color }} />,
            tag.name,
            tagCount(tag.name),
          )
        }
      </For>
      <div class="sidebar-spacer" />
      {item("settings", <Settings size={18} />, "Settings")}
      <Dialog open={adding()} onOpenChange={setAdding}>
        <Dialog.Portal>
          <Dialog.Overlay class="dialog-overlay" />
          <Dialog.Content class="dialog">
            <Dialog.Title class="dialog-title">New Tag</Dialog.Title>
            <form
              onSubmit={(e) => {
                e.preventDefault();
                addTag();
              }}
            >
              <TextField value={tagName()} onChange={setTagName} class="field">
                <TextField.Label class="field-label">Name</TextField.Label>
                <TextField.Input class="input" />
              </TextField>
              <div class="dialog-actions">
                <Dialog.CloseButton class="button">Cancel</Dialog.CloseButton>
                <Button class="button primary" type="submit">
                  Add Tag
                </Button>
              </div>
            </form>
          </Dialog.Content>
        </Dialog.Portal>
      </Dialog>
    </nav>
  );
}

function TagChips(props: { metadata: LibraryMetadata; tags: string[] }) {
  const color = (name: string) => props.metadata.tags.find((t) => t.name === name)?.color ?? "#8A8F98";
  return (
    <div class="chips">
      <For each={props.tags}>
        {(tag) => (
          <span class="chip" style={{ "--chip": color(tag) }}>
            {tag}
          </span>
        )}
      </For>
    </div>
  );
}

// The ⋯ menu of a note: favorite, tags, move to the trash.
function NoteMenu(props: { note: Note; metadata: LibraryProps["metadata"]; onMetadata: LibraryProps["onMetadata"]; onTrash: (note: Note) => void }) {
  const key = () => pathKey(props.note.path);
  const meta = (): NoteMetadata => props.metadata.notes[key()] ?? emptyNote();
  const change = (update: (note: NoteMetadata) => NoteMetadata) =>
    props.onMetadata((m) => ({ ...m, notes: { ...m.notes, [key()]: update(m.notes[key()] ?? emptyNote()) } }));
  return (
    <DropdownMenu>
      <DropdownMenu.Trigger class="icon-button" aria-label={`${props.note.name} actions`} onClick={(e: MouseEvent) => e.stopPropagation()}>
        <Ellipsis size={18} />
      </DropdownMenu.Trigger>
      <DropdownMenu.Portal>
        <DropdownMenu.Content class="menu">
          <DropdownMenu.CheckboxItem
            class="menu-item"
            checked={meta().favorite}
            onChange={(favorite) => change((n) => ({ ...n, favorite }))}
          >
            Favorite
          </DropdownMenu.CheckboxItem>
          <Show when={props.metadata.tags.length > 0}>
            <DropdownMenu.Sub>
              <DropdownMenu.SubTrigger class="menu-item">Tags</DropdownMenu.SubTrigger>
              <DropdownMenu.Portal>
                <DropdownMenu.SubContent class="menu">
                  <For each={props.metadata.tags}>
                    {(tag) => (
                      <DropdownMenu.CheckboxItem
                        class="menu-item"
                        checked={meta().tags.includes(tag.name)}
                        onChange={(on) =>
                          change((n) => ({ ...n, tags: on ? [...n.tags, tag.name] : n.tags.filter((t) => t !== tag.name) }))
                        }
                      >
                        <span class="tag-dot" style={{ background: tag.color }} /> {tag.name}
                      </DropdownMenu.CheckboxItem>
                    )}
                  </For>
                </DropdownMenu.SubContent>
              </DropdownMenu.Portal>
            </DropdownMenu.Sub>
          </Show>
          <Show when={props.note.path[0] !== ".trash"}>
            <DropdownMenu.Item class="menu-item danger" onSelect={() => props.onTrash(props.note)}>
              Move to Trash
            </DropdownMenu.Item>
          </Show>
        </DropdownMenu.Content>
      </DropdownMenu.Portal>
    </DropdownMenu>
  );
}

function NoteRow(props: { note: Note; subtitle: string; library: LibraryProps }) {
  const meta = () => props.library.metadata.notes[pathKey(props.note.path)];
  return (
    <li class="note-row">
      <Button class="note-open" onClick={() => props.library.onOpen(props.note)}>
        <PaperTile root={props.library.root} template={props.note.template} class="note-thumb" />
        <span class="note-text">
          <span class="note-title">
            {props.note.name}
            <Show when={meta()?.favorite}>
              <Star size={13} class="favorite-mark" aria-label="Favorite" />
            </Show>
          </span>
          <span class="note-summary">{meta()?.description || props.subtitle}</span>
          <span class="note-time">{ago(props.note.modified)}</span>
        </span>
      </Button>
      <NoteMenu note={props.note} metadata={props.library.metadata} onMetadata={props.library.onMetadata} onTrash={props.library.onTrash} />
    </li>
  );
}

type Sort = "modified" | "name";

function FolderCards(props: LibraryProps) {
  const [query, setQuery] = createSignal("");
  const [sort, setSort] = createSignal<Sort>("modified");
  const [layout, setLayout] = createSignal<"grid" | "list">("grid");
  const shown = createMemo(() => {
    const q = query().trim().toLowerCase();
    // "My Notes" shows only when notes sit at the top level.
    const folders = props.folders.filter((f) => (f.path.length > 0 || f.notes.length > 0) && f.name.toLowerCase().includes(q));
    return sort() === "name" ? folders.sort((a, b) => a.name.localeCompare(b.name)) : folders.sort((a, b) => b.modified - a.modified);
  });
  const folderTags = (folder: Folder) => [
    ...new Set(folder.notes.flatMap((n) => props.metadata.notes[pathKey(n.path)]?.tags ?? [])),
  ];
  return (
    <>
      <div class="filters">
        <TextField value={query()} onChange={setQuery} class="search">
          <Search size={16} />
          <TextField.Input class="search-input" placeholder="Search notebooks…" aria-label="Search notebooks" />
        </TextField>
        <DropdownMenu>
          <DropdownMenu.Trigger class="select-button" aria-label="Sort">
            {sort() === "name" ? "Name" : "Last Modified"} <ChevronDown size={14} />
          </DropdownMenu.Trigger>
          <DropdownMenu.Portal>
            <DropdownMenu.Content class="menu">
              <DropdownMenu.RadioGroup value={sort()} onChange={(v) => setSort(v as Sort)}>
                <DropdownMenu.RadioItem class="menu-item" value="modified">
                  Last Modified
                </DropdownMenu.RadioItem>
                <DropdownMenu.RadioItem class="menu-item" value="name">
                  Name
                </DropdownMenu.RadioItem>
              </DropdownMenu.RadioGroup>
            </DropdownMenu.Content>
          </DropdownMenu.Portal>
        </DropdownMenu>
        <ToggleGroup class="segmented" value={layout()} onChange={(v) => v && setLayout(v as "grid" | "list")} aria-label="Layout">
          <ToggleGroup.Item class="segment" value="grid" aria-label="Grid">
            <LayoutGrid size={16} />
          </ToggleGroup.Item>
          <ToggleGroup.Item class="segment" value="list" aria-label="List">
            <List size={16} />
          </ToggleGroup.Item>
        </ToggleGroup>
      </div>
      <ul class={layout() === "grid" ? "cards" : "cards list"} aria-label="Notebooks">
        <For each={shown()}>
          {(folder) => (
            <li class="card" aria-current={pathKey(props.selected) === pathKey(folder.path) ? "true" : undefined}>
              <Button class="card-open" aria-label={folder.name} onClick={() => props.onSelect(folder.path)}>
                <Show when={folder.notes[0]} fallback={<div class="paper-tile card-cover empty-cover" />}>
                  {(first) => <PaperTile root={props.root} template={first().template} class="card-cover" />}
                </Show>
                <span class="card-text">
                  <span class="card-title">{folder.name}</span>
                  <span class="card-meta">{noteCount(folder.notes.length)}</span>
                  <Show when={folder.modified > 0}>
                    <span class="card-meta">Modified {ago(folder.modified)}</span>
                  </Show>
                </span>
              </Button>
              <TagChips metadata={props.metadata} tags={folderTags(folder)} />
            </li>
          )}
        </For>
      </ul>
    </>
  );
}

function DetailPane(props: LibraryProps) {
  const folder = () => props.folders.find((f) => pathKey(f.path) === pathKey(props.selected));
  const [query, setQuery] = createSignal("");
  return (
    <Show when={folder()}>
      {(f) => (
        <aside class="detail" aria-label={`${f().name} notes`}>
          <Show when={f().notes[0]} fallback={<div class="paper-tile detail-cover empty-cover" />}>
            {(first) => <PaperTile root={props.root} template={first().template} class="detail-cover" />}
          </Show>
          <h2 class="detail-title">{f().name}</h2>
          <div class="detail-meta">
            {noteCount(f().notes.length)}
            <Show when={f().modified > 0}> · Modified {ago(f().modified)}</Show>
          </div>
          <h3 class="detail-tab">Notes</h3>
          <TextField value={query()} onChange={setQuery} class="search">
            <Search size={16} />
            <TextField.Input class="search-input" placeholder="Search notes…" aria-label="Search notes" />
          </TextField>
          <ul class="note-list" aria-label="Notes">
            <For each={f().notes.filter((n) => n.name.toLowerCase().includes(query().trim().toLowerCase()))}>
              {(note) => <NoteRow note={note} subtitle={f().name} library={props} />}
            </For>
          </ul>
          <Button class="button new-in" onClick={() => props.onNewNote(f().path)}>
            <Plus size={16} /> New Note in {f().name}
          </Button>
        </aside>
      )}
    </Show>
  );
}

// Search, Recent, Favorites, a tag, or the trash: one list of notes.
function NoteTable(props: { library: LibraryProps; title: string; notes: Note[]; search?: boolean }) {
  const [query, setQuery] = createSignal("");
  const folderName = (note: Note) =>
    props.library.folders.find((f) => f.notes.includes(note))?.name ?? (note.path[0] === ".trash" ? "Trash" : "");
  const shown = () => props.notes.filter((n) => n.name.toLowerCase().includes(query().trim().toLowerCase()));
  return (
    <>
      <Show when={props.search}>
        <div class="filters">
          <TextField value={query()} onChange={setQuery} class="search wide">
            <Search size={16} />
            <TextField.Input class="search-input" placeholder="Search notes by title…" aria-label="Search notes by title" autofocus />
          </TextField>
        </div>
      </Show>
      <ul class="note-list table" aria-label={props.title}>
        <For each={shown()} fallback={<li class="empty">No notes.</li>}>
          {(note) => <NoteRow note={note} subtitle={folderName(note)} library={props.library} />}
        </For>
      </ul>
    </>
  );
}

export function Library(props: LibraryProps) {
  const notes = () => props.folders.flatMap((f) => f.notes);
  const meta = (n: Note) => props.metadata.notes[pathKey(n.path)];
  const tag = () => (props.section.startsWith("tag:") ? props.section.slice(4) : "");
  const title = (): string => {
    const titles: Record<string, string> = {
      library: "Library",
      search: "Search",
      recent: "Recent",
      favorites: "Favorites",
      trash: "Trash",
      settings: "Settings",
    };
    return titles[props.section] ?? tag();
  };
  return (
    <div class="library">
      <Sidebar {...props} />
      <main class="main">
        <header class="main-header">
          <div>
            <h1>{title()}</h1>
            <Show when={props.section === "library"}>
              <p class="lede">A collection of mathematical notebooks.</p>
            </Show>
          </div>
          <div class="actions">
            <Button class="button" onClick={() => props.onNewNotebook()}>
              <Plus size={16} /> New Notebook
            </Button>
            <Button class="button primary" onClick={() => props.onNewNote(props.selected)}>
              <Plus size={16} /> New Note
            </Button>
          </div>
        </header>
        <Switch>
          <Match when={props.section === "library"}>
            <FolderCards {...props} />
          </Match>
          <Match when={props.section === "search"}>
            <NoteTable library={props} title="Search results" notes={notes()} search />
          </Match>
          <Match when={props.section === "recent"}>
            <NoteTable library={props} title="Recent notes" notes={[...notes()].sort((a, b) => b.modified - a.modified)} />
          </Match>
          <Match when={props.section === "favorites"}>
            <NoteTable library={props} title="Favorite notes" notes={notes().filter((n) => meta(n)?.favorite)} />
          </Match>
          <Match when={props.section === "trash"}>
            <NoteTable library={props} title="Trashed notes" notes={props.trash} />
          </Match>
          <Match when={props.section === "settings"}>
            <section class="settings">
              <h2>Notes folder</h2>
              <p class="lede">{props.root.name}</p>
              <Button class="button" onClick={() => props.onChooseFolder()}>
                <FolderOpen size={16} /> Choose notes folder
              </Button>
            </section>
          </Match>
          <Match when={tag()}>
            <NoteTable library={props} title={`Notes tagged ${tag()}`} notes={notes().filter((n) => meta(n)?.tags.includes(tag()))} />
          </Match>
        </Switch>
      </main>
      <Show when={props.section === "library"}>
        <DetailPane {...props} />
      </Show>
    </div>
  );
}
