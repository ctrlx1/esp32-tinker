import { defineConfig } from "astro/config";

export default defineConfig({
  site: "https://ctrlx1.github.io",
  base: "/esp32-tinker/",
  output: "static",
  trailingSlash: "always",
});
