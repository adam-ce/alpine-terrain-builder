# SF Merger Failure with Point-Touching Multipolygon Masks

## Summary

The SF merger can produce an empty merged node even when both input nodes contain significant geometry. The known reproducer uses the Tirol mask and the level-15 node at:

```text
15/26290/18610/27235
```

The failure is caused by the topology of the extruded mask, not by missing input geometry and not primarily by terrain serialization. Two components of the input multipolygon touch at one point. They are triangulated together, so the shared point becomes a shared mesh vertex. Extrusion turns that vertex into an edge with four incident faces, making the mask non-manifold. Subsequent clipping treats the damaged mask as a solid and removes both input meshes. The terrain serializer then writes the resulting empty mesh, which exposes a separate empty-mesh decoding limitation.

The proposed fix is to preserve polygon components as independent closed volumes throughout triangulation, extrusion, recursive mask clipping, and mesh clipping.

## Reproducer and CI Symptoms

The reproducer is in:

```text
unittests/data/sf_builder_merge_border
```

It builds two `.terrain` datasets, merges them using the Tirol boundary, and checks two adjacent mask-border nodes. The node at `15/26291/18610/27235` merges successfully. The node at `15/26290/18610/27235` is written as a malformed `.terrain` file.

Normal GCC and Clang unity configurations reach the final test assertion and report:

```text
Malformed mask-border merge: invalid file format
```

ASAN and TSAN configurations fail earlier while loading and extruding the mask:

```text
DEBUG_ASSERT(is_manifold(mesh))
```

These are two observations of the same mask-topology defect. Debug configurations detect the invalid mask immediately, while release configurations continue until the invalid clipping result is serialized.

## Confirmed Diagnosis

### The source nodes contain substantial geometry

The failing merge processes two leaf nodes. Immediately before clipping, their sizes are approximately:

| Input | Vertices | Faces |
| --- | ---: | ---: |
| New node | 581 | 1,000 |
| Base node | 581 | 998 |

The empty merged result is therefore not explained by empty or insignificant source data.

### Two mask components touch at one point

The Tirol fixture is a valid three-part `MultiPolygon`. Its first and second components have a distance of zero and intersect at exactly one point:

```text
POINT(183502.2 410278.98)
```

Point-touching components are valid polygonal input, but they must not be converted into a single non-manifold surface mesh.

### Joint triangulation welds the touching components

`mask::triangulate()` inserts all polygon components into one constrained Delaunay triangulation. Coincident polygon vertices map to the same CDT vertex handle. The two components that touch at the point above therefore share one vertex in the triangulated surface.

This converts two topologically separate polygon components into one surface with a shared vertex.

### Extrusion creates a non-manifold edge

`mask::extrude()` extrudes the jointly triangulated surface between the minimum and maximum mask radii. The shared surface vertex becomes a radial edge in the extruded volume. Both components contribute side walls to that edge, leaving four incident faces where a manifold edge must have two.

Inspection of the full extruded mask found:

| Property | Value |
| --- | ---: |
| Vertices | 22,670 |
| Faces | 45,332 |
| Boundary edges | 0 |
| Edges with four incident faces | 1 |

The mask is closed in the boundary-edge sense, but it is not manifold because of that single four-face edge. This is the condition caught by the sanitizer configurations.

### Recursive clipping turns the invalid mask into an open mask

The masked merger recursively clips the current mask to each octree node's padded bounds using `mesh::clip_on_bounds_and_cap()`. CGAL's volume-clipping operation requires a valid closed input surface. Continuing with the non-manifold extrusion violates that expectation.

At the failing leaf, the node-local clipping mask has only 11 vertices and 9 faces, with 11 boundary edges. It is open and cannot reliably classify points as inside or outside a volume.

### Both clipping operations incorrectly return empty geometry

The merger applies complementary operations:

- retain the new mesh inside the mask;
- retain the base mesh outside the mask.

Because the clipping mesh is open and non-manifold, CGAL removes all geometry in both operations. The special cases in `Masked::merge_meshes()` do not apply because both results are newly owned empty meshes. The code combines them and returns `Merged` with zero vertices and zero faces.

### Terrain decoding is a downstream symptom

Terrain encoding permits a mesh with zero vertices and writes no encoded position buffer. Terrain decoding unconditionally calls `meshopt_decodeVertexBuffer()`, even when the serialized vertex count and position-buffer size are both zero. Meshoptimizer returns `-2`, which is reported as `invalid file format`.

Making the decoder accept an empty mesh would not fix this regression. The integration test correctly requires the merged node to be non-empty, and accepting the empty file would merely hide the geometry loss.

## Proposed Solution

### Preserve mask components as independent volumes

The mask representation should retain the component structure of the source `MultiPolygon`. Each `PolygonWithHoles` should be processed independently:

1. Create a separate constrained Delaunay triangulation for the component.
2. Convert that triangulation to its own surface mesh.
3. Extrude the surface into its own closed volume.
4. Validate that the volume is closed, manifold, and free of self-intersections.

Point-touching components will then remain topologically independent instead of sharing a vertex or radial edge.

`SphereMeshMask` and `MeshMask` should represent a collection of component meshes rather than one combined `SimpleMesh`.

### Clip each mask component independently

Recursive clipping to octree bounds should operate on each mask volume independently:

1. Clip every component to the padded node bounds.
2. Validate or assert the topology required by the following CGAL operation.
3. Remove empty components.
4. Pass the remaining component collection into the child context.

This prevents a failure in one component from corrupting the complete mask and avoids recreating shared topology while descending the octree.

### Apply multipolygon set semantics to terrain clipping

Clipping against multiple volumes must preserve the union semantics of a multipolygon.

For `keep_inside`, retain the part of the source mesh inside each component and combine the retained pieces. Valid multipolygon components have disjoint interiors, so the resulting pieces should not overlap in area.

For `keep_outside`, subtract the components sequentially:

```text
result = source
for each mask component:
    result = result outside component
```

This computes the complement of the component union without first constructing a geometrically touching combined clipping surface.

Texture coordinates and texture ownership must be preserved while combining inside pieces. The implementation should avoid rebuilding a texture atlas when all pieces still reference the same source texture.

## Alternatives Considered

### Accept empty terrain meshes

The terrain decoder could special-case a zero vertex count and skip Meshoptimizer decoding. That would make the file readable but leave the merged tile empty. It does not address the regression and should not be treated as its fix.

### Ignore empty clipping results

Adding a branch that chooses one input when both clipped results are empty would conceal the clipping failure and arbitrarily select incorrect data at the mask boundary.

### Duplicate non-manifold vertices after extrusion

CGAL provides utilities for duplicating non-manifold vertices. This may repair combinatorial adjacency, but the duplicated components would still occupy the same geometric radial edge. Such coincident surfaces can still violate corefinement and clipping preconditions. It is therefore not a robust replacement for preserving components through the complete pipeline.

### Perturb or buffer the input mask

Moving the shared point or buffering the polygon could avoid the reproducer, but it changes the requested geographic boundary and makes correctness depend on an arbitrary tolerance. Valid point-touching multipolygons should be supported without modifying their geometry.

## Verification Plan

The fix should be verified at three levels.

### Mask topology tests

- Load the existing Tirol fixture.
- Confirm that it produces three independent component volumes.
- Confirm that every volume is closed and manifold.
- Check for self-intersections before using a volume as a CGAL clipper.
- Confirm that recursive clipping to the failing node's bounds preserves valid topology.

### Focused clipping regression

For node `15/26290/18610/27235`:

- confirm that both source meshes are non-empty before clipping;
- confirm that the retained new-inside and base-outside pieces are not both empty;
- confirm that the final merged mesh is non-empty and valid.

With a valid component-wise mask, this node has no positive-area contribution
inside the mask: the new side retains 0 of 1,000 triangles and the base side
retains all 998 triangles. Reusing the base node is therefore a correct result.
The regression must not require this particular output to differ from both
inputs; it must require that the substantial result remains readable and
non-empty.

### End-to-end and configuration coverage

- Run the existing SF builder/merger integration test.
- Confirm that both adjacent output tiles load successfully and contain geometry.
- Run the normal GCC configuration.
- Run ASAN and TSAN configurations to ensure the earlier manifold assertion is also resolved.

## Expected Outcome

After the change, valid point-touching multipolygon components remain independent clipping volumes. The merger can correctly select geometry on both sides of the mask boundary, the failing node remains substantial (in this case by correctly reusing the base node), and no empty terrain file is produced. The same design also supports disconnected islands and other valid multipolygon inputs without welding their topology together.
