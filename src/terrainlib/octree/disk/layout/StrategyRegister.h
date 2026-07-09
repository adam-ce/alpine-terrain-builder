#pragma once

#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <memory>

#include "octree/disk/layout/Strategy.h"
#include "log.h"

namespace octree::disk::layout {

class StrategyRegister {
public:
    using Factory = std::function<std::unique_ptr<Strategy>()>;

    static StrategyRegister &instance() {
        static StrategyRegister inst;
        return inst;
    }

    template <typename T>
    void register_strategy(const std::string &id) {
        this->_factories[id] = [] { return std::make_unique<T>(); };
        this->_type_to_id[std::type_index(typeid(T))] = id;
    }

    std::unique_ptr<Strategy> create(const std::string &id) const {
        auto it = this->_factories.find(id);
        if (it != this->_factories.end()) {
            return (it->second)();
        }
        return nullptr;
    }

    const std::unordered_map<std::string, Factory> &factories() const {
        return this->_factories;
    }

    template <typename T>
    std::string get_id() const {
        return this->_get_id(typeid(T));
    }

    std::string get_id(const Strategy &s) const {
        return this->_get_id(typeid(s));
    }

private:
    std::unordered_map<std::string, Factory> _factories;
    std::unordered_map<std::type_index, std::string> _type_to_id;

    // Private ctor/dtor to enforce singleton
    StrategyRegister() = default;
    ~StrategyRegister() = default;
    StrategyRegister(const StrategyRegister &) = delete;
    StrategyRegister &operator=(const StrategyRegister &) = delete;

    std::string _get_id(std::type_index type_index) const {
        auto it = this->_type_to_id.find(type_index);
        if (it != this->_type_to_id.end()) {
            return it->second;
        }
        LOG_ERROR_AND_EXIT("Encountered octree layout strategy not registered");
    }
};

} // namespace octree::disk::layout

#include "octree/disk/layout/strategy/Flat.h"
#include "octree/disk/layout/strategy/LevelAndCoordinateDirectories.h"

namespace {
template <typename T>
struct Registrar {
    Registrar(const std::string &id) {
        octree::disk::layout::StrategyRegister::instance().register_strategy<T>(id);
    }
};

const Registrar<octree::disk::layout::strategy::Flat> reg_flat("flat");
const Registrar<octree::disk::layout::strategy::LevelAndCoordinateDirectories> reg_lacd("level_and_coordinate_directories");
}
