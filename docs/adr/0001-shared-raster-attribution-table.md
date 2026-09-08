# Share an attribution table across raster snapshots

RF snapshots normally share a source-attribution table. A TB receives an
independent copy beside its index with the same attribution numbering. This
allows compatible tiles to be hard-linked across RF and TB roots while the TB
can be transferred as one directory and remains valid after RF removal or
table edits. Lookup checks beside the index, then the index directory's parent
and grandparent, giving the nearest table precedence.

Independent numbering could assign the same index to different sources,
requiring attribution rasters to be rewritten when combining snapshots.
Preserving numbering avoids that rewrite. RF snapshots using a shared table
depend on it; a TB with its local copy is self-contained. Entries are maintained
manually and may be corrected or reused once the user knows that no retained
snapshot using that table refers to them. Removing an entry and shifting later
indices is forbidden. Tools do not edit entries; TB construction copies them.
