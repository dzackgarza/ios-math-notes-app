// PointerEvent -> InkPenSample records: one batch per event, the coalesced
// samples (or the event itself when the list is empty), then the predicted
// ones flagged. Follows W3C Pointer Events Level 3, §"Coalesced and
// predicted events" and its drawing example.
import { Has, Phase, Tool, type PenSample } from "../engine/engine.ts";

export type BrowserEngine = "chromium" | "gecko" | "webkit";

export function browserEngine(nav: Navigator = navigator): BrowserEngine {
  const brands = (nav as Navigator & { userAgentData?: { brands: { brand: string }[] } }).userAgentData?.brands;
  if (brands?.some((b) => b.brand === "Chromium")) return "chromium";
  if (/Firefox\//.test(nav.userAgent)) return "gecko";
  return /Chrome\//.test(nav.userAgent) ? "chromium" : "webkit";
}

// INK_HAS_* bits: what the browser engine measures for a pointer type. The
// values say nothing: browsers report pressure 0.5 and twist 0 without a
// sensor. Safari reports twist 0 for every pen.
export function capabilities(engine: BrowserEngine, pointerType: string): number {
  if (pointerType !== "pen") return 0;
  const angles = Has.pressure | Has.altitude | Has.azimuth;
  return engine === "webkit" ? angles : angles | Has.roll;
}

// Numbers each sample; `ink_input_update` refers to samples by id.
export interface SampleIds {
  next: number;
}

const ERASER_BUTTON = 5; // PointerEvent.button of the pen's eraser end
const ERASER_BUTTONS = 32; // its bit in PointerEvent.buttons

function phase(e: PointerEvent): number {
  switch (e.type) {
    case "pointerdown":
      return Phase.begin;
    case "pointerup":
      return Phase.end;
    case "pointercancel":
      return Phase.cancel;
    default:
      return e.buttons === 0 ? Phase.hover : Phase.move;
  }
}

function tool(e: PointerEvent): number {
  if (e.pointerType === "touch") return Tool.touch;
  if (e.pointerType === "mouse") return Tool.mouse;
  const eraser = (e.buttons & ERASER_BUTTONS) !== 0 || e.button === ERASER_BUTTON;
  return eraser ? Tool.eraser : Tool.pen;
}

// Altitude and azimuth from tiltX and tiltY, for browsers without
// altitudeAngle (WebKit on the desktop). A port of tilt2spherical in W3C
// Pointer Events Level 3, §"Converting between tiltX/tiltY and
// altitudeAngle/azimuthAngle".
export function tiltToSpherical(tiltX: number, tiltY: number): { altitude: number; azimuth: number } {
  const x = (tiltX * Math.PI) / 180;
  const y = (tiltY * Math.PI) / 180;
  const vertical = Math.abs(tiltX) === 90 || Math.abs(tiltY) === 90;
  let azimuth = 0;
  if (tiltX === 0) {
    if (tiltY > 0) azimuth = Math.PI / 2;
    else if (tiltY < 0) azimuth = (3 * Math.PI) / 2;
  } else if (tiltY === 0) {
    if (tiltX < 0) azimuth = Math.PI;
  } else if (!vertical) {
    azimuth = Math.atan2(Math.tan(y), Math.tan(x));
    if (azimuth < 0) azimuth += 2 * Math.PI;
  }
  let altitude = 0;
  if (vertical) altitude = 0;
  else if (tiltX === 0) altitude = Math.PI / 2 - Math.abs(y);
  else if (tiltY === 0) altitude = Math.PI / 2 - Math.abs(x);
  else altitude = Math.atan(1 / Math.sqrt(Math.tan(x) ** 2 + Math.tan(y) ** 2));
  return { altitude, azimuth };
}

// `origin` is the canvas's top-left in client coordinates (CSS px).
export function penSamples(
  e: PointerEvent,
  origin: { x: number; y: number },
  has: number,
  ids: SampleIds,
): PenSample[] {
  const kind = tool(e);
  const eventPhase = phase(e);
  const sample = (p: PointerEvent, samplePhase: number, predicted: boolean): PenSample => {
    const angles =
      p.altitudeAngle === undefined ? tiltToSpherical(p.tiltX, p.tiltY) : { altitude: p.altitudeAngle, azimuth: p.azimuthAngle };
    return {
      x: p.clientX - origin.x,
      y: p.clientY - origin.y,
      time: p.timeStamp,
      pressure: p.pressure,
      altitude: angles.altitude,
      azimuth: angles.azimuth,
      roll: (p.twist * Math.PI) / 180,
      hoverHeight: 0,
      buttons: p.buttons,
      has,
      id: ids.next++,
      tool: kind,
      phase: samplePhase,
      predicted,
    };
  };
  const coalesced = e.getCoalescedEvents?.() ?? [];
  const real = coalesced.length > 0 ? coalesced : [e];
  // Begin marks the first sample of the stroke; end and cancel the last.
  const samples = real.map((p, i) => {
    if (eventPhase === Phase.begin) return sample(p, i === 0 ? Phase.begin : Phase.move, false);
    if (eventPhase === Phase.end || eventPhase === Phase.cancel) {
      return sample(p, i === real.length - 1 ? eventPhase : Phase.move, false);
    }
    return sample(p, eventPhase, false);
  });
  if (eventPhase === Phase.move) {
    for (const p of e.getPredictedEvents?.() ?? []) samples.push(sample(p, Phase.move, true));
  }
  return samples;
}
