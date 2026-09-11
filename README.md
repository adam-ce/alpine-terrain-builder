# alpine-terrain-builder

See the project [code style](docs/code-style.md) before contributing.

The [RF builder](docs/raster-store/rf-builder-design.md#command-usage) imports prepared
GDAL rasters into immutable raster-store snapshots.

Sanitizer exclusions are maintained in `misc/suppression/`. CI's ASan job also
enables UBSan and loads [ubsan.txt](misc/suppression/ubsan.txt) for the known CGAL
arrangement downcasts and the ignored old Boolean value in
`set_with_guarantees()`. These exclusions leave those upstream issues unfixed;
address checks, leak detection and unrelated undefined-behavior checks remain
enabled.

To use the same UBSan exclusions locally, set these variables from the repository
root before running an ASan test executable:

```sh
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1:suppressions=$PWD/misc/suppression/ubsan.txt"
```

Ensure `llvm-symbolizer` is available on `PATH`, or set `ASAN_SYMBOLIZER_PATH` to
its absolute path, so the function-specific exclusion can match.

CI's TSan job loads [tsan.txt](misc/suppression/tsan.txt), containing only the two
oneTBB initialization exclusions previously used by CI. For local TSan tests,
set this variable from the repository root:

```sh
export TSAN_OPTIONS="suppressions=$PWD/misc/suppression/tsan.txt"
```
