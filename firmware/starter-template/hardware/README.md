# Hardware profile checklist

Before making a copied project buildable, record:

- board and chip family;
- framework and partition scheme;
- every display/peripheral signal and GPIO;
- display driver, module type, count, dimensions, and orientation;
- voltage, current, and power constraints;
- production upload method;
- Wokwi component availability and matching simulated wiring.

Do not infer a new project's wiring merely because it uses the same display
chipset as another project.
