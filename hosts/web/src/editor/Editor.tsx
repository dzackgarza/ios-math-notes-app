import { Button } from "@kobalte/core/button";
import { DropdownMenu } from "@kobalte/core/dropdown-menu";
import { ToggleGroup } from "@kobalte/core/toggle-group";
import { ChevronDown, ChevronLeft, ChevronRight, Ellipsis, Grip, Highlighter, PenLine, Redo2, Undo2, X } from "lucide-solid";
import { For, createEffect, createResource, createSignal, onCleanup, onMount } from "solid-js";

import { Brush, PageSize, type Canvas } from "../engine/engine.ts";
import { ViewController, type View } from "../input/gestures.ts";
import { browserEngine, capabilities, penSamples } from "../input/pointer.ts";
import { listTemplates } from "../storage/folder.ts";
import { AppMark } from "../ui/Library.tsx";
import { paperLabel } from "../ui/paper.tsx";
import { applyTemplate, type OpenNotebook } from "./notebook.ts";

const MARGIN = 16; // CSS px around the pages

// The tool rail's pens (docs/specs/tablet-ui.md, Editor): brush and size in pt.
const PENS = {
  pen: { label: "Pen", brush: Brush.pressurePen, size: 1.6, rgb: 0x1a1a1a },
  highlighter: { label: "Highlighter", brush: Brush.highlighter, size: 8, rgb: 0xf5d547 },
} as const;
type PenId = keyof typeof PENS;

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
  const [pen, setPen] = createSignal<PenId>("pen");
  const [colors, setColors] = createSignal<Record<PenId, number>>({ pen: PENS.pen.rgb, highlighter: PENS.highlighter.rgb });
  const [view, setView] = createSignal<View>({ scale: 1, x: MARGIN, y: MARGIN });
  const [pages, setPages] = createSignal(doc.pageCount());

  // Keeps some page in view: the content may not scroll past the margins.
  const clampView = (view: View): View => {
    const content = doc.contentSize();
    const width = content.width * view.scale, height = content.height * view.scale;
    const clampAxis = (at: number, size: number, viewport: number) =>
      size + 2 * MARGIN <= viewport ? (viewport - size) / 2 : Math.min(MARGIN, Math.max(viewport - MARGIN - size, at));
    return {
      scale: view.scale,
      x: clampAxis(view.x, width, element.clientWidth),
      y: clampAxis(view.y, height, element.clientHeight),
    };
  };
  const controller = new ViewController({ scale: 1, x: MARGIN, y: MARGIN }, (view) => {
    const clamped = clampView(view);
    controller.view = clamped;
    canvas?.setView(clamped.scale, 0, 0, clamped.scale, clamped.x, clamped.y);
    setView(clamped);
    setPages(doc.pageCount());
  });

  const fitScale = () => (element.clientWidth - 2 * MARGIN) / doc.contentSize().width;

  const fitWidth = () => controller.set({ scale: fitScale(), x: MARGIN, y: MARGIN });

  // Zooms about the top left of the view, keeping the page at the top in place.
  const zoom = (factor: number) => {
    const { scale, y } = controller.view;
    const next = fitScale() * factor;
    controller.set({ scale: next, x: MARGIN, y: MARGIN + ((y - MARGIN) * next) / scale });
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
    if (e.type === "pointerdown") element.setPointerCapture(e.pointerId);
    canvas.input(penSamples(e, at, capabilities(engineName, e.pointerType), ids));
    if (e.type === "pointerup" || e.type === "pointercancel") saver.schedule();
  };

  const onWheel = (e: WheelEvent) => {
    e.preventDefault();
    controller.wheel(e, origin());
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

  // Scrolls page `index` into view when it is not already visible.
  const showPage = (index: number) => {
    if (index < 0) return;
    const rect = doc.pageRect(index);
    const { scale, x, y } = controller.view;
    const top = y + rect.y * scale, bottom = top + rect.height * scale;
    if (bottom > 0 && top < element.clientHeight) return;
    controller.set({ scale, x, y: MARGIN - rect.y * scale });
  };

  // Puts the top of page `index` at the top of the view.
  const goToPage = (index: number) => {
    if (index < 0 || index >= doc.pageCount()) return;
    const { scale, x } = controller.view;
    controller.set({ scale, x, y: MARGIN - doc.pageRect(index).y * scale });
  };

  const history = (step: "undo" | "redo") => {
    const moved = step === "undo" ? doc.undo() : doc.redo();
    if (!moved) return;
    edit(() => showPage(moved.page));
  };

  const onKey = (e: KeyboardEvent) => {
    if (!(e.ctrlKey || e.metaKey) || e.key.toLowerCase() !== "z") return;
    e.preventDefault();
    history(e.shiftKey ? "redo" : "undo");
  };

  onMount(() => {
    window.addEventListener("keydown", onKey);
    onCleanup(() => window.removeEventListener("keydown", onKey));
    canvas = doc.createCanvas("#ink-canvas");
    canvas.setUtcOffset(performance.timeOrigin);
    createEffect(() => {
      const { brush, size } = PENS[pen()];
      canvas?.setTool({ brush, rgb: colors()[pen()], size });
    });
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
          <ToggleGroup class="tools" value={pen()} onChange={(v) => v && setPen(v as PenId)} aria-label="Pens">
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
          </ToggleGroup>
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
