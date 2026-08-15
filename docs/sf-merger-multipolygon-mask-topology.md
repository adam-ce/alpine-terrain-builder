# Multipolygon Masks in the SF Merger

## Scope

This document describes how the SF merger represents and applies polygon masks.
It covers the part of the pipeline from repaired polygon components through
triangulation, extrusion, node-local mask clipping, and clipping of terrain
meshes. It also states the topology that these operations require and the
assumptions made about later mesh processing.

It does not describe the complete SF merge traversal, source dataset creation,
terrain serialization, texture generation, or the general DAG simplification
algorithm. Those systems are mentioned only where they constrain or consume the
result of mask clipping.

## Mask model

An SF mask has multipolygon set semantics: its covered region is the union of
all polygon components. A component consists of one outer boundary and any
holes belonging to that boundary.

The component boundary is also a topology boundary. Components remain separate
meshes even when they have equal coordinates at a point or along part of an
edge. Coordinate equality must not create shared vertices, shared edges, or
other mesh connectivity between components.

The mask passes through three representations:

- `SpherePolygonMask` contains the repaired polygon components and the local
  sphere projector used for triangulation.
- `SphereMeshMask` contains one triangulated surface mesh per polygon
  component.
- `MeshMask` contains one closed, extruded volume mesh per non-empty component.

The `components` collection in `SphereMeshMask` and `MeshMask` represents a
union. It is not an arbitrary collection of overlapping solids.

## Constructing mask volumes

### Repair and triangulation

The input polygon set is repaired before triangulation. Repair may normalize
rings and produce a different collection of valid polygons with holes, so the
components returned by repair are the components used by the rest of the
pipeline.

`mask::triangulate()` creates a separate constrained Delaunay triangulation for
each repaired polygon component. The outer ring and all of that component's
holes are constraints in the same triangulation. Only faces inside the polygon
and outside its holes are converted to the component surface mesh.

Separate triangulations are required even if two components touch. Inserting
both components into one triangulation would map coincident coordinates to the
same triangulation vertex and therefore introduce connectivity that is absent
from the multipolygon.

### Extrusion

`mask::extrude()` extrudes every triangulated component independently between
the padded minimum and maximum Earth radii. Each surface becomes its own closed
volume and is stored as one `MeshMask` component.

The volume components are not combined after extrusion. For example, if two
surface components meet at one point, combining their extrusion into one
indexed mesh would turn that point into a radial edge shared by both volumes.
Such an edge has more than two incident faces and is non-manifold.

In debug builds, each extruded component is checked as a valid polygon mesh,
checked for self-intersections, checked for closure, and checked to ensure that
it bounds a volume.

## Restricting a mask to an octree node

The SF merge traversal restricts the mask to padded node bounds before applying
it to the node's terrain. `clip_mask_on_bounds()` clips each volume component
against the bounds independently and drops empty results.

The result is another `MeshMask`, so the component boundary and union semantics
are preserved while descending the octree. Bounds clipping must never combine
the remaining components into a shared mesh.

An empty component list means that the mask covers none of the current node.
This permits the traversal to return the unmasked side without performing a
terrain Boolean operation.

## Applying a mask to terrain

`clip_on_mask()` implements the set operations against the union of all mask
components. The two modes deliberately use different iteration strategies.

### Keeping terrain inside the mask

The original terrain mesh is clipped independently against every component.
The non-empty results are then combined:

```text
inside(source, mask) = combine(inside(source, component) for each component)
```

If the source is entirely inside any component, the original mesh can be
returned unchanged. If the mask has no components, the inside result is empty.

The independent results may be combined because valid multipolygon components
have disjoint interiors. Combining them concatenates their geometry; it does
not weld coincident vertices between pieces.

### Keeping terrain outside the mask

The components are subtracted sequentially:

```text
result = source
for each component in mask:
    result = outside(result, component)
```

This computes the complement of the component union without constructing a
single clipping volume from geometrically touching components. If the mask has
no components, the original mesh is returned unchanged.

### Textures

All inside pieces originate from the same source mesh and continue to reference
its texture. After pieces are combined, unused texture data is trimmed. The
sequential outside operation likewise preserves the source texture through each
clipping step and trims the final owned result.

## Use by the masked merge

At a leaf, the masked merge retains:

- the new terrain inside the component union; and
- the base terrain outside the component union.

These results are complementary with respect to the mask, but either result may
legitimately be empty. A node whose mask has no positive-area overlap with the
new terrain can correctly reuse the complete base node. Conversely, a node
fully covered by one mask component can reuse the new node. Correctness therefore
requires a readable, non-empty result when substantial geometry should survive;
it does not require every boundary node to contain geometry from both inputs.

## Topology and numerical requirements

CGAL volume clipping assumes suitable polygon meshes. Before a component is
used as a volume clipper, it must be:

- a valid triangle polygon mesh;
- closed;
- manifold;
- free of self-intersections; and
- the boundary of a volume.

Using double-precision coordinates does not make these properties automatic.
In particular, a point contact is an exact topological relationship, not merely
a rounding problem. Duplicating a non-manifold vertex after components have
already been combined also leaves coincident surfaces that can violate Boolean
operation preconditions. Applying an epsilon offset would change the geographic
mask and make its behavior tolerance-dependent. Neither approach replaces
component preservation.

Boolean operations can introduce invalid output even when both inputs are
valid. Debug validation therefore checks CGAL terrain meshes and mask clippers
before mesh-on-mesh clipping and validates the resulting terrain mesh after the
operation. Tests also validate component volumes after bounds clipping, including
self-intersection checks.

## Boundary with downstream mesh processing

Clipped terrain is later clustered and simplified by the DAG builder. The mask
code does not control those algorithms, but it must provide them with separate
vertex identities for separate components, including components that occupy the
same coordinate at a point. Downstream processing may simplify geometry but
must not turn a point contact between disconnected pieces into non-manifold
connectivity or a self-intersection.

The regression coverage exercises the production clustering, simplification,
and manifoldization path for two disconnected meshes that meet at one point.
This documents the currently verified handoff; it is not a general guarantee
for arbitrary downstream mesh operations.

## Maintained invariants

Changes to SF mask processing must preserve these invariants:

1. Each repaired polygon-with-holes component is triangulated independently.
2. Each triangulated component is extruded into an independent closed volume.
3. Bounds clipping operates independently on every volume component.
4. A `MeshMask` component list represents the union of its components.
5. Inside clipping combines independent clips of the original source.
6. Outside clipping subtracts the components sequentially.
7. Separate components never share mesh connectivity solely because their
   coordinates touch.
8. Meshes passed to CGAL volume operations satisfy the required topology, and
   Boolean outputs are validated in debug builds.

## Regression coverage

The relevant tests are:

- `unittests/sf_merger/mask.cpp`: disconnected and point-touching synthetic
  masks, union and empty-mask semantics, and the real multipolygon fixture after
  node-bounds clipping.
- `unittests/sf_merger/integration.cpp`: an end-to-end SF build and merge across
  regular and point-touching mask borders.
- `unittests/dag_builder/multi_component.cpp`: the downstream simplification
  handoff for disconnected components that meet at one point.
