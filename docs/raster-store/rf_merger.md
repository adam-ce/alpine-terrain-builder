# DRAFT — `rf_merger`

Status: **draft archive of removed ideas**.

This is not a current implementation plan or accepted RF merge specification.
It is a lightly reformatted archive of subtree-copy and paired-walker material
removed from [refactor-plan.md](refactor-plan.md). The details are retained so
they are not lost; later discussion already established that RF semantics must
be designed before choosing these abstractions.

## Proposed names and source boundary

The removed proposal added these dimension-neutral files:

```text
src/terrainlib/store/
├── copy_subtree.h
└── merge/
    ├── Action.h
    └── walk.h
```

The proposed migration map was:

| Existing code | Removed target |
|---|---|
| `sf_merger::NodeWriter` subtree loop | `store/copy_subtree.h` |
| `sf_merger::Merger` recursion | `store/merge/walk.h` |

The proposal intended shared mechanisms to contain no mesh or raster merge
policy.

## One-node copy proposal

The proposed one-node API added:

```cpp
struct CopyOptions {
    bool force_reencode = false;
};
```

For one key, the proposed `Storage::copy_from()` behaviour was:

1. Call input and output `Codec::paths()` with the same fixed dummy
   `NodePath`, proposed as `__codec_probe__/node`.
2. Compare path lists exactly, including count, order, and filename endings.
3. When the lists match and `force_reencode` is false, call both codecs with
   the actual source and target `NodePath` and hard-link every corresponding
   file.
4. When lists differ or re-encoding is forced, read with the input codec and
   write with the output codec.
5. Update the target index only after all links or the write complete.

If a multi-file link failed partway through, the proposal removed target links
created by that call before returning `CopyError`. `CopyError` retained any
underlying `CodecError`. There was no silent file-copy fallback.

Codec settings that did not change `paths()`, such as compression level or
JPEG quality, did not force re-encoding by default. Callers could pass
`force_reencode = true`.

The removed hard-link rules were:

- never modify an existing linked payload in place;
- matching codec path lists hard-link every file;
- different path lists decode with the input codec and encode with the output
  codec;
- `force_reencode` selects decode/encode;
- hard-link failure is explicit;
- 2D snapshot tools preflight hard-link support before a long operation; and
- no silent file-copy fallback.

## Shared subtree-copy proposal

The removed API sketch was:

```cpp
std::expected<void, CopyError>
store::copy_subtree(
    const IndexedStorage& source,
    Storage& target,
    const Key& root,
    CopyOptions options = {});
```

The proposed operation:

1. traversed an indexed source subtree;
2. skipped `Virtual` nodes;
3. called `copy_from()` for physical payloads in `Leaf` and `Inner` states;
4. continued traversal below `Inner`; and
5. returned copy failures rather than asserting or terminating.

The operation was intended to be payload-neutral and know nothing about
meshes, rasters, masks, attribution, or their encodings.

The removed target call chain was:

```text
merge policy decides to keep a source subtree unchanged
    -> store::copy_subtree()
    -> store::Storage::copy_from()
    -> hard-link every codec path, or decode/encode
```

## Removed `Inner` copying rationale

The removed proposal used this topology model:

| Status | Physical payload | Indexed descendants |
|---|---:|---:|
| `Leaf` | yes | no |
| `Inner` | yes | yes |
| `Virtual` | no | yes |

It used this RF example:

```text
zoom 10 physical tile       -> Inner
└── zoom 11 physical tile   -> Leaf
```

The proposal said subtree reuse must preserve both payloads: copy the parent
payload, continue below it, and allow insertion of the descendant to promote
the copied parent from `Leaf` to `Inner` through normal index transitions.

The proposed status handling was:

```cpp
switch (status) {
case NodeStatus::Virtual:
    break;
case NodeStatus::Leaf:
case NodeStatus::Inner:
    target.copy_from(id, source, options);
    break;
}
```

This described unchanged-subtree copying only. It did not define how two RF
trees should be merged.

## Paired-tree walker proposal

The removed proposal extracted dimension-neutral recursion from
`sf_merger::Merger` and introduced these policy results:

```cpp
store::merge::Recurse<Context>
store::merge::Ignore
store::merge::KeepLeft
store::merge::KeepRight
store::merge::Write<Payload>
```

The walker owned recursion and unchanged-subtree reuse. The policy owned
selection and payload combination.

The removed proposal required all 16 combinations of `Missing`, `Leaf`,
`Inner`, and `Virtual` to be handled or rejected with a typed error rather than
falling into `UNREACHABLE()`.

It kept these concerns outside the shared walker:

- `NodeLoader` ancestor mesh reconstruction;
- ECEF node bounds;
- mesh masks and clipping;
- mesh combination and texture atlas generation; and
- mesh validation and auxiliary texture writes.

The proposal suggested that a future 2D merger could supply a raster policy.
Later discussion identified that the mutually exclusive action list cannot
express both an action for an `Inner` payload and recursion into descendants.
Ideas mentioned after that were a combined `WriteAndRecurse` result or
independent current-node and descendant decisions. None is selected.

## Removed implementation and verification ideas

The removed subtree/walker phase contained:

- compare input and output codec path lists for a fixed dummy `NodePath`;
- hard-link all files when lists match, including partial-failure cleanup;
- decode with the input codec and encode with the output codec when lists
  differ;
- add `CopyOptions::force_reencode`;
- add the shared unchanged-subtree copier;
- add the paired hierarchy walker and typed actions;
- cover all 16 status pairs with table-driven tests;
- adapt the 3D merger while keeping mesh policy in `sf_merger`;
- remove generic recursion and copy logic from `sf_merger::Merger` and
  `NodeWriter`; and
- add a 3D integration test for an unchanged hard-linked subtree and a newly
  written changed boundary node.

The removed focused tests included:

- one-file hard linking;
- multi-file hard linking;
- different path counts and endings;
- forced re-encoding with otherwise equal paths;
- conversion between terrain and glTF;
- conversion into a write-only codec;
- runtime failure for an unsupported codec operation;
- copies containing `Leaf`, `Virtual`, and `Inner`; and
- all 16 paired status combinations.

The proposed generic test file was:

```text
unittests/terrainlib/store_merge_walk.cpp
```

## Removed error and risk notes

The removed error-propagation proposal passed copy failures through
`copy_subtree()` and the merge call chain to the application boundary with the
affected key and path. Codec, filesystem, unsupported-conversion,
malformed-dataset, and overwrite failures used `std::expected`; operational
failures were not assertions or intentional exceptions.

| Removed risk | Removed control |
|---|---|
| `Inner` payloads are lost during subtree reuse | Copy every physical status and test mixed-depth fixtures |
| Multi-file hard linking fails partway through | Remove links created by the failed `copy_from()` before returning |
| Incompatible codecs return the same path list | Treat path-list equality as a codec contract and test every concrete codec pairing |
| Output-only codec is selected for required input | Return a clear `UnsupportedOperation` error |
| Shared code accumulates mesh/raster policy | Dependency tests/review against the source boundary |

All material in this document remains provisional despite the concrete names
preserved above.
