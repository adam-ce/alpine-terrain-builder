#include "../catch2_helpers.h"

#include "octree/IndexMap.h"

using namespace octree;

TEST_CASE("IndexMap basic operations") {
    IndexMap index;

    SECTION("Add root node") {
        Id root = Id::root();
        index.add(root);
        CHECK(index.is_present(root));
        CHECK(index.is(NodeStatus::Leaf, root));
        CHECK(index.get(root) == std::optional<NodeStatus>(NodeStatus::Leaf));
    }

    SECTION("Add child node creates virtual parent") {
        Id root = Id::root();
        Id child = root.child(3).value();
        index.add(child);
        CHECK(index.is_present(root));
        CHECK(index.is(NodeStatus::Virtual, root));
        CHECK(index.is_present(child));
        CHECK(index.is(NodeStatus::Leaf, child));
    }

    SECTION("Add child note promotes parent to inner") {
        Id root = Id::root();
        Id child1 = root.child(0).value();
        index.add(child1);
        index.add(root);
        CHECK(index.is(NodeStatus::Leaf, child1));
        CHECK(index.is(NodeStatus::Inner, root));
    }

    SECTION("Removing a leaf node cleans up correctly") {
        Id root = Id::root();
        Id child = root.child(2).value();
        index.add(child);
        CHECK(index.is_present(child));
        index.remove(child);
        CHECK(index.is_absent(child));
        CHECK(index.is_absent(root));
    }

    SECTION("Removing one of two children of an inner keeps it inner") {
        Id root = Id::root();
        Id child1 = root.child(0).value();
        Id child2 = root.child(1).value();
        index.add(root);
        index.add(child1);
        index.add(child2);
        index.remove(child1);

        CHECK(index.is_absent(child1));
        CHECK(index.is(NodeStatus::Leaf, child2));
        CHECK(index.is(NodeStatus::Inner, root));
    }

    SECTION("Removing last child of inner converts it to leaf") {
        Id root = Id::root();
        Id child1 = root.child(0).value();
        index.add(child1);
        index.add(root);

        index.remove(child1);

        CHECK(index.is_absent(child1));
        CHECK(index.is(NodeStatus::Leaf, root));
    }

    SECTION("Clear works") {
        Id a = Id::root().child(1).value();
        Id b = Id::root().child(2).value();
        index.add(a);
        index.add(b);
        CHECK(index.size() == 3); // root + a + b
        index.clear();
        CHECK(index.empty());
    }

    SECTION("Serialization round-trip") {
        IndexMap original;
        original.add(Id::root().child(3).value());

        const std::errc success {};

        std::vector<std::byte> buffer;
        zpp::bits::out out(buffer);
        CHECK(out(original).code == success);

        IndexMap loaded;
        zpp::bits::in in(buffer);
        CHECK(in(loaded).code == success);

        CHECK(original.size() == loaded.size());
        CHECK(loaded.is_present(Id::root().child(3).value()));
    }
}
