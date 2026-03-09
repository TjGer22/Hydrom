---
description: >
  PCB design details, fabrication notes and Gerber file locations for the Hydrom circuit board.
---

# PCB Design Files

The schematic and board layout are created with [KiCad](https://www.kicad.org/) 6+.

## Files

| File | Description |
|---|---|
| `hydrom_V1.0-2.kicad_pro` | KiCad project file — open this first |
| `hydrom_V1.0-2.kicad_sch` | Schematic |
| `hydrom_V1.0-2.kicad_pcb` | PCB layout |
| `hydrom_V1.0-2.xml` | Netlist |
| `PCB2.lib` | Custom schematic symbol library |
| `RT9080-33GJ5.lib` / `.dcm` | RT9080 LDO symbol and datasheet reference |
| `fp-lib-table` | Footprint library table |
| `sym-lib-table` | Symbol library table |

## Opening the Project

1. Install [KiCad](https://www.kicad.org/download/) 6 or later.
2. Clone the repository:
   ```bash
   git clone https://github.com/TjGer22/Hydrom.git
   ```
3. Open `hardware/pcb/hydrom_V1.0-2.kicad_pro` in KiCad.

## Board Renders

![JLCPCB Top View](../assets/images/Folie1.png)
![JLCPCB Bottom View](../assets/images/Folie1.png)
