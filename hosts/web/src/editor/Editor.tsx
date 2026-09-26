import { Button } from "@kobalte/core/button";
import { DropdownMenu } from "@kobalte/core/dropdown-menu";
import { ToggleGroup } from "@kobalte/core/toggle-group";
import {
  ChevronDown,
  ChevronLeft,
  ChevronRight,
  Copy,
  CopyPlus,
  Ellipsis,
  Eraser as EraserIcon,
  Grip,
  Highlighter,
  Lasso,
  PenLine,
  Plus,
  Redo2,
  Scissors,
  Trash2,
  Undo2,
  X,
} from "lucide-solid";
import { For, Match, Show, Switch, createEffect, createResource, createSignal, onCleanup, onMount } from "solid-js";

import { Brush, Eraser, PageSize, Selector, type Canvas, type SelectionInfo } from "../engine/engine.ts";
import { ViewController, type View } from "../input/gestures.ts";
import { browserEngine, capabilities, penSamples } from "../input/pointer.ts";
import { listTemplates } from "../storage/folder.ts";
import { AppMark } from "../ui/Library.tsx";
import { paperLabel } from "../ui/paper.tsx";
import { applyTemplate, type OpenNotebook } from "./notebook.ts";

// How far past the last page, in CSS px, a pull must go to add a page.
const PULL_THRESHOLD = 96;
const WHEEL_RELEASE_MS = 250;

// The tool rail's pens (docs/specs/tablet-ui.md, Editor): brush and size in pt.
const PENS = {
  pen: { label: "Pen", brush: Brush.pressurePen, size: 1.6, rgb: 0x1a1a1a },
  highlighter: { label: "Highlighter", brush: Brush.highlighter, size: 8, rgb: 0xf5d547 },
} as const;
type PenId = keyof typeof PENS;
type ToolId = PenId | "eraser" | "select";

// The eraser's two kinds (#23); the pen's eraser end uses the selected one.
const ERASERS = { stroke: { label: "Whole stroke", kind: Eraser.stroke }, free: { label: "Partial", kind: Eraser.free } } as const;
type EraserId = keyof typeof ERASERS;

// The selection tool's two kinds (#24).
const SELECTORS = { lasso: { label: "Freeform", kind: Selector.lasso }, rect: { label: "Rectangle", kind: Selector.rect } } as const;
type SelectorId = keyof typeof SELECTORS;

// The 15 swatches of the mockup's palette, three per row.
export const PALETTE = [
  0x1a1a1a, 0x8a8f98, 0xffffff, 0x1f4fd1, 0xd6455d, 0xf08a24, 0x3fa35b, 0x8b5cf6, 0xf5a3c7, 0xf5d547, 0x2bb3c0, 0xd8b4fe,
  0x7fb2f0, 0x8b5a2b, 0x1f3a93,
];
const hex = (rgb: number) => `#${rgb.toString(16).padStart(6, "0").toUpperCase()}`;

// Zoom factors relative to the page filling the canvas width (100%).
const ZOOMS = [0.5, 0.75, 1, 1.25, 1.5, 2, 3];

export interface Tab {
  path: string[];
  name: string;
}

export function Editor(props: {
  notebook: OpenNotebook;
  folderName: string;
  tabs: Tab[];
  // Each runs after the editor saved and freed the open notebook.
  onLibrary: () => void;
  onSelectTab: (path: string[]) => void;
  onCloseTab: (path: string[]) => void;
}) {
  let area!: HTMLDivElement;
  let element!: HTMLCanvasElement;
  let canvas: Canvas | undefined;
  let frame = 0;
  const ids = { next: 0 };
  const engineName = browserEngine();
  const { document: doc, saver, root } = props.notebook;
  const [templates] = createResource(() => listTemplates(root));
  const [template, setTemplate] = createSignal(props.notebook.template);
  const [tool, setTool] = createSignal<ToolId>("pen");
  // The pen the palette colors: the selected pen, or the last one before the eraser.
  const [pen, setPen] = createSignal<PenId>("pen");
  const [eraser, setEraser] = createSignal<EraserId>("stroke");
  const [selector, setSelector] = createSignal<SelectorId>("lasso");
  const [selection, setSelection] = createSignal<SelectionInfo | null>(null);
  const selectTool = (id: ToolId) => {
    setTool(id);
    if (id !== "eraser" && id !== "select") setPen(id);
  };
  const [colors, setColors] = createSignal<Record<PenId, number>>({ pen: PENS.pen.rgb, highlighter: PENS.highlighter.rgb });
  const [view, setView] = createSignal<View>({ scale: 1, x: 0, y: 0 });
  const [pages, setPages] = createSignal(doc.pageCount());
  // How far, in CSS px, the view is pulled past the end of the last page.
  const [pull, setPull] = createSignal(0);

  // The view stops at the ends of the pages (docs/specs/tablet-ui.md, "Pages
  // in the editor"); content narrower or shorter than the canvas is centered.
  const clampView = (view: View): View => {
    const content = doc.contentSize();
    const width = content.width * view.scale, height = content.height * view.scale;
    const clampAxis = (at: number, size: number, viewport: number) =>
      size <= viewport ? (viewport - size) / 2 : Math.min(0, Math.max(viewport - size, at));
    return {
      scale: view.scale,
      x: clampAxis(view.x, width, element.clientWidth),
      y: clampAxis(view.y, height, element.clientHeight),
    };
  };
  const controller = new ViewController(
    { scale: 1, x: 0, y: 0 },
    (view) => {
      const clamped = clampView(view);
      // Movement past the end of the last page goes into the pull; moving
      // back takes it out before the view scrolls.
      const end = clampView({ ...view, y: -Infinity }).y;
      if (pull() > 0 || view.y < end) {
        const next = Math.max(0, pull() + end - view.y);
        setPull(next);
        if (next > 0) clamped.y = end;
      }
      controller.view = clamped;
      canvas?.setView(clamped.scale, 0, 0, clamped.scale, clamped.x, clamped.y);
      setView(clamped);
      setPages(doc.pageCount());
      refreshSelection();
    },
    () => releasePull(),
  );

  // Releasing past the threshold adds a page after the last one; the pull
  // springs back either way.
  const releasePull = () => {
    const add = pull() >= PULL_THRESHOLD;
    setPull(0);
    if (add) edit(() => doc.insertPage(doc.pageCount()));
  };

  const fitScale = () => element.clientWidth / doc.contentSize().width;

  const fitWidth = () => controller.set({ scale: fitScale(), x: 0, y: 0 });

  // Zooms about the top left of the view, keeping the page at the top in place.
  const zoom = (factor: number) => {
    const { scale, y } = controller.view;
    const next = fitScale() * factor;
    controller.set({ scale: next, x: 0, y: (y * next) / scale });
  };

  const resize = () => {
    if (!canvas) return;
    const ratio = window.devicePixelRatio;
    element.width = Math.round(element.clientWidth * ratio);
    element.height = Math.round(element.clientHeight * ratio);
    canvas.setSurfaceSize(element.width, element.height, ratio);
    controller.set(controller.view);
  };

  const loop = () => {
    canvas?.render();
    frame = requestAnimationFrame(loop);
  };

  const origin = () => {
    const rect = element.getBoundingClientRect();
    return { x: rect.left, y: rect.top };
  };

  const onPointer = (e: PointerEvent) => {
    if (!canvas) return;
    const at = origin();
    if (controller.pointer(e, at)) {
      if (e.type === "pointerdown") element.setPointerCapture(e.pointerId);
      return;
    }
    if (e.type === "pointerdown") {
      element.setPointerCapture(e.pointerId);
      setSelection(null); // the actions return where the gesture leaves the selection
    }
    canvas.input(penSamples(e, at, capabilities(engineName, e.pointerType), ids));
    if (e.type === "pointerup" || e.type === "pointercancel") {
      refreshSelection();
      saver.schedule();
    }
  };

  const refreshSelection = () => setSelection(canvas?.selection() ?? null);

  // A wheel or trackpad scroll has no release event: the pull is released
  // when no wheel event has come for WHEEL_RELEASE_MS.
  let wheelRelease = 0;
  const onWheel = (e: WheelEvent) => {
    e.preventDefault();
    controller.wheel(e, origin());
    clearTimeout(wheelRelease);
    wheelRelease = window.setTimeout(releasePull, WHEEL_RELEASE_MS);
  };

  // The page at the middle of the view; the last page below the pages.
  const currentPage = () => {
    view();
    pages();
    if (!canvas) return 0;
    const page = canvas.pageAt(element.clientWidth / 2, element.clientHeight / 2);
    const count = doc.pageCount();
    return page < 0 || page >= count ? count - 1 : page;
  };

  const edit = (change: () => void) => {
    change();
    controller.set(controller.view);
    saver.schedule();
  };

  // The system clipboard holds the selection as SVG text (#24).
  const copy = (cut: boolean) => {
    if (!canvas) return;
    const svg = canvas.copySelection(cut);
    if (!svg) return;
    void navigator.clipboard.writeText(svg);
    if (cut) edit(() => {});
  };
  // Pastes at the middle of the view.
  const paste = (svg: string) => {
    const target = canvas;
    if (!target || !svg) return;
    edit(() => target.paste(svg, element.clientWidth / 2, element.clientHeight / 2));
  };
  // Ctrl+V: the paste event carries the clipboard text without a permission prompt.
  const onPaste = (e: ClipboardEvent) => {
    const text = e.clipboardData?.getData("text/plain");
    if (!text) return;
    e.preventDefault();
    paste(text);
  };

  // Scrolls page `index` into view when it is not already visible.
  const showPage = (index: number) => {
    if (index < 0) return;
    const rect = doc.pageRect(index);
    const { scale, x, y } = controller.view;
    const top = y + rect.y * scale, bottom = top + rect.height * scale;
    if (bottom > 0 && top < element.clientHeight) return;
    controller.set({ scale, x, y: -rect.y * scale });
  };

  // Puts the top of page `index` at the top of the view.
  const goToPage = (index: number) => {
    if (index < 0 || index >= doc.pageCount()) return;
    const { scale, x } = controller.view;
    controller.set({ scale, x, y: -doc.pageRect(index).y * scale });
  };

  const history = (step: "undo" | "redo") => {
    const moved = step === "undo" ? doc.undo() : doc.redo();
    if (!moved) return;
    edit(() => showPage(moved.page));
  };

  const onKey = (e: KeyboardEvent) => {
    if (e.target instanceof HTMLInputElement) return;
    const key = e.key.toLowerCase();
    if ((key === "delete" || key === "backspace") && selection()) {
      e.preventDefault();
      edit(() => canvas?.deleteSelection());
      return;
    }
    if (key === "escape") {
      canvas?.clearSelection();
      refreshSelection();
      return;
    }
    if (!(e.ctrlKey || e.metaKey)) return;
    const commands: Record<string, () => void> = {
      z: () => history(e.shiftKey ? "redo" : "undo"),
      a: () => {
        canvas?.selectAll(currentPage());
        refreshSelection();
      },
      c: () => copy(false),
      x: () => copy(true),
      d: () => edit(() => canvas?.duplicateSelection()),
    };
    if (!commands[key]) return;
    e.preventDefault();
    commands[key]();
  };

  onMount(() => {
    window.addEventListener("keydown", onKey);
    window.addEventListener("paste", onPaste);
    onCleanup(() => {
      window.removeEventListener("keydown", onKey);
      window.removeEventListener("paste", onPaste);
    });
    canvas = doc.createCanvas("#ink-canvas");
    canvas.setUtcOffset(performance.timeOrigin);
    createEffect(() => {
      const { brush, size } = PENS[pen()];
      canvas?.setTool({ brush, rgb: colors()[pen()], size });
    });
    createEffect(() => canvas?.setEraser(ERASERS[eraser()].kind, tool() === "eraser"));
    createEffect(() => canvas?.setSelector(SELECTORS[selector()].kind, tool() === "select"));
    const observer = new ResizeObserver(resize);
    observer.observe(area);
    resize();
    fitWidth();
    frame = requestAnimationFrame(loop);
    onCleanup(() => {
      observer.disconnect();
      cancelAnimationFrame(frame);
    });
  });

  // Saves and frees the notebook, then `next` moves to another screen.
  const leave = async (next: () => void) => {
    await saver.save();
    canvas?.free();
    canvas = undefined;
    doc.free();
    next();
  };

  const isOpen = (path: string[]) => path.join("/") === props.notebook.path.join("/");
  const zoomLabel = () => (element ? `${Math.round((view().scale / fitScale()) * 100)}%` : "100%");

  return (
    <div class="editor">
      <header class="editor-top">
        <Button class="link-button" aria-label="Library" onClick={() => void leave(props.onLibrary)}>
          <ChevronLeft size={18} />
          <AppMark />
        </Button>
        <div class="editor-title">
          <div class="editor-folder">{props.folderName}</div>
          <div class="editor-note">{props.notebook.name}</div>
        </div>
        <div class="tabs" role="tablist" aria-label="Open notes">
          <For each={props.tabs}>
            {(tab) => (
              <div class="tab" aria-current={isOpen(tab.path) ? "page" : undefined}>
                <Button
                  role="tab"
                  aria-selected={isOpen(tab.path)}
                  class="tab-label"
                  onClick={() => !isOpen(tab.path) && void leave(() => props.onSelectTab(tab.path))}
                >
                  {tab.name}
                </Button>
                <Button
                  class="icon-button"
                  aria-label={`Close ${tab.name}`}
                  onClick={() => (isOpen(tab.path) ? void leave(() => props.onCloseTab(tab.path)) : props.onCloseTab(tab.path))}
                >
                  <X size={14} />
                </Button>
              </div>
            )}
          </For>
        </div>
        <DropdownMenu>
          <DropdownMenu.Trigger class="icon-button" aria-label="Page actions">
            <Ellipsis size={20} />
          </DropdownMenu.Trigger>
          <DropdownMenu.Portal>
            <DropdownMenu.Content class="menu">
              <DropdownMenu.Item class="menu-item" onSelect={() => void navigator.clipboard.readText().then(paste)}>
                Paste
              </DropdownMenu.Item>
              <DropdownMenu.Item class="menu-item" onSelect={() => edit(() => doc.insertPage(currentPage()))}>
                Insert page before
              </DropdownMenu.Item>
              <DropdownMenu.Item class="menu-item" onSelect={() => edit(() => doc.insertPage(currentPage() + 1))}>
                Insert page after
              </DropdownMenu.Item>
              <DropdownMenu.Item
                class="menu-item"
                onSelect={() => edit(() => doc.pageCount() > 1 && doc.deletePage(currentPage()))}
              >
                Delete page
              </DropdownMenu.Item>
              <DropdownMenu.Item
                class="menu-item"
                onSelect={() => edit(() => currentPage() > 0 && doc.movePage(currentPage(), currentPage() - 1))}
              >
                Move page up
              </DropdownMenu.Item>
              <DropdownMenu.Item
                class="menu-item"
                onSelect={() =>
                  edit(() => currentPage() < doc.pageCount() - 1 && doc.movePage(currentPage(), currentPage() + 1))
                }
              >
                Move page down
              </DropdownMenu.Item>
              <DropdownMenu.Sub>
                <DropdownMenu.SubTrigger class="menu-item">Page size</DropdownMenu.SubTrigger>
                <DropdownMenu.Portal>
                  <DropdownMenu.SubContent class="menu">
                    <DropdownMenu.Item class="menu-item" onSelect={() => edit(() => doc.setPageSize(PageSize.a4))}>
                      A4
                    </DropdownMenu.Item>
                    <DropdownMenu.Item class="menu-item" onSelect={() => edit(() => doc.setPageSize(PageSize.letter))}>
                      Letter
                    </DropdownMenu.Item>
                  </DropdownMenu.SubContent>
                </DropdownMenu.Portal>
              </DropdownMenu.Sub>
            </DropdownMenu.Content>
          </DropdownMenu.Portal>
        </DropdownMenu>
      </header>
      <div class="editor-body">
        <aside class="tool-rail" aria-label="Tools">
          <ToggleGroup class="tools" value={tool()} onChange={(v) => v && selectTool(v as ToolId)} aria-label="Pens">
            <For each={Object.keys(PENS) as PenId[]}>
              {(id) => (
                <ToggleGroup.Item class="tool" value={id} aria-label={PENS[id].label}>
                  {id === "pen" ? <PenLine size={20} /> : <Highlighter size={20} />}
                  <span class="tool-text">
                    <span>{PENS[id].label}</span>
                    <span class="tool-size">{PENS[id].size}</span>
                  </span>
                </ToggleGroup.Item>
              )}
            </For>
            <ToggleGroup.Item class="tool" value="eraser" aria-label="Eraser">
              <EraserIcon size={20} />
              <span class="tool-text">
                <span>Eraser</span>
                <span class="tool-size">{ERASERS[eraser()].label}</span>
              </span>
            </ToggleGroup.Item>
            <ToggleGroup.Item class="tool" value="select" aria-label="Lasso">
              <Lasso size={20} />
              <span class="tool-text">
                <span>Lasso</span>
                <span class="tool-size">{SELECTORS[selector()].label}</span>
              </span>
            </ToggleGroup.Item>
          </ToggleGroup>
          <Switch
            fallback={
              <ToggleGroup
                class="palette"
                value={hex(colors()[pen()])}
                onChange={(v) => v && setColors((c) => ({ ...c, [pen()]: parseInt(v.slice(1), 16) }))}
                aria-label="Colors"
              >
                <For each={PALETTE}>
                  {(rgb) => (
                    <ToggleGroup.Item class="swatch" value={hex(rgb)} aria-label={hex(rgb)} style={{ background: hex(rgb) }} />
                  )}
                </For>
              </ToggleGroup>
            }
          >
            <Match when={tool() === "eraser"}>
              <ToggleGroup
                class="tools eraser-kinds"
                value={eraser()}
                onChange={(v) => v && setEraser(v as EraserId)}
                aria-label="Eraser"
              >
                <For each={Object.keys(ERASERS) as EraserId[]}>
                  {(id) => (
                    <ToggleGroup.Item class="tool" value={id}>
                      {ERASERS[id].label}
                    </ToggleGroup.Item>
                  )}
                </For>
              </ToggleGroup>
            </Match>
            <Match when={tool() === "select"}>
              <ToggleGroup
                class="tools"
                value={selector()}
                onChange={(v) => v && setSelector(v as SelectorId)}
                aria-label="Selection"
              >
                <For each={Object.keys(SELECTORS) as SelectorId[]}>
                  {(id) => (
                    <ToggleGroup.Item class="tool" value={id}>
                      {SELECTORS[id].label}
                    </ToggleGroup.Item>
                  )}
                </For>
              </ToggleGroup>
            </Match>
          </Switch>
        </aside>
        <div class="canvas-area" ref={area}>
          <canvas
            id="ink-canvas"
            ref={element}
            onPointerDown={onPointer}
            onPointerMove={onPointer}
            onPointerUp={onPointer}
            onPointerCancel={onPointer}
            onWheel={onWheel}
            onContextMenu={(e) => e.preventDefault()}
          />
          <Show when={selection()}>
            {(sel) => (
              <div
                class="selection-bar bar-group"
                role="toolbar"
                aria-label="Selection actions"
                style={{ left: `${sel().x + sel().width / 2}px`, top: `${sel().y + sel().height + 12}px` }}
              >
                <Button class="icon-button" aria-label="Copy" title="Copy (Ctrl+C)" onClick={() => copy(false)}>
                  <Copy size={18} />
                </Button>
                <Button class="icon-button" aria-label="Cut" title="Cut (Ctrl+X)" onClick={() => copy(true)}>
                  <Scissors size={18} />
                </Button>
                <Button
                  class="icon-button"
                  aria-label="Duplicate"
                  title="Duplicate (Ctrl+D)"
                  onClick={() => edit(() => canvas?.duplicateSelection())}
                >
                  <CopyPlus size={18} />
                </Button>
                <Button
                  class="icon-button"
                  aria-label="Delete"
                  title="Delete (Del)"
                  onClick={() => edit(() => canvas?.deleteSelection())}
                >
                  <Trash2 size={18} />
                </Button>
              </div>
            )}
          </Show>
          <div
            class="pull-indicator"
            data-active={pull() > 0 ? "" : undefined}
            data-ready={pull() >= PULL_THRESHOLD ? "" : undefined}
            style={{ height: `${Math.min(pull(), 1.5 * PULL_THRESHOLD)}px` }}
          >
            <Plus size={16} />
            {pull() >= PULL_THRESHOLD ? "Release to add a page" : "Pull to add a page"}
          </div>
          <div class="bottom-bar">
            <div class="bar-group">
              <Button class="icon-button" aria-label="Undo" title="Undo (Ctrl+Z)" onClick={() => history("undo")}>
                <Undo2 size={18} />
              </Button>
              <Button class="icon-button" aria-label="Redo" title="Redo (Shift+Ctrl+Z)" onClick={() => history("redo")}>
                <Redo2 size={18} />
              </Button>
            </div>
            <DropdownMenu>
              <DropdownMenu.Trigger class="bar-group bar-menu" aria-label="Zoom">
                {zoomLabel()} <ChevronDown size={14} />
              </DropdownMenu.Trigger>
              <DropdownMenu.Portal>
                <DropdownMenu.Content class="menu">
                  <DropdownMenu.Item class="menu-item" onSelect={fitWidth}>
                    Fit width
                  </DropdownMenu.Item>
                  <For each={ZOOMS}>
                    {(factor) => (
                      <DropdownMenu.Item class="menu-item" onSelect={() => zoom(factor)}>
                        {factor * 100}%
                      </DropdownMenu.Item>
                    )}
                  </For>
                </DropdownMenu.Content>
              </DropdownMenu.Portal>
            </DropdownMenu>
            <DropdownMenu>
              <DropdownMenu.Trigger class="bar-group bar-menu" aria-label="Paper">
                <Grip size={16} /> {paperLabel(template())} <ChevronDown size={14} />
              </DropdownMenu.Trigger>
              <DropdownMenu.Portal>
                <DropdownMenu.Content class="menu">
                  <DropdownMenu.RadioGroup
                    value={template()}
                    onChange={(name) =>
                      void applyTemplate(root, doc, name).then(() => {
                        setTemplate(name);
                        edit(() => {});
                      })
                    }
                  >
                    <For each={templates()}>
                      {(name) => (
                        <DropdownMenu.RadioItem class="menu-item" value={name}>
                          {paperLabel(name)}
                        </DropdownMenu.RadioItem>
                      )}
                    </For>
                  </DropdownMenu.RadioGroup>
                </DropdownMenu.Content>
              </DropdownMenu.Portal>
            </DropdownMenu>
            <div class="bar-spacer" />
            <div class="bar-group">
              <Button class="icon-button" aria-label="Previous page" onClick={() => goToPage(currentPage() - 1)}>
                <ChevronLeft size={18} />
              </Button>
              <span class="page-indicator" aria-label="Page">
                {currentPage() + 1} / {pages()}
              </span>
              <Button class="icon-button" aria-label="Next page" onClick={() => goToPage(currentPage() + 1)}>
                <ChevronRight size={18} />
              </Button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
