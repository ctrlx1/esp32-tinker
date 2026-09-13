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
  return null;
}
