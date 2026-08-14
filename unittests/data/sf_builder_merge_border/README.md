# SF builder/merger border fixture

This approximately 454 KB input fixture drives both SF builders and the SF
merger. It retains two adjacent level-15 mask-border nodes: one valid merge and
one merge that currently produces a malformed terrain file. The integration
test intentionally requires both results to be readable, so the known defect
makes the test fail.

The fixture contains:

- 128 x 176 pixel, 1 m GS and GT elevation crops derived from the Geoland.at
  `Oe_2020` datasets;
- the nine Basemap and nine Gataki orthophoto tiles intersecting that crop from
  zoom levels 12 through 17; and
- the full Statistik Austria NUTS-2 Tirol (`AT33`) polygon simplified with a
  5 m tolerance. The full extent is significant because the mask projection
  uses the polygon bounds; clipping the mask no longer reproduces the defect.

Sources and attribution:

- Elevation: Geoland.at (Austria and its federal states), CC BY 4.0.
- Basemap orthophotos:
  `https://mapsneu.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/{zoom}/{y}/{x}.jpeg`.
- Gataki orthophotos:
  `https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/{zoom}/{y}/{x}.jpeg`.
- Tirol boundary: Statistik Austria NUTS-2 2026-01-01, feature `AT33`, obtained
  from the Statistik Austria geodata WFS.
