#pragma once

#include <cstddef>
#include <iterator>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "octree/Id.h"

namespace octree {

class IdRect {
public:
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = Id;
        using difference_type = std::ptrdiff_t;
        using reference = Id;
        using pointer = void;

        Iterator() = default;

        Iterator(const Id begin, const Id end, const bool is_end = false)
            : _level(begin.level()),
              _begin(begin.coords()),
              _end(end.coords()),
              _current(begin.coords()),
              _is_end(is_end) {
            DEBUG_ASSERT(begin.level() == end.level());
            DEBUG_ASSERT(_begin.x <= _end.x);
            DEBUG_ASSERT(_begin.y <= _end.y);
            DEBUG_ASSERT(_begin.z <= _end.z);
        }

        Id operator*() const {
            return Id(_level, _current);
        }

        Iterator &operator++() {
            this->advance();
            return *this;
        }

        Iterator operator++(int) {
            const Iterator copy = *this;
            this->advance();
            return copy;
        }

        friend bool operator==(const Iterator &a, const Iterator &b) {
            if (a._is_end && b._is_end) {
                return true;
            }

            return a._is_end == b._is_end &&
                   a._level == b._level &&
                   a._current == b._current &&
                   a._begin == b._begin &&
                   a._end == b._end;
        }

        friend bool operator!=(const Iterator &a, const Iterator &b) {
            return !(a == b);
        }

    private:
        void advance() {
            if (this->_is_end) {
                return;
            }

            if (this->_current.z < this->_end.z) {
                ++this->_current.z;
                return;
            }

            this->_current.z = this->_begin.z;

            if (this->_current.y < this->_end.y) {
                ++this->_current.y;
                return;
            }

            this->_current.y = this->_begin.y;

            if (this->_current.x < this->_end.x) {
                ++this->_current.x;
                return;
            }

            this->_is_end = true;
        }

        Id::Level _level;
        Id::Coords _begin;
        Id::Coords _end;
        Id::Coords _current;
        bool _is_end = true;
    };

    IdRect() = default;

    IdRect(Id begin, Id end) : _begin(begin), _end(end), _empty(false) {
        DEBUG_ASSERT(begin.level() == end.level());
        DEBUG_ASSERT(begin.coords().x <= end.coords().x);
        DEBUG_ASSERT(begin.coords().y <= end.coords().y);
        DEBUG_ASSERT(begin.coords().z <= end.coords().z);
    }

    Iterator begin() const {
        if (this->_empty) {
            return this->end();
        }

        return Iterator(this->_begin, this->_end, false);
    }

    Iterator end() const {
        return Iterator(this->_begin, this->_end, true);
    }

    bool empty() const {
        return this->_empty;
    }

    size_t size() const {
        if (this->_empty) {
            return 0;
        }

        const Id::Coords extent = this->_end.coords() - this->_begin.coords() + Id::Coords(1);
        return glm::compMul(glm::tvec3<size_t>(extent));
    }

    const Id &begin_id() const {
        return this->_begin;
    }

    const Id &end_id() const {
        return this->_end;
    }

    bool contains(const Id &id) const {
        if (this->_empty || id.level() != this->_begin.level()) {
            return false;
        }

        const Id::Coords coords = id.coords();
        return glm::all(glm::greaterThanEqual(coords, this->_begin.coords())) &&
               glm::all(glm::lessThanEqual(coords, this->_end.coords()));
    }

private:
    Id _begin;
    Id _end;
    bool _empty = true;
};

}
