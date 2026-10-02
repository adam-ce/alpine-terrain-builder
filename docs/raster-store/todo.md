# Raster store TODO

- Consider extending `sf::validate_index()` beyond rejecting `Inner` after
  additional SF invariants and their required error reporting are defined.
  This is not part of the shared-store refactor.
- Idea: store each tile's XXH3-64 content hash (see the
  [hash decision](../adr/0005-xxh3-64-envelope-hashes.md)) in its raster-store index entry.
  TB dependency checks would become in-memory lookups instead of one envelope
  header read per source tile, which matters for millions of tiles and on
  network filesystems. Saving a tile knows the hash from the envelope write,
  and `copy_from` takes it from the source index. The cost is 8 bytes per
  entry, about 1.6 times the current index size. Either the generic
  `store::Index` carries per-node data beyond `NodeStatus`, or the raster
  store keeps a separate hash table beside it. Header reads remain available
  for verifying or rebuilding an index. Decide with the TB builder design that
  defines the dependency records.
- Design derived-channel generation (shading, slope, PLaTSA, AO). It is not
  part of the TB builder, which converts one RF into one TB without computing
  channels. A separate tool is expected to produce an RF-like input for the TB
  builder;