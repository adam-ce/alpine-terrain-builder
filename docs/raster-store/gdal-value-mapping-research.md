# GDAL value-mapping research

Research date: 2026-09-13. This note informs the halo design; recommendations
below are not accepted implementation decisions. Source inspection targets
GDAL 3.10.3, matching the locally installed `gdalinfo` and project build.

## Can GDAL identify linear versus sRGB samples?

Sometimes, through optional format metadata. There is no general
`Linear`/`SRGB` answer in band colour interpretation: GDAL's enum identifies
roles such as red, green, blue, alpha and spectral bands, without a transfer
function. RGB bands therefore do not establish sRGB encoding.
[GDAL colour-interpretation enum](https://github.com/OSGeo/gdal/blob/v3.10.3/gcore/gdal.h)

The dataset's `COLOR_PROFILE` metadata domain can provide evidence:

| Driver | Available evidence |
| --- | --- |
| GeoTIFF | Base64 `SOURCE_ICC_PROFILE`, or primaries/white point and per-channel TIFF transfer-function tables. An ICC profile takes precedence over the other profile tags. |
| PNG | ICC profile and profile name, an explicit sRGB marker, or `PNG_GAMMA` and chromaticities. |
| JPEG | Base64 embedded ICC profile; EXIF metadata can also be exposed. |

These are optional source descriptions, not a classification into our two
mapping values. GDAL also warns that profile metadata describes original raw
samples and may not apply after its automatic conversion to RGB.
[GeoTIFF driver](https://gdal.org/en/stable/drivers/raster/gtiff.html#color-profile-metadata),
[PNG driver](https://gdal.org/en/stable/drivers/raster/png.html#color-profile-metadata),
[JPEG driver](https://gdal.org/en/stable/drivers/raster/jpeg.html#color-profile-metadata)

In GDAL 3.10.3, PNG's `LoadICCProfile()` first reads an ICC profile; otherwise
an sRGB chunk produces `SOURCE_ICC_PROFILE_NAME=sRGB`; otherwise it exposes
gamma/chromaticity metadata. If an ICC payload exists, its arbitrary profile
name alone is insufficient evidence. The application would need to interpret
the profile itself.
[PNG implementation](https://github.com/OSGeo/gdal/blob/v3.10.3/frmts/png/pngdataset.cpp)

PNG can describe linear samples: a gamma of 1 represents a linear transfer.
Other gamma values and ICC profiles need not be sRGB. In particular, an
approximate gamma of 0.45455 alone does not establish the exact piecewise sRGB
transfer function; the sRGB chunk specifies that separately.
[PNG gamma and sRGB specification](https://www.w3.org/TR/png-3/#11gAMA)

JPEG/YCbCr decoding and sRGB linearization are separate operations. GDAL's
JPEG driver describes conversion from YCbCr/CMYK to RGB and reports the source
model through `IMAGE_STRUCTURE:SOURCE_COLOR_SPACE`. This does not promise
linear-light output. Compression and channel roles are therefore insufficient
evidence for selecting `ValueMapping`.
[JPEG reading behaviour](https://gdal.org/en/stable/drivers/raster/jpeg.html)

## VRT mosaics

VRT can store dataset metadata in named domains. That capability does not
mean a mosaic automatically inherits or reconciles source profiles.
[VRT format](https://gdal.org/en/stable/drivers/raster/vrt.html)

In GDAL 3.10.3, `gdalbuildvrt` checks band colour interpretation but does not
copy or compare `COLOR_PROFILE`. `VRTDataset::GetMetadata()` returns the
VRT dataset's own metadata, apart from its special XML domain; it does not
aggregate underlying source colour profiles. Consequently, a VRT containing
RGB sources with different transfer functions cannot be assumed homogeneous
merely because its creation succeeded. This conclusion follows from the
versioned implementations, rather than a guarantee about every VRT-producing
tool.
[VRT builder](https://github.com/OSGeo/gdal/blob/v3.10.3/apps/gdalbuildvrt_lib.cpp),
[VRT metadata implementation](https://github.com/OSGeo/gdal/blob/v3.10.3/frmts/vrt/vrtdataset.cpp)

## Swissimage evidence

Swisstopo documents SWISSIMAGE download imagery as COG, three 8-bit RGB bands,
with JPEG95 compression. That product page does not specify sRGB or a
transfer function.
[Official SWISSIMAGE product specification](https://www.swisstopo.admin.ch/en/orthoimage-swissimage-10)

The existing RF design's
[reference TIFF](https://data.geo.admin.ch/ch.swisstopo.swissimage-dop10/swissimage-dop10_2024_2682-1199/swissimage-dop10_2024_2682-1199_0.1_2056.tif)
was inspected again on the research date with GDAL 3.10.3 using
`gdalinfo -json -mdd all` on its `/vsicurl/` URL. Observed results:

- 10000 by 10000 pixels; bands identified as Red, Green and Blue.
- `LAYOUT=COG`, `SOURCE_COLOR_SPACE=YCbCr`,
  `COMPRESSION=YCbCr JPEG`, `JPEG_QUALITY=95`.
- No `COLOR_PROFILE` domain or explicit transfer-function metadata reported.

This specific file therefore supplies no GDAL profile evidence that proves
sRGB. Its absence does not prove Linear either. The observation applies to
this reference file, not every swisstopo product or delivery.

## Selected policy — 2026-09-15

The user selected a type-based convention: default three- and four-component
unsigned 8-bit pixels to `ValueMapping::SRGBA`, otherwise `Linear`, with an
explicit override. RGB channels use the sRGB transfer function; alpha, if
present, remains linear. Persist the resolved mapping in store metadata and
document the defaults and override in RF CLI help. See
[Tiles with halo](tiles-with-halo.md) for the accepted contract.

This replaces the earlier recommendation below; it does not change the
findings about what GDAL metadata can establish.

## Earlier recommendation — superseded

Allow an explicit RF input mapping for the prepared dataset, including a VRT.
Persist the resolved `ValueMapping::{Linear, SRGB}` in the raster-store
manifest. Keep unknown/automatic selection at the import boundary if needed;
it need not become a third stored mapping value.

Use trustworthy metadata when available. Treat the agreed JPEG/PNG imagery
default as a policy assumption, not format-based proof, and allow a declared
linear input to override it. Do not silently infer Linear for an untagged TIFF
or VRT. An explicit SRGB choice is a practical way to import the intended
Swissimage VRT, but this research has not established its transfer function.

A prepared mosaic should have one agreed mapping across its sources. Mixed
mappings require normalization before treating it as one dataset. Arbitrary
ICC/gamma encodings cannot all be represented by the proposed two-value enum;
converting those inputs to a supported mapping is a separate scope decision.

Reading profile metadata does not itself linearize samples for subsequent
filtering. If linear-light filtering is also required during RF reprojection,
that is an additional design question beyond using the mapping during halo
downsampling.
