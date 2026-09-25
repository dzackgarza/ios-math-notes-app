// Loads the engine module. Vite emits engine.wasm as an asset from the
// module's `new URL("engine.wasm", import.meta.url)` (Emscripten test/vite).
import { Engine } from "./engine.ts";
import factory from "./wasm/engine.mjs";

export function loadEngine(): Promise<Engine> {
  return Engine.load(factory);
}
