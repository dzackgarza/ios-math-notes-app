import { Button } from "@kobalte/core/button";
import { DropdownMenu } from "@kobalte/core/dropdown-menu";
import { For, createResource, onCleanup, onMount } from "solid-js";

import { Brush, PageSize, type Canvas } from "../engine/engine.ts";
import { ViewController, type View } from "../input/gestures.ts";
import { browserEngine, capabilities, penSamples } from "../input/pointer.ts";
import { listTemplates } from "../storage/folder.ts";
import { applyTemplate, type OpenNotebook } from "./notebook.ts";

const MARGIN = 16; // CSS px around the pages
const DOUBLE_TAP_MS = 350;
const DOUBLE_TAP_PX = 24;

export function Editor(props: { notebook: OpenNotebook; onClose: () => void }) {
  let area!: HTMLDivElement;
  let element!: HTMLCanvasElement;
  let canvas: Canvas | undefined;
  let frame = 0;
  const ids = { next: 0 };
  const engineName = browserEngine();
  const { document: doc, saver, root } = props.notebook;
  const [templates] = createResource(() => listTemplates(root));

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
  });

  const fitWidth = () => {
    const content = doc.contentSize();
    controller.set({ scale: (element.clientWidth - 2 * MARGIN) / content.width, x: MARGIN, y: MARGIN });
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

  // Double tap on the ghost page after the last page adds a page (Write
  // syncscribble/scribblearea.cpp:1891-1900, scribbledoc.cpp:381-385).
  let lastTap: { time: number; x: number; y: number } | undefined;
  let onGhost = false;
  const onGhostTap = (x: number, y: number, time: number) => {
    if (lastTap && time - lastTap.time < DOUBLE_TAP_MS && Math.hypot(x - lastTap.x, y - lastTap.y) < DOUBLE_TAP_PX) {
      lastTap = undefined;
      edit(() => doc.insertPage(doc.pageCount()));
      return;
    }
    lastTap = { time, x, y };
  };

  const onPointer = (e: PointerEvent) => {
    if (!canvas) return;
    const at = origin();
    if (controller.pointer(e, at)) {
      if (e.type === "pointerdown") element.setPointerCapture(e.pointerId);
      return;
    }
    const x = e.clientX - at.x, y = e.clientY - at.y;
    if (e.type === "pointerdown") {
      element.setPointerCapture(e.pointerId);
      onGhost = canvas.pageAt(x, y) === doc.pageCount();
    }
    if (onGhost) {
      if (e.type === "pointerup") onGhostTap(x, y, e.timeStamp);
      if (e.type === "pointerup" || e.type === "pointercancel") onGhost = false;
      return;
    }
    canvas.input(penSamples(e, at, capabilities(engineName, e.pointerType), ids));
    if (e.type === "pointerup" || e.type === "pointercancel") saver.schedule();
  };

  const onWheel = (e: WheelEvent) => {
    e.preventDefault();
    controller.wheel(e, origin());
  };

  // The page at the middle of the view; the last page below the pages.
  const currentPage = () => {
    const page = canvas?.pageAt(element.clientWidth / 2, element.clientHeight / 2) ?? 0;
    const count = doc.pageCount();
    return page < 0 || page >= count ? count - 1 : page;
  };

  const edit = (change: () => void) => {
    change();
    controller.set(controller.view);
    saver.schedule();
  };

  onMount(() => {
    canvas = doc.createCanvas("#ink-canvas");
    canvas.setTool({ brush: Brush.pressurePen, rgb: 0x1a1a1a, size: 1.6 });
    canvas.setUtcOffset(performance.timeOrigin);
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

  const close = async () => {
    await saver.save();
    canvas?.free();
    canvas = undefined;
    doc.free();
    props.onClose();
  };

  return (
    <div class="editor">
      <div class="toolbar">
        <Button class="button" onClick={() => void close()}>
          Library
        </Button>
        <span>{props.notebook.name}</span>
        <DropdownMenu>
          <DropdownMenu.Trigger class="button">Page</DropdownMenu.Trigger>
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
              <DropdownMenu.Sub>
                <DropdownMenu.SubTrigger class="menu-item">Template</DropdownMenu.SubTrigger>
                <DropdownMenu.Portal>
                  <DropdownMenu.SubContent class="menu">
                    <For each={templates()}>
                      {(name) => (
                        <DropdownMenu.Item
                          class="menu-item"
                          onSelect={() => void applyTemplate(root, doc, name).then(() => edit(() => {}))}
                        >
                          {name}
                        </DropdownMenu.Item>
                      )}
                    </For>
                  </DropdownMenu.SubContent>
                </DropdownMenu.Portal>
              </DropdownMenu.Sub>
            </DropdownMenu.Content>
          </DropdownMenu.Portal>
        </DropdownMenu>
      </div>
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
      </div>
    </div>
  );
}
