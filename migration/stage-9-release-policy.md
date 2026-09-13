# Stage 9 release policy

These defaults are the packaging rules until the Astro site (Stage 10) and
GitHub Releases (Stage 11) replace the current `docs/` installer.

## Identifiers

| Field | Value |
| --- | --- |
| Project IDs | `justin`, `moon_phase`, `weather_watch`, `real_weather`, `flight_watch`, `starter-max7219`, `starter-template` |
| Initial versions | `justin` remains `2.0.4`; extracted and starter projects start at `1.0.0` |
| App filenames | Every buildable project publishes a stable `firmware.bin` in its own folder |
| Docs routes | `/` for `justin`; hyphenated routes such as `/moon-phase/` for the others |
| Release tags | `<project>/v<version>` (for example `justin/v2.0.5`) |

`starter-template` is not stageable, publishable, or deployable.

## Staging and publishing

Production builds stage a verified package under gitignored
`dist/<project>/<version>/`. That directory contains the bootloader, partition
table, project-qualified app binary, installer manifest, and `metadata.json`
(project id, version, production environment, chip family, hardware profile,
offsets, sizes, and SHA-256 hashes).

`./scripts/publish.sh` restages from the production build, then copies into the
registered docs path only after those metadata fields match the registry. A
package from another project, version, chip, environment, partition offset, or
filename is rejected.

## Retention

- `dist/` is local and ephemeral. Do not commit it.
- Each project keeps only the current `firmware.bin` on its site path.
  Publishing overwrites that file and removes leftover versioned app binaries.
- Older builds are not uploaded to GitHub Releases in this stage. Revisit that
  when Stage 11 adds release automation.

## Deploy

`./scripts/deploy <project> --patch|--minor|--major` bumps, builds, stages,
publishes, commits, and tags one project. It fails before commit when the tree
is dirty, a required step fails, or the commit would include another project's
files. The version file is restored when deploy fails before the release
commit.

Deploy does not push unless `--push` is passed. Creating the commit, tag, or
remote update still requires an explicit user request.
