#include <filesystem>

#include "mesh/io.h"
#include "cluster.h"
#include "clusterize.h"
#include "group.h"
#include "simplify.h"
#include "validate.h"
#include "utils.h"

int main(int argc, char **argv) {
    const std::filesystem::path path = "/home/user/master/meshes/innenstadt3/13/6478/4795/6857.glb";
    auto mesh = mesh::io::load_from_path(path).value();

    auto clustering = clusterize(mesh);
    validate(clustering);
    const auto clusters_mesh = clustering_to_mesh(clustering);
    mesh::io::save_to_path(clusters_mesh, "/home/user/master/meshes/clusters.glb");

    clustering = group(clustering, GroupOptions{.clusters_per_group = 4});
    validate(clustering);
    const auto grouped_mesh = clustering_to_mesh(clustering);
    mesh::io::save_to_path(grouped_mesh, "/home/user/master/meshes/grouped.glb");

    clustering = simplify(clustering, SimplifyOptions{.target_ratio = 0.5});
    validate(clustering);
    const auto simplified_mesh = clustering_to_mesh(clustering);
    mesh::io::save_to_path(simplified_mesh, "/home/user/master/meshes/simplified.glb");
}


    /*
void perform(mesh::Simple mesh) {
    const mesh::ComponentsIndex component_index = find_connected_components(mesh);
    Clustering components = clusterize_by_vertex_map(mesh, component_index);

    for (const Cluster component : components.clusters) {
        Clustering clustering = clusterize(component);

        for (size_t iter=0; iter<3; iter++) {
            clustering = group(clustering, GroupOptions{.clusters_per_group = 4});
            for (Cluster &cluster : clustering.cluster) {
                cluster.uv_unwrapping = create_uv_unwrapping(cluster, clustering.positions);
                cluster = simplify(cluster, clustering.positions);
            }
            clustering = split_clusters(clustering);
        }
    }
}

*/
