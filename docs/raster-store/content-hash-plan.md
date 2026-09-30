# Envelope content hashes: plan

Status: plan, 2026-09-30. Agreed; no decisions are open. Per-tile hashes in
the raster-store index are deferred; see the [TODO](todo.md).

## Motivation

TB tiles should record which source tiles produced each lower zoom level, so
that a data change triggers only a partial rebuild. That needs a cheap content
identity per tile, without separate per-tile metadata files that would slow
reading. The RF merger needs the same for its recovery fingerprints of input
metadata and index files, which it currently computes by hashing the whole
files with a local 64-bit FNV-1a.

The current envelope checksums do not provide such an identity:

- `HandledByCompressionLib` is the Zstandard frame checksum, the low 32 bits of
  XXH64, stored at the end of the frame rather than in the envelope header.
- `Crc32c` is 32 bits, computed with a byte-wise table at about 0.5 GB/s.
- `None` stores nothing.

## Hash

Use XXH3-64 (`XXH3_64bits`, seed 0) over the uncompressed payload bytes.
Hashing uncompressed bytes makes the identity independent of the Zstandard
settings and of recompression; hard-linked payloads share it.

Measured single-threaded on an AMD Ryzen 9 3900X over 1 GiB in memory:

| Algorithm | Output | Throughput |
|---|---:|---:|
| CRC-32C, SSE4.2 | 32 bit | 10.7 GB/s |
| SHA-1, SHA-NI (`sha1sum`) | 160 bit | 1.8 GB/s |
| SHA-256, SHA-NI (`sha256sum`) | 256 bit | 1.6 GB/s |
| FNV-1a 64 | 64 bit | 1.1 GB/s |
| BLAKE2b (`b2sum`) | 512 bit | 0.9 GB/s |
| MD5 (`md5sum`) | 128 bit | 0.8 GB/s |
| CRC-32C, envelope table | 32 bit | 0.5 GB/s |

XXH3-64 measured 18.3 GB/s in a Release build (see
[ADR 0005](../adr/0005-xxh3-64-envelope-hashes.md)). A 4096-pixel float tile
with attribution is about 96 MiB uncompressed, so FNV-1a would add about 90 ms
to every write and verified read, XXH3 about 5 ms. XXH3 also mixes much better than FNV-1a. With 64 bits, a changed
payload keeps its hash with probability about 2⁻⁶⁴; there is no adversary, and
each check compares against one recorded value.

Vendor xxHash (header-only, BSD-2-Clause) with `alp_add_git_repository` at a
pinned release. The copy bundled in Zstandard cannot be reused: it disables
XXH3 and prefixes its symbols with `ZSTD_`.

Add `io/hash.h` with the enumeration `io::hash::Algorithm { None, Xxh3_64 }`
and `io::hash::data(std::span<const std::byte> bytes, Algorithm algorithm)`,
returning the hash as `std::vector<std::byte>`: empty for `None` and 8 bytes
in big-endian order, the canonical xxHash form, for `Xxh3_64`. A value
outside the enumeration is a programming error and fails an `ASSERT`. The
algorithm-specific functions stay in `io/hash.cpp`, the only file that
includes xxHash. Longer or stronger hashes are added later as new `Algorithm`
values.

## Envelope changes

1. Change the envelope magic. Envelope files of all stores change: raster
   tiles, metadata and index, mesh and octree stores, DAG files and tool
   records. Existing files are no longer readable. No real datasets exist yet,
   so no migration is needed.
2. Replace `ChecksumAlgorithm` with `io::hash::Algorithm` and renumber
   `CompressionAlgorithm` from zero as
   `{ None, ZstdBestCompression, ZstdDefaultCompression }`. `Crc32c`,
   `HandledByCompressionLib`, `ZstdBestCompressionWithChecksum` and
   `ZstdDefaultCompressionWithChecksum` are removed, together with the CRC-32C
   implementation. `checksum_algorithm` parameters and fields become
   `hash_algorithm`.
3. The envelope field `checksum` becomes `hash` of type
   `std::vector<std::byte>`, holding `hash::data` of the uncompressed payload.
   The reader recomputes it and compares; a mismatch, including a hash of the
   wrong length, is `CorruptData`. The Zstandard algorithms write frames
   without the frame checksum; the reader no longer requires one.
4. Separate compression from hashing. Combining them saves no work: the
   current `compress_with_checksum` already hashes and compresses in two
   passes, and a fused streaming pass would save at most a few milliseconds
   per tile against a few hundred for Zstandard. `io/compression.h` keeps only
   `CompressionAlgorithm`, `compress(data, algorithm)` returning the
   compressed bytes and `decompress(data, algorithm, max_size)`;
   `CompressedData`, `compress_with_checksum` and `checked_decompress` are
   removed. Both functions keep returning `Expected` for Zstandard and
   size-limit failures, but an algorithm outside the enumeration fails an
   `ASSERT`. The envelope computes the payload hash with `hash::data` when
   writing and verifies it after decompressing; `io/hash.h` keeps xxHash out
   of `envelope.h`.
5. Every hash algorithm is valid with every compression algorithm, including
   `None` with `None`, which stores no integrity check. The combination check
   goes. Enumerator values read from files are untrusted, so the envelope
   checks both ranges while decoding the header, before `hash::data` or
   `decompress` sees them; a value outside the enumeration is `Unsupported`.
   A `Header` returned by the envelope therefore always holds valid
   enumerators.
6. Split the envelope into `Header` (magic, class name, class version, hash
   algorithm, hash, compression algorithm, uncompressed size) and
   `Envelope { Header header; std::vector<std::byte> compressed_data; }`.
   zpp_bits serializes the nested aggregate field by field, so this does not
   change the layout. Class names are limited to 128 bytes by a
   `static_assert` in `PayloadSchema`, hashes to 64 bytes. `max_header_size`
   is derived from these limits and the field sizes.
7. Make `Xxh3_64` with `ZstdDefaultCompression` the default for `serialize`,
   `write_to_path`, `TileCodec`, `tile_codec::from_name` and the raster-store
   `CreateOptions`. `terrainlib/store` does not handle envelopes; its codecs
   call `io::envelope` directly, so every envelope-based store, including the
   mesh stores, gets the new default without store changes.
8. Add `io::envelope::read_header(path)`, returning the `Header`. It reads at
   most `max_header_size` bytes; a shorter file is fine as long as it contains
   the header, and payload bytes in the read prefix are ignored. It decodes
   with a zpp_bits allocation limit of `max_header_size`, because zpp_bits
   resizes containers before checking their length against the input. A header
   that fails to decode is `CorruptData`. It validates the magic and the
   enumerator ranges, but not the class name. Callers take the recorded
   payload hash from `Header::hash`; it is neither read from nor verified
   against the payload.

## Byte buffers

Remove the `io::envelope::Bytes` typedef; its users spell
`std::vector<std::byte>`. Byte buffers use `std::byte` instead of `uint8_t`
where they hold raw bytes: `io::read_bytes_from_path` returns
`std::vector<std::byte>`, and `io::write_bytes_to_path` takes
`std::span<const std::byte>`. Their callers switch accordingly, including
`io/image.cpp`, the JSON readers of the attribution table and the RF merger
priorities, the RF tile provider and the tests. Casts move to interfaces that
require other types, such as OpenCV buffers; `envelope.h` no longer needs one.

## Raster tiles

Raster-store tiles keep a configurable hash algorithm in `CreateOptions`, with
`Xxh3_64` as the default. A comment at that default states that the tile
hashes serve as content identities, for example for TB dependency records, and
that `None` leaves tiles without one. Tiles that `copy_from` hard-links keep
the hash of their source.

The RF merger has no hash option. `merge::Options::checksum_algorithm` is
removed, and the merger uses the `CreateOptions` default. `--compression` maps
`zstd`, `zstd-best` and `none` to `ZstdDefaultCompression`,
`ZstdBestCompression` and `None`; its help text no longer mentions CRC32C.

## RF merger

Replace the FNV-1a fingerprints with the header hashes of
`raster_store.metadata` and `raster_store.index`, read with `read_header`.
`Fingerprint` keeps the canonical path and stores both hashes as
`std::vector<std::byte>`. A file whose header records no hash is an
`Unsupported` error, because an empty hash would match any content. The source
attribution table stays out of the fingerprint. Remove the local hash.

## Documentation

- Update the envelope and tile codec sections of
  [storage format](storage-format.md).
- Add ADR 0005 recording the XXH3-64 decision, the measurements and the
  rejected alternatives: FNV-1a, CRC-32C, the Zstandard frame checksum, a
  snapshot UUID and the SHA family. It also records the removal of CRC-32C and
  of the Zstandard frame checksum variants, and the binary hash field.
- Update [implementation status](implementation-status.md) after
  implementation.

## Verification

- `hash::data` with `Xxh3_64` against known xxHash test vectors.
- Envelope: round trips for every compression and hash combination, rejection
  of the old magic and of unknown enumerator values by both `deserialize` and
  `read_header`, detection of corrupted payloads with `Xxh3_64` and of hashes
  with the wrong length.
- Header reads: a class name at the length limit, a file with a short class
  name and an empty payload, a truncated header, and a corrupt length prefix,
  which fails without a large allocation.
- The header hash agrees with `hash::data` of the payload from a full read,
  and is empty for `None`.
- A Catch2 benchmark of `hash::data` with `Xxh3_64` on the same machine and
  data size as the table above, with the project's build flags.
- Raster store and RF merger suites, then all CI suites; tests that compare
  envelope bytes or sizes may need updates.
