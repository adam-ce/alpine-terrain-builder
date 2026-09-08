# Store native raster pixel buffers inside the envelope

The first `.amort` format stores dimensions and the native element bytes of
the data and attribution rasters inside `io::envelope`. Supporting only x86
and the project's current unpadded GLM configuration lets one templated codec
preserve payload bits without per-component GLM serializers or an explicit
matrix of type instantiations. This binds the byte encoding to the supported
scalar representations and pixel layouts; it does not promise portability
to other layouts or architectures. Compile-time layout checks and read-time
byte-count checks enforce that representation, while the metadata retains the
payload-type identifier.
