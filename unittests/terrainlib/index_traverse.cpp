#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <unordered_set>
#include <algorithm>

#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/traverse.h"

using namespace octree;

TEST_CASE("octree::traverse basic") {
    IndexMap index;

    const Id root = Id::root();
    index.add(root);

    const auto children = root.children().value();
    for (const auto& child : children) {
        index.add(child);
    }

    SECTION("traversal visits root then children") {
        std::vector<Id> visited;
        traverse(
            index,
            [&](const Id &id, const NodeStatus &) {
                visited.push_back(id);
            },
            [](const Id &) { return true; }, // refine always
            root,
            TraversalOrder::DepthFirst);

        CHECK(visited.front() == root);
        CHECK(visited.size() == 9); // root + 8 children
        std::vector<Id> expected(children.begin(), children.end());
        visited.erase(visited.begin());
        CHECK(visited == expected);
    }

    SECTION("Refine function disables recursion") {
        std::vector<Id> visited;
        traverse(
            index,
            [&](const Id &id, const NodeStatus &) {
                visited.push_back(id);
            },
            [](const Id &) { return false; }, // don't refine
            root,
            TraversalOrder::DepthFirst);

        CHECK(visited.size() == 1);
        CHECK(visited.front() == root);
    }

    SECTION("Traversal skips when root is missing") {
        IndexMap empty;
        bool visited = false;

        traverse(
            empty,
            [&](const Id&, const NodeStatus&) {
                visited = true;
            },
            [](const Id&) { return true; },
            root,
            TraversalOrder::DepthFirst
        );

        CHECK_FALSE(visited);
    }
}

TEST_CASE("octree::traverse with depth") {
    IndexMap index;

    const Id root = Id::root();
    index.add(root);

    const auto children = root.children().value();
    const Id c0 = children[0];
    const Id c1 = children[1];
    const Id c2 = children[2];

    index.add(c0);
    index.add(c1);
    index.add(c2);

    // Add grandchildren
    const Id gc0 = c0.child(0).value(); // grandchild of c0
    const Id gc1 = c2.child(1).value(); // grandchild of c2

    index.add(gc0);
    index.add(gc1);

    SECTION("DFS visits deeply before siblings") {
        std::vector<Id> visited;
        traverse(
            index,
            [&](const Id& id, const NodeStatus&) {
                visited.push_back(id);
            },
            [](const Id&) { return true; },
            root,
            TraversalOrder::DepthFirst
        );

        REQUIRE(visited.size() == 6);

        // DFS order should be: root, c0, gc0, c1, c2, gc1
        std::vector<Id> expected{root, c0, gc0, c1, c2, gc1};
        CHECK(visited == expected);
    }

    SECTION("BFS visits all nodes level by level") {
        std::vector<Id> visited;
        traverse(
            index,
            [&](const Id &id, const NodeStatus &) {
                visited.push_back(id);
            },
            [](const Id &) { return true; },
            root,
            TraversalOrder::BreadthFirst);

        REQUIRE(visited.size() == 6);

        // BFS order should be: root, c0, c1, c2, gc0, gc1
        std::vector<Id> expected{root, c0, c1, c2, gc0, gc1};
        CHECK(visited == expected);
    }
}
