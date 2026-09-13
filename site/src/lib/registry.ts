import { readFileSync } from "node:fs";
import { resolve } from "node:path";

export type Hardware = {
  status: "configured" | "unassigned";
  profile?: string;
  chipFamily?: string;
};

export type ProjectRecord = {
  id: string;
  name: string;
  description: string;
  path: string;
  versionFile: string;
  buildable: boolean;
  hardware: Hardware;
  docs: {
    route: string;
    path: string;
    manifest?: string;
  };
};

export type SiteProject = ProjectRecord & {
  version: string;
  href: string;
  slug: string;
  manifestHref?: string;
  firmwareHref?: string;
};

const MAX7219_PROFILE = {
  id: "max7219-4-fc16",
  board: "ESP32 Dev Module",
  display: "FC16 MAX7219, four 8×8 modules",
  pins: [
    { signal: "DIN", gpio: "23" },
    { signal: "CLK", gpio: "18" },
    { signal: "CS", gpio: "5" },
  ],
};

const HUB75_P4_PROFILE = {
  id: "hub75-p4-64x32",
  board: "ESP32 Dev Module",
  display: "P4-256x128-2121-A5 HUB75, 64×32 RGB, 1/16 scan",
  pins: [
    { signal: "R1", gpio: "25" },
    { signal: "G1", gpio: "26" },
    { signal: "B1", gpio: "27" },
    { signal: "R2", gpio: "14" },
    { signal: "G2", gpio: "12" },
    { signal: "B2", gpio: "13" },
    { signal: "A", gpio: "23" },
    { signal: "B", gpio: "19" },
    { signal: "C", gpio: "5" },
    { signal: "D", gpio: "17" },
    { signal: "LAT", gpio: "4" },
    { signal: "OE", gpio: "15" },
    { signal: "CLK", gpio: "16" },
  ],
};

function repoRoot(): string {
  return resolve(process.cwd(), "..");
}

function routeSlug(route: string): string {
  return route.replace(/^\/+|\/+$/g, "");
}

export function loadProjects(): SiteProject[] {
  const root = repoRoot();
  const data = JSON.parse(readFileSync(resolve(root, "projects.json"), "utf8"));
  return data.projects.map((project: ProjectRecord) => {
    const version = readFileSync(resolve(root, project.versionFile), "utf8").trim();
    const slug = routeSlug(project.docs.route);
    const href = slug ? `/${slug}/` : "/";
    return {
      ...project,
      version,
      slug,
      href,
      manifestHref: project.buildable
        ? `/firmware/${project.id}/manifest.json`
        : undefined,
      firmwareHref: project.buildable
        ? `/firmware/${project.id}/firmware.bin`
        : undefined,
    };
  });
}

export function hardwareProfile(project: SiteProject) {
  if (project.hardware.profile === MAX7219_PROFILE.id) {
    return MAX7219_PROFILE;
  }
  if (project.hardware.profile === HUB75_P4_PROFILE.id) {
    return HUB75_P4_PROFILE;
  }
  return null;
}
