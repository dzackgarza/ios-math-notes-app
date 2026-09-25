import type { NotebookFile } from "./engine/engine.ts";

declare global {
  interface Window {
    // With ?root=opfs: every file the page wrote, in order. The browser
    // tests compare the saved files with these bytes.
    mathNotesWrites?: NotebookFile[];
  }
}
