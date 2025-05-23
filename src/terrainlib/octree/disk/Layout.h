#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <string>

#include "octree/Id.h"
#include "octree/disk/layout/Strategy.h"

namespace octree::disk {
    
class Layout {
public:
    Layout(std::filesystem::path base_path, std::unique_ptr<layout::Strategy> strategy, std::string extension_with_dot = ".terrain")
        : _base_path(std::move(base_path)), _strategy(std::move(strategy)), _extension_with_dot(std::move(extension_with_dot)) {
        assert(!_extension_with_dot.empty() && _extension_with_dot[0] == '.');
    }

    std::filesystem::path get_node_path(Id id) const {
        return this->_base_path / this->_strategy->get_relative_node_path(id, this->_extension_with_dot);
    }
    std::optional<Id> get_id_from_node_path(const std::filesystem::path& path) const {
        const std::filesystem::path relative_path = std::filesystem::relative(path, _base_path);
        return this->_strategy->get_id_from_relative_node_path(relative_path);
    }

    const std::filesystem::path& base_path() const {
        return this->_base_path;
    }
    const layout::Strategy &strategy() const {
        return *this->_strategy;
    }
    std::string_view extension_with_dot() const {
        return this->_extension_with_dot;
    }
    std::string_view extension() const {
        return this->extension_with_dot().substr(1);
    }

private:
    std::filesystem::path _base_path;
    std::unique_ptr<layout::Strategy> _strategy;
    std::string _extension_with_dot;
};

}
