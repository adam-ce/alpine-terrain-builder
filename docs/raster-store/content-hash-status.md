# Envelope content hashes: status

Implementation checklist for the [content-hash plan](content-hash-plan.md).

- [x] Vendor xxHash and add `io/hash.h` with `hash::Algorithm` and `hash::data`
- [x] Switch `io::read_bytes_from_path` and `io::write_bytes_to_path` and their
      callers to `std::byte`
- [x] Reduce `io/compression.h` to `compress` and `decompress`; remove CRC-32C
      and the frame-checksum variants
- [x] Envelope: new magic, `Header` split, binary `hash`, enumerator range
      checks, name and hash limits, defaults, `read_header`; remove `Bytes`
- [x] Raster tiles: `TileCodec`, `tile_codec::from_name` and `CreateOptions`
      use `hash_algorithm`
- [x] RF merger: no hash option, `--compression` mapping, fingerprints from
      header hashes
- [x] Tests: envelope, header reads, hash vectors, benchmark; adapt existing
      suites
- [x] Documentation: storage format, ADR 0005, implementation status
- [x] Build, all CI suites, formatting (Linux GCC Debug, 2026-09-30)

Notes:

- xxHash is pinned at v0.8.3 and linked privately into terrainlib.
- `read_header` and `detail::validate_header` live in the new `io/envelope.cpp`.
  `deserialize` decodes the whole envelope in one pass and both paths validate
  the header with the same function. Only `read_header` decodes with an
  allocation limit of `max_header_size`; a corrupt length prefix can still
  cause a transient allocation of up to 1 GiB in `deserialize`, as before.
- `io::read_bytes_from_path` gained an optional maximum size for the header
  read.
- The image API (`io::image::encode`, `decode_rgb8`, `decode_rgba8`), the RF
  builder's `HttpClient` and its test server also use `std::byte`. `encode`
  copies once from OpenCV's `std::vector<uchar>`.
- Invalid enumerators in `hash::data` and `compress` fail with `PANIC`, which,
  unlike `UNREACHABLE`, also aborts with a message in release builds.
- XXH3-64 measured 18.3 GB/s over 1 GiB in a Release build, recorded in
  ADR 0005.
- Added an RF merger test: fingerprints equal the header hashes, survive
  recompression, and files without a hash are rejected.
- clang-format was applied to new files and functions. `envelope.h` keeps its
  existing, unformatted style, and one-line changes were left as they were.
