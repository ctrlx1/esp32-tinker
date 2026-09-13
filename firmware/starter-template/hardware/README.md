# Hardware profile checklist

Before making a copied project buildable, record:

- board and chip family;
- framework and partition scheme;
- every display/peripheral signal and GPIO;
- display driver, module type, count, dimensions, and orientation;
- voltage, current, and power constraints;
- production upload method;
- Wokwi part type and a `diagram.json` whose wires match this profile.

A copied project is not finished until `wokwi.toml`, `diagram.json`,
`[env:wokwi]`, and `projects.json` `environments.wokwi` exist. If Wokwi has no
official part, record the unofficial or custom-chip part you used.

Do not infer a new project's wiring merely because it uses the same display
chipset as another project.
