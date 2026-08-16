#!/usr/bin/env bash

# Repeatable, non-unit end-to-end verification for the current 3D SF/DAG path.
# Inputs are prepared once under INPUT_ROOT. Completed steps are skipped through
# explicit state markers, so an interrupted SF or DAG run can be resumed.

set -Eeuo pipefail

readonly SOURCE_DIR="${SOURCE_DIR:-/home/codex/Documents/alpine-terrain-builder/terrain-builder-raster-store}"
readonly BUILD_DIR="${BUILD_DIR:-${SOURCE_DIR}/build/golden-e2e}"
readonly RUN_ROOT="${RUN_ROOT:-/data/scratch/codex/alpine-terrain-builder-golden-e2e}"
readonly INPUT_ROOT="${INPUT_ROOT:-${RUN_ROOT}/inputs}"
readonly REFERENCE_ROOT="${REFERENCE_ROOT:-${RUN_ROOT}/reference}"
readonly LOG_ROOT="${LOG_ROOT:-${RUN_ROOT}/logs}"
readonly STATE_ROOT="${STATE_ROOT:-${RUN_ROOT}/state}"
readonly RUN_ID="${RUN_ID:-$(date +%Y%m%dT%H%M%S)}"
readonly RUN_LOG_ROOT="${LOG_ROOT}/runs/${RUN_ID}"
readonly TIMINGS_FILE="${RUN_LOG_ROOT}/timings.tsv"

readonly SF_BUILDER="${BUILD_DIR}/src/sf_builder/sf-builder"
readonly SF_MERGER="${BUILD_DIR}/src/sf_merger/sf-merger"
readonly DAG_BUILDER="${BUILD_DIR}/src/dag_builder/dag-builder"

# These VRTs are identical 4x4 km, native-resolution windows into the GS and
# GT rasters. They are recreated from the raw datasets at the start of a run.
readonly RAW_GS="/data/raw/raster_data/Oe_2020/OeRect_01m_gs_31287.img"
readonly RAW_GT="/data/raw/raster_data/Oe_2020/OeRect_01m_gt_31287.img"
readonly ELEVATION_ROOT="${ELEVATION_ROOT:-${INPUT_ROOT}/grossglockner-elevation-4km}"
readonly GS_VRT="${ELEVATION_ROOT}/grossglockner-gs-4km.vrt"
readonly GT_VRT="${ELEVATION_ROOT}/grossglockner-gt-4km.vrt"
readonly BASEMAP_TILES="${INPUT_ROOT}/basemap"
readonly GATAKI_TILES="${INPUT_ROOT}/gataki"
# A 0.01 m simplification preserves the Tirol border to sub-centimetre area
# accuracy while avoiding the exact geometry's pathological merge time.
readonly TIROL_MASK="${INPUT_ROOT}/tirol-boundary/benchmark-shape/0_01m/tirol.shp"

readonly SF_ROOT="${REFERENCE_ROOT}/sf"
readonly DAG_ROOT="${REFERENCE_ROOT}/dag"
readonly SF_BASEMAP="${SF_ROOT}/grossglockner-basemap-gs-terrain"
readonly SF_GATAKI="${SF_ROOT}/grossglockner-gataki-gt-terrain"
readonly SF_MERGED="${SF_ROOT}/grossglockner-merged-terrain"
readonly DAG_MERGED="${DAG_ROOT}/grossglockner-merged-terrain"

readonly SF_TARGET_LEVEL="${SF_TARGET_LEVEL:-15}"
readonly SF_THREADS="${SF_THREADS:-12}"
readonly MIN_TEXTURE_LEVEL=12
readonly MAX_TEXTURE_LEVEL=19

mkdir -p "${SF_ROOT}" "${DAG_ROOT}" "${RUN_LOG_ROOT}" "${STATE_ROOT}"
exec > >(tee -a "${RUN_LOG_ROOT}/golden-e2e.log") 2>&1

timestamp()
{
    date --iso-8601=seconds
}

run_timed()
{
    local name="$1"
    shift

    local start_epoch end_epoch elapsed status
    start_epoch="$(date +%s)"
    printf '[%s] START %s\n' "$(timestamp)" "${name}"

    set +e
    "$@"
    status=$?
    set -e

    end_epoch="$(date +%s)"
    elapsed=$((end_epoch - start_epoch))
    printf '%s\t%s\t%s\t%s\t%s\n' \
        "${name}" "${start_epoch}" "${end_epoch}" "${elapsed}" "${status}" \
        >> "${TIMINGS_FILE}"
    printf '[%s] END %s status=%s elapsed_seconds=%s\n' \
        "$(timestamp)" "${name}" "${status}" "${elapsed}"

    if ((status != 0)); then
        return "${status}"
    fi
}

require_file()
{
    if [[ ! -f "$1" ]]; then
        printf 'Required file is missing: %s\n' "$1" >&2
        exit 2
    fi
}

require_directory()
{
    if [[ ! -d "$1" ]]; then
        printf 'Required directory is missing: %s\n' "$1" >&2
        exit 2
    fi
}

require_executable()
{
    if [[ ! -x "$1" ]]; then
        printf 'Required executable is missing: %s\n' "$1" >&2
        exit 2
    fi
}

verify_snapshot()
{
    local snapshot="$1"
    local extension="$2"
    local payload_count

    require_file "${snapshot}/terrain.index"
    if [[ ! -s "${snapshot}/terrain.index" ]]; then
        printf 'Index is empty: %s\n' "${snapshot}/terrain.index" >&2
        return 1
    fi

    payload_count="$(find "${snapshot}" -type f -name "*${extension}" | wc -l)"
    if ((payload_count == 0)); then
        printf 'No %s payloads found in %s\n' "${extension}" "${snapshot}" >&2
        return 1
    fi
    printf 'Verified %s: %s payloads\n' "${snapshot}" "${payload_count}"
}

build_sf()
{
    local name="$1"
    local dataset="$2"
    local textures="$3"
    local output="$4"
    local marker="${STATE_ROOT}/${name}.complete"

    if [[ -f "${marker}" ]]; then
        printf '[%s] SKIP %s: completion marker exists\n' "$(timestamp)" "${name}"
        verify_snapshot "${output}" ".terrain"
        return
    fi

    mkdir -p "${output}"
    run_timed "${name}" \
        "${SF_BUILDER}" \
        --dataset "${dataset}" \
        --textures "${textures}" \
        --min-texture-level "${MIN_TEXTURE_LEVEL}" \
        --max-texture-level "${MAX_TEXTURE_LEVEL}" \
        --mesh-srs EPSG:4978 \
        --verbosity info \
        batch \
        --target-level "${SF_TARGET_LEVEL}" \
        --output "${output}" \
        --format .terrain \
        --threads "${SF_THREADS}"

    verify_snapshot "${output}" ".terrain"
    touch "${marker}"
}

merge_sf()
{
    local marker="${STATE_ROOT}/merge_sf.complete"

    if [[ -f "${marker}" ]]; then
        printf '[%s] SKIP merge_sf: completion marker exists\n' "$(timestamp)"
        verify_snapshot "${SF_MERGED}" ".terrain"
        return
    fi

    if [[ -d "${SF_MERGED}" ]] && [[ -n "$(find "${SF_MERGED}" -mindepth 1 -print -quit)" ]]; then
        printf 'Partial merge output exists at %s.\n' "${SF_MERGED}" >&2
        printf 'The current sf-merger cannot safely resume this output; refusing to overwrite it.\n' >&2
        return 1
    fi

    mkdir -p "${SF_MERGED}"
    run_timed merge_sf \
        "${SF_MERGER}" merge \
        --base "${SF_BASEMAP}" \
        --new "${SF_GATAKI}" \
        --mask "${TIROL_MASK}" \
        --output "${SF_MERGED}" \
        --verbosity info

    verify_snapshot "${SF_MERGED}" ".terrain"
    touch "${marker}"
}

build_dag()
{
    local marker="${STATE_ROOT}/build_dag.complete"
    local continuation=(--overwrite)

    if [[ -f "${marker}" ]]; then
        printf '[%s] SKIP build_dag: completion marker exists\n' "$(timestamp)"
        verify_snapshot "${DAG_MERGED}" ".bin"
        return
    fi

    mkdir -p "${DAG_MERGED}"
    if [[ -s "${DAG_MERGED}/terrain.index" ]]; then
        continuation=(--resume)
    fi

    run_timed build_dag \
        "${DAG_BUILDER}" \
        --input "${SF_MERGED}" \
        --output "${DAG_MERGED}" \
        "${continuation[@]}" \
        --verbosity info

    verify_snapshot "${DAG_MERGED}" ".bin"
    touch "${marker}"
}

write_manifest()
{
    local name="$1"
    local snapshot="$2"
    local output="${RUN_LOG_ROOT}/${name}.sha256"

    (
        cd "${snapshot}"
        find . -type f ! -name terrain.index -print0 \
            | sort -z \
            | xargs -0 -r sha256sum
    ) > "${output}"
}

record_hard_links()
{
    local merged_count=0
    local linked_to_gataki=0
    local linked_to_basemap=0
    local newly_written=0
    local merged_file relative merged_inode candidate_inode

    while IFS= read -r -d '' merged_file; do
        relative="${merged_file#"${SF_MERGED}/"}"
        merged_inode="$(stat -c '%d:%i' "${merged_file}")"
        ((merged_count += 1))

        if [[ -f "${SF_GATAKI}/${relative}" ]]; then
            candidate_inode="$(stat -c '%d:%i' "${SF_GATAKI}/${relative}")"
            if [[ "${merged_inode}" == "${candidate_inode}" ]]; then
                ((linked_to_gataki += 1))
                continue
            fi
        fi

        if [[ -f "${SF_BASEMAP}/${relative}" ]]; then
            candidate_inode="$(stat -c '%d:%i' "${SF_BASEMAP}/${relative}")"
            if [[ "${merged_inode}" == "${candidate_inode}" ]]; then
                ((linked_to_basemap += 1))
                continue
            fi
        fi

        ((newly_written += 1))
    done < <(find "${SF_MERGED}" -type f -name '*.terrain' -print0)

    printf 'merged_payloads=%s\nlinked_to_gataki=%s\nlinked_to_basemap=%s\nnewly_written=%s\n' \
        "${merged_count}" "${linked_to_gataki}" "${linked_to_basemap}" "${newly_written}" \
        | tee "${RUN_LOG_ROOT}/merge-hard-links.txt"
}

prepare_elevation_vrts()
{
    mkdir -p "${ELEVATION_ROOT}"
    gdal_translate -q -of VRT -srcwin 259507 245043 4000 4000 "${RAW_GS}" "${GS_VRT}"
    gdal_translate -q -of VRT -srcwin 259507 245043 4000 4000 "${RAW_GT}" "${GT_VRT}"
    gdal_edit.py -units m "${GS_VRT}"
    gdal_edit.py -units m "${GT_VRT}"
}

require_file "${RAW_GS}"
require_file "${RAW_GT}"
run_timed prepare_elevation_vrts prepare_elevation_vrts
require_file "${GS_VRT}"
require_file "${GT_VRT}"
require_file "${TIROL_MASK}"
require_directory "${BASEMAP_TILES}"
require_directory "${GATAKI_TILES}"
require_executable "${SF_BUILDER}"
require_executable "${SF_MERGER}"
require_executable "${DAG_BUILDER}"

{
    printf 'run_started=%s\n' "$(timestamp)"
    printf 'git_commit=%s\n' "$(git -C "${SOURCE_DIR}" rev-parse HEAD)"
    printf 'git_status=%q\n' "$(git -C "${SOURCE_DIR}" status --porcelain=v1 --branch)"
    printf 'sf_target_level=%s\n' "${SF_TARGET_LEVEL}"
    printf 'sf_threads=%s\n' "${SF_THREADS}"
    printf 'min_texture_level=%s\n' "${MIN_TEXTURE_LEVEL}"
    printf 'max_texture_level=%s\n' "${MAX_TEXTURE_LEVEL}"
    printf 'cpu_count=%s\n' "$(nproc)"
} >> "${RUN_LOG_ROOT}/run-metadata.txt"

build_sf build_sf_basemap_gs_terrain "${GS_VRT}" "${BASEMAP_TILES}" "${SF_BASEMAP}"
build_sf build_sf_gataki_gt_terrain "${GT_VRT}" "${GATAKI_TILES}" "${SF_GATAKI}"
merge_sf
record_hard_links
build_dag
run_timed manifest_sf_basemap write_manifest sf-basemap "${SF_BASEMAP}"
run_timed manifest_sf_gataki write_manifest sf-gataki "${SF_GATAKI}"
run_timed manifest_sf_merged write_manifest sf-merged "${SF_MERGED}"
run_timed manifest_dag_merged write_manifest dag-merged "${DAG_MERGED}"

printf '[%s] Golden end-to-end run complete\n' "$(timestamp)"
