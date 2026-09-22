#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <type_traits>

#include "View.h"

namespace raster {

namespace view_details {
    struct ClampedAccess;
}

/// Read-only logical rectangle whose source coordinates clamp to source edges.
template <typename T>
class ClampedView {
public:
    using value_type = std::remove_const_t<T>;
    using element_type = const value_type;

    [[nodiscard]] unsigned width() const { return m_size.x; }
    [[nodiscard]] unsigned height() const { return m_size.y; }
    [[nodiscard]] glm::uvec2 size() const { return m_size; }

    [[nodiscard]] const value_type& pixel(glm::uvec2 position) const
    {
        assert(position.x < width() && position.y < height());
        return m_source.pixel(source_position(position));
    }

private:
    friend struct view_details::ClampedAccess;

    ClampedView(View<const value_type> source, glm::i64vec2 origin, glm::uvec2 size)
        : m_source(source)
        , m_origin(origin)
        , m_size(size)
    {
    }

    glm::uvec2 source_position(glm::uvec2 position) const
    {
        return {
            std::clamp(m_origin.x + position.x, std::int64_t(0), std::int64_t(m_source.width()) - 1),
            std::clamp(m_origin.y + position.y, std::int64_t(0), std::int64_t(m_source.height()) - 1),
        };
    }

    View<const value_type> m_source;
    glm::i64vec2 m_origin;
    glm::uvec2 m_size;
};

namespace view_details {
    struct ClampedAccess {
        template <typename T>
        static ClampedView<std::remove_const_t<T>> create(const View<T>& source, glm::i64vec2 origin, glm::uvec2 size)
        {
            // Origins beyond these limits have identical sampling behavior.
            // Bounding them also makes subsequent coordinate additions safe.
            origin.x = std::clamp(origin.x, -std::int64_t(size.x), std::int64_t(source.width()));
            origin.y = std::clamp(origin.y, -std::int64_t(size.y), std::int64_t(source.height()));
            return { source, origin, size };
        }

        template <typename T>
        static ClampedView<T> region(const ClampedView<T>& source, glm::uvec2 origin, glm::uvec2 size)
        {
            return { source.m_source, source.m_origin + glm::i64vec2(origin), size };
        }

        template <typename T>
        static Footprint footprint(const ClampedView<T>& source)
        {
            auto result = Access::footprint(source.m_source);
            if (source.width() == 0 || source.height() == 0) {
                result.size = { 0, 0 };
                return result;
            }
            const auto first = source.source_position({ 0, 0 });
            const auto last = source.source_position(source.size() - glm::uvec2(1));
            result.origin += first;
            result.size = last - first + glm::uvec2(1);
            return result;
        }
    };

} // namespace view_details

template <typename T>
[[nodiscard]] ClampedView<T> make_view(const ClampedView<T>& source)
{
    return source;
}

template <typename T>
[[nodiscard]] Expected<ClampedView<T>> make_view(const ClampedView<T>& source, glm::i64vec2 origin, glm::uvec2 size)
{
    if (!view_details::region_fits(source.size(), origin, size)) {
        return Error::fail(Error::Code::InvalidInput, "clamped view region is outside its logical bounds");
    }
    return view_details::ClampedAccess::region(source, glm::uvec2(origin), size);
}

template <typename Source>
requires requires(Source&& source) {
    make_view(std::forward<Source>(source));
    requires view_details::is_ordinary_view<decltype(make_view(std::forward<Source>(source)))>;
}
[[nodiscard]] auto make_clamped_view(Source&& source, glm::i64vec2 origin, glm::uvec2 size)
    -> Expected<ClampedView<typename decltype(make_view(std::forward<Source>(source)))::value_type>>
{
    const auto view = make_view(std::forward<Source>(source));
    if (size.x != 0 && size.y != 0 && (view.width() == 0 || view.height() == 0)) {
        return Error::fail(Error::Code::InvalidInput, "a nonempty clamped view needs a nonempty source");
    }
    return view_details::ClampedAccess::create(view, origin, size);
}

} // namespace raster
