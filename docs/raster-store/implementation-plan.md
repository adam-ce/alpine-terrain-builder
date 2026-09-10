# Raster-store tile-storage implementation plan

Status: implemented and verified; see [implementation-status.md](implementation-status.md).
The accepted storage
contract is in [architecture.md](architecture.md) and
[storage-format.md](storage-format.md).

## Scope

Implement persistent tile storage in `terrainlib`, using the existing shared
store types. RF/TB builders, merging, spatial window reads, sampling, and
enforcing read-only access through the type system are separate work.

Track each stage and its verification evidence in
[implementation-status.md](implementation-status.md).

## Accepted implementation decisions

- Attribution JSON is a top-level array of complete objects with snake_case
  keys, numeric resolution in metres per pixel, and verbatim date strings.
  There is no clearing operation or special interpretation of cleared entries.
- Tile reads and writes do not scan attribution pixels to validate references.
  The later RF builder rejects attribution indices outside the table or the
  supported index range before producing tiles.
- Do not add raster allocation limits derived from the envelope's current
  size limit. Envelope limits remain an envelope concern.
- Fix flush and close error reporting in `io/bytes.cpp` for all callers.
- Shared-store changes are allowed where useful, retaining mesh-store
  compatibility. Keep general mechanisms in `store` and raster policy and
  format adapters in `raster_store`.
- Persist a raster codec name in metadata and resolve the reader from that
  string, independently of filename extensions. Preserve the existing mesh
  extension-based resolver. Use the existing resolver mechanism rather than
  adding a registry of hypothetical future codecs.

## Implementation sequence

1. Add the templated tile type and JSON attribution-table reader. Default
   attribution rasters to 0; keep every table slot a complete object. Implement
   the agreed three-location lookup, string dates, and strict index limit at
   table/API boundaries, without scanning tile attribution rasters.
2. Add the XYZ layout and separate raster-specific index and metadata
   envelopes. The index contains hierarchy entries; metadata contains layout,
   codec, payload type, and runtime tile dimensions. Raster-local opening
   reads both and configures the codec and existing `IndexedStorage`.
   Creation writes immutable snapshot metadata; checkpoints write only the
   hierarchy index through a temporary file followed by replacement.
3. Add the templated `.amort` codec for raster dimensions and native buffer
   bytes, with the agreed layout/byte-count checks and configurable envelope
   compression. Instantiate only types used by callers and focused tests.
4. Add create/open, explicit checkpointing, incomplete-snapshot opening, and
   publication. First fix shared byte writes to report flush and close errors.
   Publication saves the final index and closes the `.part` handle, then renames
   the directory to its final path and returns success. It does not reopen the
   snapshot or scan the payloads. Table copying supports self-contained
   snapshots without editing attribution entries.
5. Use the existing copy machinery for tiles the caller has decided can be
   reused. The caller owns payload type, dimension, encoding, and attribution
   compatibility decisions. Cross-root hard links remain supported on the
   same filesystem.

## Shared-store integration

Hard-link eligibility is a caller decision. No new codec compatibility hook
or automatic metadata-compatibility gate is planned for `copy_from()`.
Storage continues to report failures from the actual copy/link operation.

Dimensions are stored in the separately written metadata file. The index
writer remains a function pointer and persists only the current hierarchy;
it needs no capturing callback. Extend shared opening only as needed to use
already decoded metadata and the existing codec resolver, retaining existing
mesh entry points and selectors.

## Verification

Use focused tests for scalar and packed-GLM round trips, exact preservation
of payload bits, non-power-of-two square dimensions, attribution lookup and
slot-0 behavior, and the retained attribution-index limit. Exercise normal
envelope/adapter errors, dimensions retained across checkpoints, explicit
incomplete opening, publication, and rejected destination collisions.

Verify cross-root hard-link reuse with a copied local attribution table,
including continued TB reads after removing the source RF fixture. Verify
that existing shared-store and octree persistence/copy tests retain their
behavior. Run the relevant Linux build and tests through the configured
project workflow.

The renderer-only `dev_driver.py` rejects this repository. Use the existing
terrain-builder CMake build and this repository's Linux CI test configuration;
do not extend the renderer driver as part of raster storage.
