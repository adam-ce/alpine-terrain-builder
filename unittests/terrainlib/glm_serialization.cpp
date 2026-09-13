#include "io/glm_serialization.h"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

template <typename Value>
auto encode(const Value& value)
{
    std::vector<std::byte> bytes;
    zpp::bits::out output(bytes);
    REQUIRE(!zpp::bits::failure(output(value)));
    return bytes;
}

template <typename Value, typename Scalar, std::size_t Size>
void check_encoding(const Value& value, const std::array<Scalar, Size>& components)
{
    const auto bytes = encode(value);
    CHECK(bytes.size() == Size * sizeof(Scalar));
    CHECK(bytes == encode(components));

    Value decoded {};
    zpp::bits::in input(bytes);
    REQUIRE(!zpp::bits::failure(input(decoded)));
    CHECK(decoded == value);
    CHECK(input.position() == bytes.size());

    // Non-const values must also work with output archives.
    std::vector<std::byte> mutable_bytes;
    zpp::bits::out output(mutable_bytes);
    REQUIRE(!zpp::bits::failure(output(decoded)));
    CHECK(mutable_bytes == bytes);
}

} // namespace

TEMPLATE_TEST_CASE("GLM vectors serialize only their components",
    "[glm_serialization]",
    glm::vec1,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    glm::dvec3,
    glm::ivec3,
    glm::uvec4,
    glm::bvec2,
    (glm::vec<3, float, glm::packed_lowp>))
{
    []<glm::length_t Length, typename Scalar, glm::qualifier Qualifier>(glm::vec<Length, Scalar, Qualifier> value) {
        std::array<Scalar, Length> components {};
        for (glm::length_t index = 0; index < Length; ++index) {
            value[index] = components[index] = static_cast<Scalar>(index % 2 == 0 ? index + 1 : 0);
        }
        check_encoding(value, components);
    }(TestType {});
}

TEMPLATE_TEST_CASE("GLM matrices serialize scalars in column-major order",
    "[glm_serialization]",
    glm::mat2,
    glm::mat2x3,
    glm::mat2x4,
    glm::mat3x2,
    glm::mat3,
    glm::mat3x4,
    glm::mat4x2,
    glm::mat4x3,
    glm::mat4,
    glm::dmat3x2,
    (glm::mat<2, 3, float, glm::packed_mediump>))
{
    []<glm::length_t Columns, glm::length_t Rows, typename Scalar, glm::qualifier Qualifier>(glm::mat<Columns, Rows, Scalar, Qualifier> value) {
        std::array<Scalar, Columns * Rows> components {};
        for (glm::length_t column = 0; column < Columns; ++column) {
            for (glm::length_t row = 0; row < Rows; ++row) {
                const auto index = column * Rows + row;
                value[column][row] = components[index] = static_cast<Scalar>(index - 3);
            }
        }
        check_encoding(value, components);
    }(TestType {});
}

TEST_CASE("GLM serialization works inside nested aggregates and containers", "[glm_serialization]")
{
    struct Vertex {
        glm::dvec3 position;
        glm::vec2 texture_coordinates;
        bool operator==(const Vertex&) const = default;
    };
    struct Payload {
        std::uint32_t id;
        std::vector<Vertex> vertices;
        std::array<glm::mat2x3, 2> transforms;
        bool operator==(const Payload&) const = default;
    };
    const Payload value {
        42,
        { { { 1.0, -2.0, 3.5 }, { 0.25f, 0.75f } }, { { -4.0, 5.0, 6.0 }, { 1.0f, 0.0f } } },
        { glm::mat2x3(1.0f), glm::mat2x3(2.0f) },
    };
    const auto bytes = encode(value);
    Payload decoded {};
    zpp::bits::in input(bytes);
    REQUIRE(!zpp::bits::failure(input(decoded)));
    CHECK(decoded == value);
    CHECK(input.position() == bytes.size());
}

TEMPLATE_TEST_CASE("GLM serialization propagates truncated buffer errors", "[glm_serialization]", glm::vec3, glm::mat2x3)
{
    const TestType value(1.0f);
    const auto bytes = encode(value);
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        CAPTURE(size);
        TestType decoded {};
        zpp::bits::in input(std::span<const std::byte>(bytes.data(), size));
        CHECK(zpp::bits::failure(input(decoded)));

        std::vector<std::byte> buffer(size);
        zpp::bits::out output { std::span<std::byte>(buffer) };
        CHECK(zpp::bits::failure(output(value)));
    }
}

#if GLM_CONFIG_ALIGNED_GENTYPES == GLM_ENABLE
TEST_CASE("Aligned GLM vectors and matrices omit padding", "[glm_serialization]")
{
    using Vector = glm::vec<3, float, glm::aligned_highp>;
    using Matrix = glm::mat<2, 3, float, glm::aligned_highp>;
    static_assert(sizeof(Vector) > 3 * sizeof(float));
    static_assert(sizeof(Matrix) > 6 * sizeof(float));

    check_encoding(Vector(1.0f, 2.0f, 3.0f), std::array { 1.0f, 2.0f, 3.0f });
    check_encoding(Matrix(Vector(1.0f, 2.0f, 3.0f), Vector(4.0f, 5.0f, 6.0f)), std::array { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f });

    const std::vector<Vector> aligned { Vector(1.0f, 2.0f, 3.0f), Vector(4.0f, 5.0f, 6.0f) };
    const std::vector<glm::vec3> packed { { 1.0f, 2.0f, 3.0f }, { 4.0f, 5.0f, 6.0f } };
    const auto bytes = encode(aligned);
    CHECK(bytes == encode(packed));
    std::vector<Vector> decoded;
    zpp::bits::in input(bytes);
    REQUIRE(!zpp::bits::failure(input(decoded)));
    CHECK(decoded == aligned);
}
#endif
