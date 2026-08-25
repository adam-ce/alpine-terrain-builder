add_library(dagbuilderlib
    ContinuationMode.h
    Partitioning.h
    TextureSet.h
    VertexInCluster.h
    VertexInClustering.h
    build.h build.cpp
    centroids.h
    cluster.h
    clusterize.h
    compact.h
    correspondence.h
    dag_id.h
    dag_node.h
    fixed.h
    fixed_point.h
    meshopt.h
    metadata.h
    optimize.h
    parallel.h
    partition.h
    simplify.h
    slice.h
    split.h
    storage.h
    texture_reuse.h
    texture_sizing.h
    texturing.h
    utils.h
    validate.h
    vertex_lock.h

    atlas/BakeSource.h
    atlas/bake_cluster_texture.h
    atlas/pull_reproject_texture.h
    atlas/rect/HorizontalStripPlanner.h atlas/rect/HorizontalStripPlanner.cpp
    atlas/rect/PackingPlanner.h atlas/rect/PackingPlanner.cpp
    atlas/rect/Plan.h
    atlas/rect/Planner.h
    atlas/rect/atlas.h atlas/rect/atlas.cpp
    atlas/rect/create.h
    atlas/reprojection.h

    codec/Dag.h
    codec/DagFormat.h
    codec/from_extension.h

    merge/clusterings.h merge/clusterings.cpp
    merge/clusters.h
    merge/weld.h
)

target_include_directories(dagbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dagbuilderlib PUBLIC terrainlib libassert::assert metis rectpack2D meshoptimizer)

if(ALP_BUILD_DAG_BUILDER)
    add_executable(dag-builder
        cli.h cli.cpp
        main.cpp
    )
    target_link_libraries(dag-builder PRIVATE dagbuilderlib CLI11::CLI11)
endif()
