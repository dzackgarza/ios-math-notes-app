// The pointer adapter on constructed PointerEvents, in Chromium, Firefox and
// WebKit (vitest.config.ts).
import { describe, expect, test } from "vitest";

import { Has, Phase, Tool } from "../engine/engine.ts";
import { browserEngine, capabilities, penSamples, tiltToSpherical } from "./pointer.ts";

const origin = { x: 10, y: 20 };
const pen = capabilities(browserEngine(), "pen");

function event(type: string, init: PointerEventInit): PointerEvent {
  return new PointerEvent(type, { pointerId: 1, pointerType: "pen", isPrimary: true, ...init });
}

describe("penSamples", () => {
  test("a pen sample carries pressure, the tilt angles and twist in radians", () => {
    const e = event("pointermove", {
      clientX: 110,
      clientY: 70,
      buttons: 1,
      pressure: 0.625,
      tiltX: 30,
      tiltY: 20,
      twist: 90,
    });
    const [s] = penSamples(e, origin, pen, { next: 7 });
    expect(s).toMatchObject({ x: 100, y: 50, pressure: 0.625, buttons: 1, id: 7 });
    // The angles of the tilts the event reports: Chromium and Firefox keep the
    // init's tilts and compute altitudeAngle from them; Playwright's WebKit
    // reports tiltX 90, tiltY 0 and no altitudeAngle (TRAPS.md).
    const angles = tiltToSpherical(e.tiltX, e.tiltY);
    expect(s.altitude).toBeCloseTo(angles.altitude, 6);
    expect(s.azimuth).toBeCloseTo(angles.azimuth, 6);
    expect(s.roll).toBeCloseTo(Math.PI / 2, 6);
    expect(s).toMatchObject({ tool: Tool.pen, phase: Phase.move, predicted: false, has: pen });
  });

  test("tilts convert to altitude and azimuth as in Pointer Events Level 3", () => {
    const deg = Math.PI / 180;
    expect(tiltToSpherical(30, 0).altitude).toBeCloseTo(60 * deg, 12);
    expect(tiltToSpherical(30, 0).azimuth).toBe(0);
    expect(tiltToSpherical(-30, 0).azimuth).toBeCloseTo(Math.PI, 12);
    expect(tiltToSpherical(0, -45)).toEqual({ altitude: 45 * deg, azimuth: 1.5 * Math.PI });
    expect(tiltToSpherical(0, 0)).toEqual({ altitude: Math.PI / 2, azimuth: 0 });
    expect(tiltToSpherical(90, 20)).toEqual({ altitude: 0, azimuth: 0 });
    const both = tiltToSpherical(45, 45);
    expect(both.altitude).toBeCloseTo(Math.atan(1 / Math.SQRT2), 12);
    expect(both.azimuth).toBeCloseTo(Math.PI / 4, 12);
  });

  test("a move without buttons is hover", () => {
    const [s] = penSamples(event("pointermove", { buttons: 0 }), origin, pen, { next: 0 });
    expect(s.phase).toBe(Phase.hover);
  });

  test("the eraser end is the eraser tool, down and up", () => {
    const down = penSamples(event("pointerdown", { button: 5, buttons: 32 }), origin, pen, { next: 0 });
    const up = penSamples(event("pointerup", { button: 5, buttons: 0 }), origin, pen, { next: 1 });
    expect(down).toMatchObject([{ tool: Tool.eraser, phase: Phase.begin }]);
    expect(up).toMatchObject([{ tool: Tool.eraser, phase: Phase.end }]);
  });

  test("coalesced samples come first in order, then the predicted ones flagged", () => {
    const at = (x: number) => new PointerEvent("pointermove", { pointerType: "pen", clientX: x, clientY: 20, buttons: 1 });
    const e = event("pointermove", {
      clientX: 40,
      clientY: 20,
      buttons: 1,
      coalescedEvents: [at(20), at(30), at(40)],
      predictedEvents: [at(50)],
    });
    const ids = { next: 0 };
    const samples = penSamples(e, origin, pen, ids);
    expect(samples).toMatchObject([
      { x: 10, predicted: false, id: 0 },
      { x: 20, predicted: false, id: 1 },
      { x: 30, predicted: false, id: 2 },
      { x: 40, predicted: true, id: 3 },
    ]);
    expect(ids.next).toBe(4);
  });

  test("an event without a coalesced list is its own sample", () => {
    const samples = penSamples(event("pointerdown", { clientX: 15, clientY: 25, buttons: 1 }), origin, pen, { next: 0 });
    expect(samples).toMatchObject([{ x: 5, y: 5, phase: Phase.begin }]);
  });
});

describe("capabilities", () => {
  test("come from the browser engine and pointer type, not from values", () => {
    expect(capabilities("chromium", "pen")).toBe(Has.pressure | Has.altitude | Has.azimuth | Has.roll);
    expect(capabilities("webkit", "pen")).toBe(Has.pressure | Has.altitude | Has.azimuth);
    expect(capabilities("gecko", "mouse")).toBe(0);
    expect(capabilities("chromium", "touch")).toBe(0);
  });
});
