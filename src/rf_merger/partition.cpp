#include "partition.h"

namespace rf_merger::partition {
namespace {
    using Status = std::optional<store::NodeStatus>;

    Expected<Status> status(const Index& index, const Key& key)
    {
        auto result = index.get(key);
        if (!result) {
            return Error::propagate(std::move(result), "read RF input index");
        }
        if (*result == store::NodeStatus::Inner) {
            return Error::fail(Error::Code::InvalidInput, "RF merger input has a physical tile with physical descendants at " + to_string(key));
        }
        return *result;
    }
} // namespace

Expected<std::optional<Key>> supplier(const Index& index, const Key& key)
{
    for (std::optional<Key> current = key; current; current = raster_store::StoreTraits::parent(*current)) {
        auto found = status(index, *current);
        if (!found) {
            return Error::propagate(std::move(found));
        }
        if (*found == store::NodeStatus::Leaf) {
            return current;
        }
    }
    return std::nullopt;
}

Expected<bool> is_leaf(const Index& left, const Index& right, const Key& key)
{
    for (const auto* index : { &left, &right }) {
        auto found = status(*index, key);
        if (!found) {
            return Error::propagate(std::move(found));
        }
        if (*found == store::NodeStatus::Virtual) {
            return false;
        }
    }
    for (const auto* index : { &left, &right }) {
        auto covering = supplier(*index, key);
        if (!covering) {
            return Error::propagate(std::move(covering));
        }
        if (*covering) {
            return true;
        }
    }
    return false;
}

Cursor::Cursor(const Index& left, const Index& right)
    : m_left(&left)
    , m_right(&right)
{
    if (!left.empty() || !right.empty()) {
        m_stack.push_back({ raster_store::StoreTraits::root(), std::nullopt, std::nullopt });
    }
}

Expected<std::optional<Leaf>> Cursor::next()
{
    while (!m_stack.empty()) {
        auto frame = m_stack.back();
        m_stack.pop_back();
        auto left = status(*m_left, frame.key);
        if (!left) {
            return Error::propagate(std::move(left));
        }
        auto right = status(*m_right, frame.key);
        if (!right) {
            return Error::propagate(std::move(right));
        }
        if (*left == store::NodeStatus::Leaf) {
            frame.left = frame.key;
        }
        if (*right == store::NodeStatus::Leaf) {
            frame.right = frame.key;
        }
        const bool descend = *left == store::NodeStatus::Virtual || *right == store::NodeStatus::Virtual;
        if (!descend) {
            if (frame.left || frame.right) {
                return Leaf { frame.key, frame.left, frame.right };
            }
            continue;
        }
        const auto children = raster_store::StoreTraits::children(frame.key);
        if (!children) {
            return Error::fail(Error::Code::CorruptData, "RF input index has a virtual node without children at " + to_string(frame.key));
        }
        // Reverse order pops the first child next.
        for (auto child = children->rbegin(); child != children->rend(); ++child) {
            m_stack.push_back({ *child, frame.left, frame.right });
        }
    }
    return std::nullopt;
}

} // namespace rf_merger::partition
