#pragma once

#include <memory>
#include <span>
#include <string>
#include "RasterTransform.h"

namespace rf_builder {

class Mask {
public:
    static Expected<Mask> open(const std::string& identifier);
    ~Mask();
    Mask(Mask&&) noexcept;
    Mask& operator=(Mask&&) noexcept;
    Expected<void> select(std::span<const glm::dvec2> centres, std::span<std::uint8_t> validity) const;
    const std::vector<RasterTransform::Bounds>& bounds() const;

private:
    struct Data;
    explicit Mask(std::unique_ptr<Data> data);
    std::unique_ptr<Data> m_data;
};

} // namespace rf_builder
