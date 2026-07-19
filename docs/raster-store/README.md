# Raster store design

This directory describes a proposed authoritative raster store and the
generation of delivery tile pyramids from it. The documents are a design
baseline, not a finalized binary-format specification.

The documents distinguish three kinds of statement:

- **Confirmed** records a requirement or decision established in the design
  discussion.
- **Proposed** records the current recommendation and is subject to review.
- **Open** records a question that must be resolved before the affected part
  is implemented.

## Documents

- [Requirements and terminology](requirements.md)
- [Status quo and reuse assessment](status-quo.md)
- [Architecture](architecture.md)
- [Storage format](storage-format.md)
- [Sampling and pyramid generation](sampling-and-generation.md)
- [Decisions and open questions](decisions-and-open-questions.md)

## Scope

The authoritative store holds Web Mercator raster data at the best available
quality, together with exact per-pixel source attribution. It is separate from
delivery tile pyramids, which are filtered, formatted, and regenerated for a
particular consumer.

The current scope includes raster imagery of different kinds, including
height rasters used to generate geometry. It does not define vector storage,
rendering styles, mesh formats, or a final source-ranking policy.
