#include <list>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include "log.h"
#include "octree/Id.h"
#include "mesh/storage.h"
#include "mesh/storage.h"
#include "cli.h"
#include "store/NodeStatusOrMissing.h"

namespace {
struct IndexNode {
    octree::Id id;
    store::NodeStatusOrMissing status;
    bool expanded = false;
};
struct MeshNode {
    octree::Id id;
    size_t vertex_count;
    size_t face_count;
};
struct ErrorNode {
    octree::Id id;
    std::string message;
};

using DisplayEntry = std::variant<IndexNode, MeshNode, ErrorNode>;

class TreeView {
public:
    using iterator = std::list<DisplayEntry>::iterator;
    using const_iterator = std::list<DisplayEntry>::const_iterator;

    explicit TreeView(const mesh::storage::IndexedStorage &storage, const octree::Id root) : root(root), _storage(storage) {
        const store::NodeStatusOrMissing status(
            DEBUG_ASSERT_VAL(this->_storage.index().get(root)).value());
        this->_view.emplace_back(IndexNode{root, status, false});
    }

    size_t size() const {
        return this->_view.size();
    }

    iterator begin() {
        return this->_view.begin();
    }
    iterator end() {
        return this->_view.end();
    }
    const_iterator begin() const {
        return this->_view.begin();
    }
    const_iterator end() const {
        return this->_view.end();
    }

    void expand_node(const size_t display_index) {
        iterator it = this->_view.begin();
        std::advance(it, display_index);
        return expand_node(it);
    }
    void expand_node(iterator it) {
        if (!std::holds_alternative<IndexNode>(*it)) {
            return;
        }
        IndexNode &parent = std::get<IndexNode>(*it);
        if (parent.expanded) {
            return;
        }
        parent.expanded = true;

        if (parent.status == store::NodeStatusOrMissing::Leaf) {
            auto result = this->_storage.load(parent.id);
            DisplayEntry entry;
            if (result) {
                const auto &mesh = result.value();
                entry = MeshNode{parent.id, mesh.vertex_count(), mesh.face_count()};
            } else {
                entry = ErrorNode{parent.id, result.error().to_string()};
            }
            it++;
            it = this->_view.emplace(it, entry);
            return;
        }

        if (!parent.id.has_children()) {
            return;
        }
        const auto children = parent.id.children().value();
        for (const auto &child_id : children) {
            auto status_result = this->_storage.index().get(child_id);
            DEBUG_ASSERT(status_result.has_value());
            if (!status_result->has_value()) {
                continue;
            }
            const store::NodeStatus status = status_result->value();
            IndexNode child_node{child_id, status, false};
            it++;
            it = this->_view.emplace(it, child_node);
        }
    }

    void collapse_node(const size_t display_index) {
        iterator it = this->_view.begin();
        std::advance(it, display_index);
        return collapse_node(it);
    }
    void collapse_node(iterator it) {
        if (!std::holds_alternative<IndexNode>(*it)) {
            return;
        }
        IndexNode &parent = std::get<IndexNode>(*it);
        if (!parent.expanded) {
            return;
        }
        parent.expanded = false;

        if (parent.status == store::NodeStatusOrMissing::Leaf) {
            it++;
            it = this->_view.erase(it);
            return;
        }

        it++;
        while (it != this->_view.end()) {
            const octree::Id id = std::visit([](const auto &node) { return node.id; }, *it);
            if (id.parent() != parent.id) {
                break;
            }

            this->collapse_node(it);
            it = this->_view.erase(it);
        }
    }

    const octree::Id root;

private:
    std::list<DisplayEntry> _view;
    const mesh::storage::IndexedStorage& _storage;
};

const octree::Id find_deepest_root(
    const store::Index<octree::StoreTraits> &index,
    const octree::Id &root = octree::Id::root()) {
    switch (store::NodeStatusOrMissing(DEBUG_ASSERT_VAL(index.get(root)).value())) {
    case store::NodeStatusOrMissing::Missing:
        return octree::Id::root();
    case store::NodeStatusOrMissing::Leaf:
        return root;
    default:
        break;
    }

    octree::Id current = root;
    while (current.has_children()) {
        const auto children = current.children().value();
        std::optional<octree::Id> next;
        for (const octree::Id &child_id : children) {
            const auto status = DEBUG_ASSERT_VAL(index.get(child_id)).value();
            if (!status.has_value()) {
                continue;
            }
            if (next.has_value()) {
                return current;
            }
            next = child_id;
        }
        if (next.has_value()) {
            current = next.value();
        } else {
            return current;
        }
    }

    return current;
}

} // namespace

mesh::storage::IndexedStorage open_path_indexed(const std::filesystem::path& path) {
    auto result = std::filesystem::is_directory(path)
        ? mesh::storage::open_folder_indexed(path)
        : mesh::storage::open_index(path);
    if (!result) {
        LOG_ERROR_AND_EXIT(
            "Failed to open dataset {}: {}",
            path,
            result.error().to_string());
    }
    return std::move(result.value());
}

std::string render_line_content(const DisplayEntry &entry) {
    return std::visit([](const auto &node) {
        using NodeType = std::remove_cvref_t<decltype(node)>;
        if constexpr (std::is_same_v<NodeType, IndexNode>) {
            return fmt::format("{} [{}]", node.id, node.status);
        } else if constexpr (std::is_same_v<NodeType, MeshNode>) {
            return fmt::format("Mesh({} vertices, {} faces)", node.vertex_count, node.face_count);
        } else if constexpr (std::is_same_v<NodeType, ErrorNode>) {
            return fmt::format("Error({})", node.message);
        }
    }, entry);
}
ftxui::Element render_line(const DisplayEntry &entry, size_t base_level, bool is_selected) {
    const octree::Id id = std::visit([](const auto &node) { return node.id; }, entry);
    size_t depth = id.level() - base_level;
    if (!std::holds_alternative<IndexNode>(entry)) {
        depth += 1;
    }
    const size_t indent = depth * 2;
    auto text_line = ftxui::text(fmt::format("{:<{}} {}",
                                             "", indent, render_line_content(entry)));
    if (is_selected) {
        return text_line | ftxui::inverted;
    }
    return text_line;
}

int run(const cli::Args &args) {
    const mesh::storage::IndexedStorage storage = open_path_indexed(args.dataset_path);
    const store::Index<octree::StoreTraits> &index = storage.index();
    const octree::Id root_id = args.full_view ? octree::Id::root() : find_deepest_root(index);

    TreeView tree_view(storage, root_id);

    size_t selected = 0;
    size_t scroll_offset = 0;
    size_t visible_lines = 0;
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto renderer = ftxui::Renderer([&] {
        ftxui::Elements lines;
        lines.reserve(tree_view.size());

        visible_lines = std::max<size_t>(1, screen.dimy() - 2);

        for (size_t i = scroll_offset;
             i < tree_view.size() && (i - scroll_offset) < visible_lines;
             ++i) {
            auto it = tree_view.begin();
            std::advance(it, i);
            lines.push_back(render_line(*it, tree_view.root.level(), i == selected));
        }

        return ftxui::vbox(std::move(lines)) | ftxui::border;
    });

    auto container = ftxui::CatchEvent(renderer, [&](ftxui::Event e) {
        auto ensure_visible = [&] {
            if (selected < scroll_offset) {
                scroll_offset = selected;
            } else if (selected >= scroll_offset + visible_lines) {
                scroll_offset = selected - visible_lines + 1;
            }
        };

        if (e == ftxui::Event::ArrowUp) {
            if (selected > 0) {
                selected--;
                ensure_visible();
            }
            return true;
        }
        if (e == ftxui::Event::ArrowDown) {
            if (selected + 1 < tree_view.size()) {
                selected++;
                ensure_visible();
            }
            return true;
        }
        if (e == ftxui::Event::ArrowRight) {
            tree_view.expand_node(selected);
            ensure_visible();
            return true;
        }
        if (e == ftxui::Event::ArrowLeft) {
            tree_view.collapse_node(selected);
            ensure_visible();
            return true;
        }
        if ((e == ftxui::Event::Character('q') || e == ftxui::Event::Character('Q') || e == ftxui::Event::Escape)) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(container);

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    Log::init(args.log_level);

    const std::string arg_str = std::accumulate(argv, argv + argc, std::string(),
                                                [](const std::string &acc, const char *arg) {
                                                    return acc + (acc.empty() ? "" : " ") + arg;
                                                });
    LOG_DEBUG("Running with: {}", arg_str);

    return run(args);
}
