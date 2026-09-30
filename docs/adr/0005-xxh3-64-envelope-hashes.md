# Hash envelope payloads with XXH3-64

Envelopes store XXH3-64 (seed 0) of the uncompressed payload as 8 big-endian
bytes in their header, and readers verify it. The hash is both the integrity
check and a content identity that `io::envelope::read_header` exposes without
reading the payload, for TB dependency records and RF merger fingerprints.
Hashing uncompressed bytes keeps the identity independent of the Zstandard
settings. The header field is binary, not text, and `io::hash::Algorithm`
leaves room for longer or stronger hashes.

Single-threaded throughput on an AMD Ryzen 9 3900X over 1 GiB in memory:

| Algorithm | Output | Throughput |
|---|---:|---:|
| XXH3-64, `io::hash::data`, Release build | 64 bit | 18.3 GB/s |
| CRC-32C, SSE4.2 | 32 bit | 10.7 GB/s |
| SHA-1, SHA-NI (`sha1sum`) | 160 bit | 1.8 GB/s |
| SHA-256, SHA-NI (`sha256sum`) | 256 bit | 1.6 GB/s |
| FNV-1a 64 | 64 bit | 1.1 GB/s |
| BLAKE2b (`b2sum`) | 512 bit | 0.9 GB/s |
| MD5 (`md5sum`) | 128 bit | 0.8 GB/s |
| CRC-32C, former envelope table | 32 bit | 0.5 GB/s |

XXH3-64 was measured with the Catch2 benchmark in
`unittests/terrainlib/io_hash.cpp` at 58.7 ms mean over 100 samples, compiled
with the project's Release flags (`-O3`, no `-march`). A 4096-pixel float tile
with attribution is 96 MiB uncompressed, so the hash costs about 5 ms per write
or verified read, against a few hundred for Zstandard. With 64 bits and no
adversary, a changed payload keeps its hash with probability about 2⁻⁶⁴.

Rejected alternatives:

- FNV-1a, formerly used for RF merger fingerprints: about 90 ms per tile and
  weaker mixing.
- CRC-32C: 32 bits are too few for an identity, and the table implementation
  was the slowest option. It was removed rather than kept beside XXH3-64.
- The Zstandard frame checksum: only the low 32 bits of XXH64, stored at the
  end of the frame and unavailable for uncompressed payloads. The
  `…WithChecksum` compression variants were removed; frames are written
  without it.
- A snapshot UUID: identifies a write, not content, so identical tiles from
  different runs would not match.
- The SHA family: slower by an order of magnitude, with collision resistance
  that is not needed without an adversary.
