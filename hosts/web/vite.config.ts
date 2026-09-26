import { defineConfig } from "vite";
import { VitePWA } from "vite-plugin-pwa";
import solid from "vite-plugin-solid";

export default defineConfig({
  base: "/math-notes/",
  plugins: [
    solid(),
    // vite-plugin-pwa guide "Static assets": precache everything, the engine's
    // .wasm included, so the app loads offline after one visit.
    VitePWA({
      registerType: "autoUpdate",
      manifest: {
        name: "Math Notes",
        short_name: "Math Notes",
        start_url: "/math-notes/",
        display: "standalone",
        background_color: "#F6F7F9",
        icons: [{ src: "favicon.svg", sizes: "any", type: "image/svg+xml" }],
      },
      workbox: {
        globPatterns: ["**/*.{js,css,html,svg,wasm}"],
        maximumFileSizeToCacheInBytes: 16 * 1024 * 1024,
      },
    }),
  ],
});
