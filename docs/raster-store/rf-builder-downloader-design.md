# RF builder: online tile import

Status: implemented and verified, 2026-09-12. Progress and
verification evidence are recorded in [implementation-status.md](implementation-status.md).

This extends the implemented [GDAL builder](rf-builder-design.md). The
[storage format](storage-format.md), snapshot lifecycle in
[architecture](architecture.md), and shared [terminology](terminology.md)
continue to apply.

## Scope and command interface

Provide one executable with two explicit input modules:

```sh
rf-builder gdal --dataset prepared.vrt --mask validity.gpkg \
    --output new-rf --attribution-index 1

rf-builder tiles --provider providers/basemap.json \
    --mask validity.gpkg --output new-rf --attribution-index 1
```

Preserve the existing GDAL options under `gdal`;
the old invocation without a subcommand is intentionally replaced.

The online module supports JPEG imagery on the regular Web Mercator tile
grid. Require `--provider <provider.json>` to load all source settings from
one JSON file. There is no compiled provider store, built-in provider-name
lookup, or CLI override for URL/pattern, Y direction, source zoom limits, or
source tile size. RF keys always remain canonical XYZ keys. Region selection
uses the required vector mask rather than a root-tile argument.

The provider's source tile size is a positive power of two, constant
throughout an import. Reject JPEGs whose decoded dimensions do not match the
configured square dimensions. Online output is RGB8. The RF
`--tile-size` default remains 4096 and must equal the source side multiplied
by a nonnegative power of two. GDAL retains its existing dimension support.
Keep `--jobs` (default one), `--cache`, attribution, output, and persistent
logging options. There is no tile-directory input or source plugin system.

Keep `tile-downloader`, its scripts, and its tests. Copy useful URL/HTTP code
into the online module where appropriate; RF must build and operate with
`ALP_BUILD_TILE_DOWNLOADER=OFF`. Removing the old tool is a separate task.
Its existing compiled provider presets and CLI remain unchanged; the new
JSON-only provider interface belongs to RF.

## Provider JSON

Each provider file is a JSON object with these required fields:

```json
{
  "url_pattern": "https://example.org/tiles/{zoom}/{x}/{y}.jpeg",
  "y_direction": "down",
  "min_zoom": 7,
  "max_zoom": 20,
  "tile_size": 256
}
```

This is an illustrative schema example, not a verified configuration for
Basemap or Gataki. `url_pattern` is an HTTP(S) URL template containing
`{zoom}`, `{x}`, and `{y}`. `y_direction` is `down` for XYZ or `up` for legacy
TMS addressing. `min_zoom` and `max_zoom` are integer source zoom limits,
with minimum no greater than maximum and both within the supported tile-key
range. `tile_size` is the expected square source side in pixels, not the RF
output side. All fields must be explicit; reject unreadable files, malformed
JSON, missing or unknown fields, wrong types, and invalid values before HTTP
discovery or tile production.

Ship `providers/basemap.json` and `providers/gataki.json` in the repository.
Copy their URL templates and Y directions from the current downloader
presets. Vienna has the largest source zoom level; use Vienna to establish
each provider's maximum. Verify the minimum zoom and image dimensions before
filling in those fields. The executable reads the supplied path and
does not depend on the repository's provider directory at runtime. Additional
providers require only another JSON file, without rebuilding RF.

Both shipped files configure zooms 4..20 and 256-pixel source tiles. Basemap's
minimum deliberately excludes its available zooms 1..3, as approved, so the
default 4096-pixel RF side has a valid starting RF zoom of zero.

`rf-builder tiles --help` must show both a complete invocation using
`--provider providers/basemap.json` and a small JSON example with all required
fields, explaining that the source zoom maximum is a ceiling. Remove the
proposed individual source-setting options from the CLI and its help. Keep
RF output and operational options on the command line.

## Source discovery and RF selection

Start HTTP discovery at the provider's minimum source zoom. Missing lower
levels do not matter and are never requested. Within the configured range,
a 404 terminates that branch, including at the minimum. Do not search for
descendants beneath it. This is a supported-source assumption, as in the old
downloader, rather than a claim proved about every online provider.

The maximum is a ceiling, not a uniform target resolution. Preserve the
deepest available imagery in each relevant region up to that ceiling. For
example, Vienna can retain finer imagery than the rest of Austria. The
importer does not infer native image detail or detect provider overzooming.

Use GET while walking relevant branches. Intermediate JPEGs may be downloaded
and reused temporarily for discovery or fallback, but no source pyramid is
persisted. Use bounded in-memory reuse for bodies/decoded images and missing
results; redundant downloads after eviction or interruption are acceptable.
Do not add HEAD probing, speculative zoom skipping, or an advance discovery
pass. Conservative mask intersection must not discard narrow regions merely
because a coarse candidate has no accepted pixel centres.

Let source side be S, RF side be R, and k = log2(R/S). RF zoom r has native
pixel spacing matching source zoom r+k. At R=4096 and S=256, k=4, and one RF
tile can assemble a 16-by-16 block of source tiles at the matching source
level. Zoom limits always refer to source zoom. Validate the zoom/dimension
combination and integer arithmetic before production. Start at RF zoom
minimum-source-zoom minus k. Reject the configuration if this would be
negative, rather than clamping to RF zoom zero. Never request source imagery
above the configured maximum.

Proposed incremental selection algorithm:

1. Lazily enumerate RF candidates at minimum-source-zoom minus k that
   intersect conservative mask coverage. Consult the RF cache before any
   source work for a candidate.
2. In an uncached candidate, follow source ancestry from the configured
   minimum, respecting 404 pruning. Determine whether any relevant source
   region has imagery finer than the candidate's matching source level.
   A successful next-level probe is sufficient to request refinement; the
   full remaining subtree need not be discovered at that point.
3. If finer imagery is found, replace the candidate with its relevant RF
   children. Never write its payload. Continue independently in each child.
4. Otherwise prepare the candidate from available imagery at its matching
   level and coarser ancestor fallback. Apply the shared validity mask and
   attribution. Omit the tile if it has no accepted valid pixels.

Source discovery must account for every relevant quadrant, not infer sibling
resolution from one branch. A parent source image can supply missing regions,
but a physical RF parent must never coexist with any physical descendant.
Fixed RF dimensions can require substantial upscaling around a small fine
patch; preserving fine data takes priority over avoiding that expansion.

## Pixels, fallback, and mask

Copy decoded source values exactly where grids align at native resolution.
Use bilinear interpolation in linear-light RGB for enlarged ancestor fallback:
Treat decoded RGB as sRGB without ICC-profile conversion. Use the standard sRGB
transfer function to convert to linear light, interpolate, then encode back to
sRGB8 for storage. OpenCV JPEG decoding does not perform this linearization. Native-resolution copies do
not undergo this conversion round trip. This is separate from the existing
GDAL Lanczos policy and future TB filtering.

Resolve fallback neighbours consistently across source and RF tile edges.
Neighbours outside the validity mask may contribute. At a missing neighbour,
use available ancestor data where possible; at a true coverage edge extend
available edge samples. Do not create valid output where the output centre
itself has no source coverage. Interpolation must not independently clamp at
every RF tile edge. Longitude neighbours wrap at the canonical world seam;
latitude never wraps across the poles.

All successfully decoded JPEG pixels are valid before the mask, including
black pixels. An accepted valid pixel receives the one configured attribution
index; other pixels have attribution zero. Keep the existing mask centre
selection, boundary/hole rules, simplification, CRS behavior, and requirement
for caller-split antimeridian polygons. The mask is not a filter cutline.

## Shared coordinator interface

Extract the lifecycle currently embedded in `build.cpp` into `run.h/.cpp`:
bounded scheduling, cache linking, output writes/index ownership, progress,
checkpoints, error/cancellation handling, and publication.

The preparation result has three alternatives:

- a prepared RF tile for the candidate key;
- an empty completed candidate;
- subdivision into candidate children, with no parent payload.

The subdivision alternative is the necessary extension to the earlier idea
that the coordinator receives only prepared RF tiles. The coordinator
schedules children but does not interpret images, masks, source availability,
sampling ratios, or URLs. The two source modules decide selection and
preparation. Use a small internal typed interface for these actual callers;
do not add registries, plugins, or hypothetical additional input types.

GDAL can keep its existing count pass and selected-leaf traversal, supplying
only final candidates. Online input supplies initial candidates lazily and
can return subdivisions. Its discovery work executes in bounded workers so
HTTP does not become a serial preplanning bottleneck. `--jobs` bounds workers
and simultaneous requests; initially each worker performs at most one
request at a time. Each worker owns its HTTP/decoder/mask state.

Keep queued, active, and completed preparation work bounded, as today. Also
bound retained source bytes and avoid materializing the full candidate tree.
Use a depth-first frontier with lazy roots and bounded outstanding work.
Only the coordinator writes or links RF payloads and mutates the output
index. Complete each payload write/link before indexing it.

## Cache and restart

Validate source-specific `inputs.tmp` before source discovery or production.
Continue to accept compatible incomplete `.part` snapshots only. Published
snapshots lack the record and remain ineligible. Preserve existing GDAL input
record serialization/compatibility when moving it into the GDAL module.

Online input records include the parsed provider settings: URL template, Y
direction, both source zoom limits, and source dimensions. Also record RF
dimensions, mask identifier and
processing settings, attribution index and selected entry, RGB/decoding and
fallback policies, relevant decoder version, and a processing version. Jobs,
logging, output path, and operational retry settings do not change pixel
compatibility. A mismatch or corrupt/missing record aborts explicitly.

Compare provider field values rather than the JSON filename or serialized
text. Moving a provider file, changing whitespace, or reordering its keys
does not invalidate reuse. Changing its settings at the same path does.
The provider path may be logged for diagnostics but is not cache identity.

Online planning reads the immutable cache hierarchy:

- An indexed physical RF leaf is final. Link it without rediscovering,
  revalidating, or downloading its source imagery.
- A virtual ancestor with cached descendants is partially complete. Refine
  around those leaves before source work and plan only the unfinished regions.
  Never produce an overlapping parent over an already reused leaf.
- Missing entries are unfinished or empty; the cache does not distinguish
  them. Rechecking empty areas is acceptable. Unindexed payloads are ignored.

Requests for an ancestor or interpolation neighbour that also covers cached
space are allowed when required for an unfinished region. They do not cause
the cached RF tile to be rebuilt.

No downloaded-image or discovery journal survives the run. Completed indexed
RF tiles are the restart unit. Matching identifiers do not prove unchanged
remote content; cached and newly fetched imagery may represent different
provider updates. A fresh run without a cache refreshes everything. See the
[cache decision](../adr/0003-online-rf-cache-reuse.md).

## Progress, errors, and cancellation

Report estimated geographic completion, elapsed time, remaining time and UTC
finish time, plus written/reused RF tiles, request count and downloaded bytes.
There is no advance HTTP discovery/count pass. GDAL retains its counted
candidate progress.

For online estimates, assign additive area weights using the union of the
mask's conservative Web Mercator bounds, with overlapping bounds counted
once. Prepare the rectangle-based weighting structure once and reuse it;
do not perform exact polygon intersections for every progress update. Child
weights sum to their parent's weight. A region is complete only after its
payload is saved/linked or it is finished with no output. A 404 requiring
fallback is not completion until that fallback output is finished.

Estimate remaining duration from observed noncached completion throughput
and remaining weight. Cached coverage increases completion but must not be
treated as freshly downloaded work when estimating throughput. Initially show
unknown ETA until usable observations exist. Label percentage/time as
estimates; different regional zooms and sparsity can revise them substantially.
Update periodically even while no RF tile has just completed. The intent is
to distinguish hours from days with little extra computation, not predict a
precise finish time. Test that weights are not double counted on subdivision,
out-of-order completion, cache reuse, empty branches, or mask-bound overlap.

Use finite connection/request timeouts and a one-hour total retry deadline
per failing tile request, including request time and backoff. Start waits at
500 ms and double them after successive transient transport failures or
retryable HTTP statuses such as 429 and selected 5xx responses. Respect
Retry-After within the remaining deadline. Log failures, attempt numbers,
next waits, remaining retry budget and exhaustion to console and persistent log.
Nonretryable responses and exhausted retries abort. Only 404 means absence;
timeouts, authentication failures, server errors and undecodable images must
never silently select coarser data. Reject successful non-JPEG responses and
unexpected decoded dimensions.

Prefer retaining the existing cancellation policy: discard queued work,
finish active work within bounded request/retry limits, save successful final
tiles, checkpoint, and retain the unpublished `.part` snapshot. An active
subdivision result does not start new child work after cancellation. User
cancellation must not be confused with a network failure. Preserve first-error
reporting and abort-on-error publication rules. Empty successful imports
publish normally. Keep the two-minute checkpoint target and remove the input
record only during finalization immediately before publication.

## Dependency choices

Use existing libcurl with a small online-module wrapper. Make curl discovery
conditional on RF builder or tile-downloader being enabled, and link
`CURL::libcurl` to RF explicitly. Do not require RF to include old-tool headers.

Use existing OpenCV in-memory JPEG decoding, explicitly preserving raster
orientation and converting BGR to RGB. Validate JPEG format, decode success,
and expected dimensions. OpenCV 4.11.0 is already fetched by project CMake;
add `-DBUILD_JPEG=ON` in `cmake/SetupOpenCV.cmake` so its bundled libjpeg-turbo
source is built instead of finding a system JPEG package. Local fetched
OpenCV sources establish that this option bypasses `FindJPEG`; the current
local build otherwise uses system libjpeg. Verify the resulting dependency
configuration/linkage after rebuilding. No new stb or HTTP dependency is
needed. The explicit fetched-source requirement applies to JPEG.

## Proposed source layout

```text
src/rf_builder/
  main.cpp                    # Subcommands, logging, signals
  run.h / run.cpp             # Shared production lifecycle
  TilePool.h                  # Bounded work execution
  Mask.h / Mask.cpp           # Shared vector validity mask
  gdal/
    cli.h / cli.cpp
    build.h / build.cpp
    planning.h / planning.cpp
    TileWorker.h
    inputs.h / inputs.cpp
  tiles/
    cli.h / cli.cpp
    provider.h / provider.cpp # JSON validation and URL building; no preset registry
    build.h / build.cpp
    planning.h / planning.cpp
    TileWorker.h / TileWorker.cpp # Discovery/refinement and pixel assembly
    inputs.h / inputs.cpp
    HttpClient.h / HttpClient.cpp
    jpeg.h / jpeg.cpp

providers/
  basemap.json
  gataki.json
```

Place online progress weighting beside online planning; keep common logging
and time-estimation formatting in the coordinator. Add a separate helper file
only when the implementation warrants it. Share mask application directly;
do not introduce a general imagery/sampling framework.

## Implementation sequence and acceptance checks

1. **Extract the shared lifecycle and introduce subcommands.** Move current
   source-specific code under `gdal`, preserve input-record compatibility and
   existing GDAL pixel behavior, and add the bounded subdivision result to
   the coordinator. Update affected includes/CLI tests. Verify GDAL scalar,
   RGB, serial/parallel, mask, cache, cancellation, failure and publication
   regressions before integrating HTTP.
2. **Add provider JSON, HTTP and JPEG decoding.** Implement the strict provider
   parser and required `--provider` file option, add the two repository provider
   files, and include invocation/JSON examples in `tiles --help`. Test missing,
   unknown and invalid fields, and rejection of the removed source-setting CLI
   options. Copy only useful downloader helpers into `tiles`, without its
   compiled provider registry. Implement the explicit error/retry policy,
   set up curl independently, and force fetched JPEG. Exercise a local HTTP
   fixture for URL order/Y direction, missing levels, 404, retry success and
   exhaustion, malformed/non-JPEG responses, dimension mismatch, RGB channel
   order and orientation. Do not rely on a live provider for correctness tests.
3. **Implement incremental adaptive selection and preparation.** Add
   mask-bounded roots, source ancestry/404 handling, refinement, native pixel
   assembly and bilinear fallback. Test a Vienna-like fine island among
   coarse regions, asymmetric children, minimum/maximum limits, missing
   minimum tiles, stopping at a missing intermediate tile despite an existing
   deeper fixture, and strict absence of physical parent/descendant overlap.
   Include source/RF dimension ratios, rejection of a negative starting RF
   zoom, narrow masks, holes, seam neighbours, true coverage edges, black
   pixels and empty publication. Verify that fallback interpolation blends
   in linear light while native-resolution copies preserve decoded values.
4. **Integrate cache-aware planning and progress.** Test that cached RF leaves
   cause no source requests for their reconstruction; partially cached
   ancestors still fill missing regions without overlap; changed limits or
   settings at an unchanged provider path reject reuse, while moving or
   reformatting an unchanged provider file preserves compatibility;
   cancellation permits restart; output failures never
   publish. Use a request log to distinguish legitimate neighbour/fallback
   reads. Test bounded queued/active/completed work and source cache memory,
   area-weight accounting, out-of-order results, and ETA with cached regions.
5. **Validate the complete integration.** Build RF and `unittests_rfbuilder`
   in the existing terrain-builder configurations; run focused reader/mask,
   storage, RF and relevant retained downloader checks. Build RF with the old
   downloader disabled. Confirm JPEG comes from the CMake-fetched source.
   Run affected existing image-writer and texture-decoder tests because the
   bundled-JPEG option changes the shared OpenCV dependency configuration.
   Exercise parallel/network paths under available sanitizers. Run Qt C++
   lint and format only changed C++ sections with clang-format-21. Compare
   native pixel values against decoded fixture JPEGs, not their pre-JPEG
   originals. Measure mixed-resolution output expansion and estimator
   overhead. A small real-provider run is supplemental, bounded by a small
   mask, and must not substitute for deterministic fixtures.
6. **Update implemented documentation.** Revise the main RF command examples,
   implementation status and this plan after validation. Keep the downloader
   and make no unrelated storage-format, mesh, or GDAL filtering changes.

Before implementation, recheck the working tree and report existing edits.
No commits, branches, pushes, or downloader deletion are included in this
plan. Before any later commit, run the repository's required line-ending
check and review the exact changed files.

## Implemented operational details

- Each worker has a 64 MiB decoded/missing-result LRU budget. Missing entries
  are charged too. The in-flight response is capped at the larger of 1 MiB or
  eight bytes per configured source pixel plus 64 KiB. Decode dimensions are
  checked in the JPEG header before allocating the image.
- Total worker memory also includes the current response/decode buffers,
  bounded ancestor references, interpolation neighbourhood, mask geometry and
  RF output/validity buffers. The LRU budget is not a total-process RSS limit.
- HTTP connection timeout is 10 seconds and request timeout is 30 seconds,
  both clipped to the remaining one-hour retry deadline. Backoff doubles from
  500 ms. Retryable statuses are 408, 429, 500, 502, 503 and 504; selected
  transient curl transport errors also retry. Redirects are limited to five
  and HTTP(S). Retry-After accepts delay seconds and HTTP dates.
- Cancellation retains the existing finish-active-work policy. An active
  tile can therefore finish its current network retry deadline before the
  cancelled run exits. Queued candidates and returned subdivisions do not
  start new work after cancellation.
- There are twice as many scheduling lanes as workers. Each lane owns one
  outstanding candidate and a depth-first sibling stack, bounding the frontier
  by tile-key depth even when completions arrive out of order. Idle lanes take
  pending sibling subtrees, allowing parallel processing below a single root.
- Online input records include OpenCV's version and JPEG build identity, as
  well as the shared mask reader's GDAL version. Provider file paths and JSON
  formatting do not affect compatibility.

Provider GET verification on 2026-09-12 at Vienna (16.3738 E, 48.2082 N)
returned 256x256 JPEGs at Basemap zooms 1..20 and Gataki zooms 4..20. Zooms
outside those ranges among the tested 0..22 returned 404. This is a regional
availability probe, not a guarantee about every tile or future provider data.
