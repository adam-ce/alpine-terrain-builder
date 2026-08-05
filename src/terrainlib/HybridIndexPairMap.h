#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/join.hpp>
#include <libassert/assert.hpp>

#include "build_config.h"
#include "hash_utils.h"

template <typename Index, typename Value>
class HybridIndexPairMap {
    static_assert(std::is_integral_v<Index>);
    static_assert(std::is_unsigned_v<Index>);
    static_assert(sizeof(Index) <= sizeof(size_t));

public:
    struct Entry {
        Index primary_key;
        Index secondary_key;
        const Value &value;
    };

    struct Key {
        Index primary_key;
        Index secondary_key;

        bool operator==(const Key &) const = default;
    };

    struct KeyHash {
        size_t operator()(const Key key) const noexcept {
            return hash::combine(key.primary_key, key.secondary_key);
        }
    };

    using OverflowMap = std::unordered_map<Key, Value, KeyHash>;

    void clear() {
        this->_slots.clear();
        this->_direct_values.clear();
        this->_overflow.clear();
        this->_size = 0;
    }

    void reserve_primary(const Index primary_key_count) {
        const size_t count = primary_key_count;

        this->_slots.reserve(count);

        if constexpr (!std::is_same_v<Index, Value>) {
            this->_direct_values.reserve(count);
        }
    }

    bool empty() const {
        return this->_size == 0;
    }

    size_t size() const {
        return this->_size;
    }

    void insert_or_assign(const Index primary_key, const Index secondary_key, Value value) {
        Slot &slot = this->ensure_slot(primary_key);

        if (slot.is_empty()) {
            slot.insert_direct(secondary_key, std::move(value), this->_direct_values);
            this->_size++;
            return;
        }

        if (slot.is_overflow()) {
            this->insert_overflow(primary_key, secondary_key, std::move(value));
            return;
        }

        if (slot.secondary_key == secondary_key) {
            slot.assign_direct(std::move(value), this->_direct_values);
            return;
        }

        slot.promote_to_overflow(primary_key, this->_overflow, this->_direct_values);
        this->insert_overflow(primary_key, secondary_key, std::move(value));
    }

    std::optional<Value> find(const Index primary_key, const Index secondary_key) const {
        const Value *value = this->find_ptr(primary_key, secondary_key);
        if (value == nullptr) {
            return std::nullopt;
        }
        return *value;
    }

    const Value *find_ptr(const Index primary_key, const Index secondary_key) const {
        const Slot *slot_ptr = this->find_slot(primary_key);
        if (slot_ptr == nullptr) {
            return nullptr;
        }
        const Slot &slot = *slot_ptr;

        if (slot.is_empty()) {
            return nullptr;
        }

        if (slot.is_overflow()) {
            const auto it = this->_overflow.find(Key{primary_key, secondary_key});
            if (it == this->_overflow.end()) {
                return nullptr;
            }
            return &it->second;
        }

        if (slot.secondary_key != secondary_key) {
            return nullptr;
        }
        return &slot.direct_value(this->_direct_values);
    }

    bool contains(const Index primary_key, const Index secondary_key) const {
        return this->find_ptr(primary_key, secondary_key) != nullptr;
    }

    auto entries() const {
        auto direct_entries =
            boost::irange(size_t{0}, this->_slots.size()) |
            boost::adaptors::filtered([this](const size_t primary_key) {
                return this->_slots[primary_key].is_direct();
            }) |
            boost::adaptors::transformed([this](const size_t primary_key) -> Entry {
                const Slot &slot = this->_slots[primary_key];

                return Entry{
                    .primary_key = static_cast<Index>(primary_key),
                    .secondary_key = slot.secondary_key,
                    .value = slot.direct_value(this->_direct_values),
                };
            });

        auto overflow_entries =
            this->_overflow | boost::adaptors::transformed([](const auto &entry) -> Entry {
                return Entry{
                    .primary_key = entry.first.primary_key,
                    .secondary_key = entry.first.secondary_key,
                    .value = entry.second,
                };
            });

        return boost::join(std::move(direct_entries), std::move(overflow_entries));
    }

private:
    enum class SlotState {
        Empty,
        Direct,
        Overflow,
    };

    static constexpr Index EMPTY_SLOT = std::numeric_limits<Index>::max();
    static constexpr Index OVERFLOW_SLOT = std::numeric_limits<Index>::max() - 1;
    static constexpr Index LAST_SAFE_INDEX = std::numeric_limits<Index>::max() - 2;

    static void check_safe_index(const Index primary_key) {
        DEBUG_ASSERT(primary_key <= LAST_SAFE_INDEX);
        ALP_UNUSED(primary_key);
    }

    struct Slot {
        Index secondary_key = 0;
        Index value = EMPTY_SLOT;

        SlotState state() const {
            if (this->is_empty()) {
                return SlotState::Empty;
            }

            if (this->is_overflow()) {
                return SlotState::Overflow;
            }

            return SlotState::Direct;
        }

        bool is_empty() const {
            return this->value == EMPTY_SLOT;
        }

        bool is_overflow() const {
            return this->value == OVERFLOW_SLOT;
        }

        bool is_direct() const {
            return !this->is_empty() && !this->is_overflow();
        }

        void insert_direct(
            const Index new_secondary_key,
            Value new_value,
            std::vector<Value> &direct_values) {
            this->secondary_key = new_secondary_key;

            if constexpr (std::is_same_v<Index, Value>) {
                check_safe_index(new_value);
                this->value = std::move(new_value);
            } else {
                this->value = direct_values.size();
                direct_values.push_back(std::move(new_value));
            }

            DEBUG_ASSERT(this->is_direct());
        }

        void assign_direct(Value new_value, std::vector<Value> &direct_values) {
            DEBUG_ASSERT(this->is_direct());

            if constexpr (std::is_same_v<Index, Value>) {
                check_safe_index(new_value);
                this->value = std::move(new_value);
            } else {
                direct_values[this->value] = std::move(new_value);
            }

            DEBUG_ASSERT(this->is_direct());
        }

        void promote_to_overflow(
            const Index primary_key,
            OverflowMap &overflow,
            const std::vector<Value> &direct_values) {
            DEBUG_ASSERT(this->is_direct());

            const bool inserted = overflow.emplace(
                Key{primary_key, this->secondary_key},
                this->direct_value(direct_values)).second;
            DEBUG_ASSERT(inserted);
            ALP_UNUSED(inserted);
            this->value = OVERFLOW_SLOT;

            DEBUG_ASSERT(this->is_overflow());
        }

        const Value &direct_value(const std::vector<Value> &direct_values) const {
            DEBUG_ASSERT(this->is_direct());

            if constexpr (std::is_same_v<Index, Value>) {
                return this->value;
            } else {
                return direct_values[this->value];
            }
        }
    };

    Slot &ensure_slot(const Index primary_key) {
        check_safe_index(primary_key);
        if (primary_key >= this->_slots.size()) {
            this->_slots.resize(primary_key + 1);
        }
        return this->_slots[primary_key];
    }

    const Slot *find_slot(const Index primary_key) const {
        if (primary_key >= this->_slots.size()) {
            return nullptr;
        }
        return &this->_slots[primary_key];
    }

    void insert_overflow(const Index primary_key, const Index secondary_key, Value value) {
        const Key key{primary_key, secondary_key};
        const auto [_, inserted] = this->_overflow.insert_or_assign(key, std::move(value));
        if (inserted) {
            this->_size++;
        }
    }

    std::vector<Slot> _slots;
    std::vector<Value> _direct_values;
    OverflowMap _overflow;

    size_t _size = 0;
};
