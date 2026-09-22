#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <radix/raster.h>

#include "Error.h"

namespace raster {

namespace view_details {
    struct Access;

    struct Footprint {
        const void* storage;
        glm::uvec2 origin;
        glm::uvec2 size;
    };
} // namespace view_details

/// Borrows a rectangular region. The backing raster must remain alive and stable.
template <typename T>
class View {
public:
    using element_type = T;
    using value_type = std::remove_const_t<T>;

    View() = default;

    template <typename U>
    requires std::is_const_v<T> && (!std::is_const_v<U>) && std::same_as<T, const U>
    View(const View<U>& source)
        : m_data(source.m_data)
        , m_origin(source.m_origin)
        , m_size(source.m_size)
        , m_stride(source.m_stride)
    {
    }

    [[nodiscard]] unsigned width() const { return m_size.x; }
    [[nodiscard]] unsigned height() const { return m_size.y; }
    [[nodiscard]] glm::uvec2 size() const { return m_size; }
    [[nodiscard]] unsigned stride() const { return m_stride; }
    [[nodiscard]] std::size_t offset() const { return std::size_t(m_origin.y) * m_stride + m_origin.x; }

    /// Descriptor constness does not change pixel constness. Coordinates must fit.
    [[nodiscard]] T& pixel(glm::uvec2 position) const
    {
        assert(position.x < width() && position.y < height());
        return m_data[(std::size_t(m_origin.y) + position.y) * m_stride + m_origin.x + position.x];
    }

private:
    friend struct view_details::Access;
    template <typename>
    friend class View;

    View(T* data, glm::uvec2 origin, glm::uvec2 size, unsigned stride)
        : m_data(data)
        , m_origin(origin)
        , m_size(size)
        , m_stride(stride)
    {
    }

    T* m_data = nullptr;
    glm::uvec2 m_origin { 0 };
    glm::uvec2 m_size { 0 };
    unsigned m_stride = 0;
};

namespace view_details {
    template <typename T>
    inline constexpr bool is_ordinary_view = false;
    template <typename T>
    inline constexpr bool is_ordinary_view<View<T>> = true;

    struct Access {
        template <typename T>
        static View<T> whole(T* data, glm::uvec2 size)
        {
            return View<T>(data, { 0, 0 }, size, size.x);
        }

        // The caller has validated that the region fits the logical view.
        template <typename T>
        static View<T> region(const View<T>& source, glm::uvec2 origin, glm::uvec2 size)
        {
            return View<T>(source.m_data, source.m_origin + origin, size, source.m_stride);
        }

        template <typename T>
        static Footprint footprint(const View<T>& source)
        {
            return { source.m_data, source.m_origin, source.m_size };
        }
    };

    inline bool region_fits(glm::uvec2 source_size, glm::i64vec2 origin, glm::uvec2 size)
    {
        return origin.x >= 0 && origin.y >= 0 && origin.x <= source_size.x && origin.y <= source_size.y && size.x <= source_size.x - origin.x
            && size.y <= source_size.y - origin.y;
    }
} // namespace view_details

template <typename T>
[[nodiscard]] View<T> make_view(radix::Raster<T>& source)
{
    return view_details::Access::whole(source.data(), source.size());
}

template <typename T>
[[nodiscard]] View<const T> make_view(const radix::Raster<T>& source)
{
    return view_details::Access::whole(source.data(), source.size());
}

template <typename T>
void make_view(radix::Raster<T>&&) = delete;
template <typename T>
void make_view(const radix::Raster<T>&&) = delete;

template <typename T>
[[nodiscard]] View<T> make_view(const View<T>& source)
{
    return source;
}

template <typename Source>
requires requires(Source&& source) {
    make_view(std::forward<Source>(source));
    requires view_details::is_ordinary_view<decltype(make_view(std::forward<Source>(source)))>;
}
[[nodiscard]] auto make_view(Source&& source, glm::i64vec2 origin, glm::uvec2 size) -> Expected<decltype(make_view(std::forward<Source>(source)))>
{
    const auto view = make_view(std::forward<Source>(source));
    if (!view_details::region_fits(view.size(), origin, size)) {
        return Error::fail(Error::Code::InvalidInput, "raster view region is outside its source");
    }
    return view_details::Access::region(view, glm::uvec2(origin), size);
}

} // namespace raster
