#include <queue>
#include <unordered_map>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include "octree/Id.h"
#include "octree/NodeStatus.h"
#include "octree/NodeStatusOrMissing.h"
#include "octree/Storage.h"
#include "octree/traverse.h"
#include "cli.h"

namespace {
struct DisplayNode {
    octree::Id id;
    octree::NodeStatusOrMissing status;
    bool expanded = false;
};

class TreeView {
public:
    using iterator = std::list<DisplayNode>::iterator;
    using const_iterator = std::list<DisplayNode>::const_iterator;

    explicit TreeView(const octree::IndexMap &index, const octree::Id root) : root(root), _index(index) {
        const octree::NodeStatusOrMissing status = this->_index.get(root);
        this->_view.emplace_back(root, status, false);
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
        DisplayNode &parent = *it;
        if (parent.expanded || !parent.id.has_children()) {
            return;
        }

        const auto children = parent.id.children().value();
        for (const auto &child_id : children) {
            auto status_opt = this->_index.get(child_id);
            if (!status_opt.has_value()) {
                continue;
            }
            const octree::NodeStatus status = status_opt.value();
            it++;
            it = this->_view.emplace(it, child_id, status, false);
        }

        parent.expanded = true;
    }

    void collapse_node(const size_t display_index) {
        iterator it = this->_view.begin();
        std::advance(it, display_index);
        return collapse_node(it);
    }
    void collapse_node(iterator it) {
        DisplayNode &parent = *it;
        if (!parent.expanded) {
            return;
        }

        it++;
        while (it != this->_view.end()) {
            DisplayNode &node = *it;
            if (node.id.parent() != parent.id) {
                break;
            }

            if (node.expanded) {
                this->collapse_node(it);
            }
            it = this->_view.erase(it);
        }

        parent.expanded = false;
    }

    const octree::Id root;

private:
    std::list<DisplayNode> _view;
    const octree::IndexMap &_index;
};

//---------------------------------------------------------------------
// Find the first non‑virtual node by BFS (may use octree::traverse)
//---------------------------------------------------------------------
const octree::Id find_deepest_root(const octree::IndexMap &index, const octree::Id &root = octree::Id::root()) {
    switch (octree::NodeStatusOrMissing(index.get(root))) {
    case octree::NodeStatusOrMissing::Missing:
        return octree::Id::root();
    case octree::NodeStatusOrMissing::Leaf:
        return root;
    default:
        break;
    }

    octree::Id current = root;
    while (current.has_children()) {
        const auto children = current.children().value();
        std::optional<octree::Id> next;
        for (const octree::Id &child_id : children) {
            if (!index.get(child_id).has_value()) {
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

octree::IndexedStorage open_path_indexed(const std::filesystem::path& path) {
    if (std::filesystem::is_directory(path)) {
        return octree::open_folder_indexed(path);
    } else {
        return octree::open_index(path).value();
    }
}

ftxui::Element render_line(const DisplayNode &node, size_t base_level, bool is_selected) {
    const size_t depth = node.id.level() - base_level;
    const size_t indent = depth * 2;
    auto text_line = ftxui::text(fmt::format("{:<{}} {} [{}]",
                                             "", indent,
                                             node.id,
                                             node.status));
    if (is_selected) {
        return text_line | ftxui::inverted;
    }
    return text_line;
}

int run(const cli::Args &args) {
    const octree::IndexedStorage storage = open_path_indexed(args.dataset_path);
    const octree::IndexMap &index = storage.index();
    const octree::Id root_id = args.full_view ? octree::Id::root() : find_deepest_root(index);

    TreeView tree_view(index, root_id);

    size_t selected = 0;
    auto renderer = ftxui::Renderer([&] {
        ftxui::Elements lines;
        lines.reserve(tree_view.size());
        size_t i = 0;
        for (auto it = tree_view.begin(); it != tree_view.end(); i++, it++) {
            lines.push_back(render_line(*it, tree_view.root.level(), i == selected));
        }
        return ftxui::vbox(std::move(lines)) | ftxui::frame | ftxui::border;
    });

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    auto container = ftxui::CatchEvent(renderer, [&](ftxui::Event e) {
        if (e == ftxui::Event::ArrowUp) {
            if (selected > 0) {
                selected--;
            }
            return true;
        }
        if (e == ftxui::Event::ArrowDown) {
            if (selected + 1 < tree_view.size()) {
                selected++;
            }
            return true;
        }
        if (e == ftxui::Event::ArrowRight) {
            tree_view.expand_node(selected);
            return true;
        }
        if (e == ftxui::Event::ArrowLeft) {
            tree_view.collapse_node(selected);
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
