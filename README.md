# alpine-terrain-builder

See the project [code style](docs/code-style.md) before contributing.

The RF import, merge, and image-export tools live in
[tile-foundry](https://github.com/AlpineMapsOrg/tile-foundry).
This repository provides their `terrainlib` dependency, including raster-store.

When included as a CMake subproject, tools and tests default to disabled.
Enable individual `ALP_BUILD_*` options explicitly when needed.

## Optional ECW input

The project-managed GDAL build leaves ECW disabled by default. To enable it,
obtain an ECW SDK licensed for your use and pass these options when configuring
this project (or tile-foundry):

```sh
cmake -S . -B build-ecw -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DALP_GDAL_ENABLE_ECW=ON -DECW_ROOT=/absolute/path/to/Desktop_Read-Only
```

Keep the SDK outside the source and build trees. `ECW_ROOT` points to the SDK
directory containing `include/NCSECWClient.h`.
It may also be supplied as an environment variable. GDAL 3.10.3's SDK finder
supports 3.3 and 5.5 or newer; on Linux, the 5.5 SDK layout includes
`lib/cpp11abi/x64/release/libNCSEcw.so`. For a nonstandard layout, pass
`ECW_INCLUDE_DIR` and `ECW_LIBRARY` explicitly. The SDK is not downloaded or
redistributed by this build. Review the licence bundled with your SDK: the
5.5 Desktop Read-Only licence permits desktop decoding, not server applications
or encoding; do not assume a current SDK has the same terms.

Enabling ECW explicitly requires SDK discovery to succeed; a missing SDK fails
configuration. The ECW driver is built into GDAL. Installed GDAL libraries retain
the PROJ runtime path and add external link directories to their runtime search
path, including the SDK library directory on Linux. Keep the SDK at that path;
on Windows its DLL directory must be on `PATH`. Changing these options or SDK
paths invalidates the managed GDAL build cache. After replacing an SDK in place,
use a fresh build directory.

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

CI's TSan job loads [tsan.txt](misc/suppression/tsan.txt), containing the two
oneTBB initialization exclusions and GDAL's unlocked cache-head comparison in
`GDALRasterBlock::Touch()`. The GDAL race is accepted without patching the
dependency; the suppression matches any race with that function in its stack.
For local TSan tests, set this variable from the repository root:

```sh
export TSAN_OPTIONS="suppressions=$PWD/misc/suppression/tsan.txt"
```
