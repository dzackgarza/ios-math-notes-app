import { Button } from "@kobalte/core/button";
import { onCleanup, onMount } from "solid-js";

import { Brush, type Canvas } from "../engine/engine.ts";
import { browserEngine, capabilities, penSamples } from "../input/pointer.ts";
import type { OpenNotebook } from "./notebook.ts";

const MARGIN = 16; // CSS px around the pages

export function Editor(props: { notebook: OpenNotebook; onClose: () => void }) {
  let area!: HTMLDivElement;
  let element!: HTMLCanvasElement;
  let canvas: Canvas | undefined;
  let frame = 0;
  let scrollY = MARGIN;
  const ids = { next: 0 };
  const engineName = browserEngine();
  const { document: doc, saver } = props.notebook;

  // Fits the widest page to the width; scrolls vertically.
  const updateView = () => {
    if (!canvas) return;
    const content = doc.contentSize();
    const width = element.clientWidth;
    const scale = Math.min((width - 2 * MARGIN) / content.width, 2);
    const bottom = element.clientHeight - MARGIN - content.height * scale;
    scrollY = Math.max(Math.min(scrollY, MARGIN), Math.min(bottom, MARGIN));
    canvas.setView(scale, 0, 0, scale, (width - content.width * scale) / 2, scrollY);
  };

  const resize = () => {
    if (!canvas) return;
    const ratio = window.devicePixelRatio;
    element.width = Math.round(element.clientWidth * ratio);
    element.height = Math.round(element.clientHeight * ratio);
    canvas.setSurfaceSize(element.width, element.height, ratio);
    updateView();
  };

  const loop = () => {
    canvas?.render();
    frame = requestAnimationFrame(loop);
  };

  let panFrom: number | undefined;
  const onPointer = (e: PointerEvent) => {
    if (!canvas) return;
    if (e.pointerType === "touch") {
      // Fingers scroll; they never draw.
      if (e.type === "pointerdown") panFrom = e.clientY;
      if (e.type === "pointermove" && panFrom !== undefined) {
        scrollY += e.clientY - panFrom;
        panFrom = e.clientY;
        updateView();
      }
      if (e.type === "pointerup" || e.type === "pointercancel") panFrom = undefined;
      return;
    }
    if (e.type === "pointerdown") element.setPointerCapture(e.pointerId);
    const rect = element.getBoundingClientRect();
    canvas.input(penSamples(e, { x: rect.left, y: rect.top }, capabilities(engineName, e.pointerType), ids));
    if (e.type === "pointerup" || e.type === "pointercancel") saver.schedule();
  };

  const onWheel = (e: WheelEvent) => {
    e.preventDefault();
    scrollY -= e.deltaY;
    updateView();
  };

  onMount(() => {
    canvas = doc.createCanvas("#ink-canvas");
    canvas.setTool({ brush: Brush.pressurePen, rgb: 0x1a1a1a, size: 1.6 });
    canvas.setUtcOffset(performance.timeOrigin);
    const observer = new ResizeObserver(resize);
    observer.observe(area);
    resize();
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
