---
description: >
  Ready-to-order production files for PCB fabrication and SMD assembly of the Hydrom.
---

# Production Files

Ready-to-order production files are in [`hardware/production/`](https://github.com/TjGer22/Hydrom/tree/main/hardware/production).

## Gerbers

Standard Gerber 274-X files for PCB fabrication. Tested successfully with **JLCPCB**.

| File | Layer |
|---|---|
| `hydrom-F_Cu.gbr` | Front copper |
| `hydrom-B_Cu.gbr` | Back copper |
| `hydrom-F_Mask.gbr` | Front solder mask |
| `hydrom-B_Mask.gbr` | Back solder mask |
| `hydrom-F_Silkscreen.gbr` | Front silkscreen |
| `hydrom-B_Silkscreen.gbr` | Back silkscreen |
| `hydrom-F_Paste.gbr` | Front solder paste |
| `hydrom-B_Paste.gbr` | Back solder paste |
| `hydrom-Edge_Cuts.gbr` | Board outline |
| `hydrom-PTH.drl` | Plated through-holes |
| `hydrom-NPTH.drl` | Non-plated through-holes |

## Centroid (Pick-and-Place)

Pick-and-place files for SMT assembly:

- `hydrom-top-pos.xlsx` — top side component positions
- `hydrom-bottom-pos.xlsx` — bottom side component positions

## Ordering from JLCPCB

1. Download the `gerbers/` folder as a ZIP.
2. Upload to [jlcpcb.com](https://jlcpcb.com) → *Order Now*.
3. Enable *SMT Assembly* and upload the BOM and centroid files from `bom/` and `centroid/`.
4. Review the component placement preview and place your order.

!!! tip
    Select **HASL-Lead Free** or **ENIG** surface finish for best solderability.
