#pragma once


#include <algorithm>
#include <cmath>
#include <glm/fwd.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Image.h"
#include "geometrix.h"
#include "logging.h"
#include "tntn_assert.h"
#include <radix/tile.h>

namespace tntn {

namespace detail {

    template <typename T>
    struct isnan_helper {
        static bool isnan(const T&) noexcept { return false; }
    };

    template <>
    struct isnan_helper<double> {
        static bool isnan(const double x) noexcept { return std::isnan(x); }
    };

    template <>
    struct isnan_helper<float> {
        static bool isnan(const double x) noexcept { return std::isnan(x); }
    };

    template <class T>
    class NDVDefault {
    public:
        static void set(T& value) { }
    };

    template <>
    class NDVDefault<double> {
    public:
        static void set(double& value) { value = std::numeric_limits<double>::max(); }
    };

} // namespace detail

// Terrain Raster
template <class T>
class Raster {
private:
    // disallow copy and assign, only move semantics allowed to avoid accidential copies
    Raster(const Raster& other) = default;
    Raster& operator=(const Raster& other) = delete;

public:
    Raster() { ::tntn::detail::NDVDefault<T>::set(m_noDataValue); }
    Raster(Raster&& other) noexcept = default;
    explicit Raster(Image<T>&& other, const radix::tile::SrsBounds& srs_bounds)
        : m_width(other.width())
        , m_height(other.height())
        , m_data(std::move(other.m_data))
        , m_cell_width_in_srs(srs_bounds.width() / other.width())
        , m_cell_height_in_srs(srs_bounds.height() / other.height())
        , m_xpos(srs_bounds.min.x)
        , m_ypos(srs_bounds.min.y)
    {
        ::tntn::detail::NDVDefault<T>::set(m_noDataValue);
    }
    Raster& operator=(Raster&& other) noexcept = default;

    /**
     deep copy raster

     @return copy of current raster
    */
    Raster clone() const
    {
        return *this;
    }

    /**
     constructor

     @param w width of raster
     @param h height of raster
    */
    Raster(const unsigned int w, const unsigned int h)
        : Raster()
    {
        allocate(w, h);
    }

    /**
     copies settable parameters from parent raster
     (i.e. not width and height)

     @param raster parent to copy parametrs from
     */
    void copy_parameters(const Raster& raster)
    {
        m_xpos = raster.m_xpos;
        m_ypos = raster.m_ypos;
        m_cell_width_in_srs = raster.m_cell_width_in_srs;
        m_cell_height_in_srs = raster.m_cell_height_in_srs;
        m_noDataValue = raster.m_noDataValue;
    }

    /**
     clears all data and parameters
     (except noDataValue)
    */
    void clear()
    {
        m_width = 0;
        m_height = 0;
        m_xpos = 0;
        m_ypos = 0;
        m_cell_width_in_srs = 0;
        m_cell_height_in_srs = 0;

        m_data.resize(0);
    }

    /**
     allocate memory for data

     @param w width of raster
     @param h height of raster
    */
    void allocate(const unsigned int w, const unsigned int h)
    {
        m_width = w;
        m_height = h;
        m_data.resize(m_width * m_height);
    }

    /**
     set all raster pixels to this value

     @param value value to be set
    */
    void set_all(T value) { std::fill(get_ptr(), get_ptr() + m_width * m_height, value); }

    /**
     count all ourrences of given value

     @param value value to count
     @return number of pixels with given value
    */
    long count(T value)
    {
        long count = 0;

        T* pData = get_ptr(0);

        for (int i = 0; i < m_width * m_height; i++) {
            if (pData[i] == value)
                count++;
        }

        return count;
    }

    /**
     crop to sub raster

     row and column index coordinates with origin at LOWER left
     automatically sets xpos and ypos as expected (using lower left coordinate system)

     @param cx column of sub image lower left corner
     @param cy row of sub image lower left corner
     @param cw width of sub image
     @param ch height of sub image
     @return the sub image with updated xpos & ypos and correct cellsize parameter
    */
    Raster crop_ll(const unsigned int cx,
        const unsigned int cy,
        const unsigned int cw,
        const unsigned int ch) const
    {
        int cyn = m_height - (cy + ch);
        return crop(cx, cyn, cw, ch);
    }

    /**
     crop to sub raster

     row and column index coordinates with origin at TOP left
     automatically sets xpos and ypos as expected (using lower left coordinate system)

     @param cx column of sub image lower left corner
     @param cy row of sub image lower left corner
     @param cw width of sub image
     @param ch height of sub image
     @param dst_raster output raster (will be overwritten)
     */
    void crop(const int cx, const int cy, const int cw, const int ch, Raster& dst_raster) const
    {

        // bounds:
        int max_x = cx + cw;
        int max_y = cy + ch;

        int min_x = cx;
        int min_y = cy;

        if (max_x > m_width) {
            TNTN_LOG_DEBUG("raster.crop - max x {} larger than width {}", max_x, m_width);
        }

        if (max_y > m_height) {
            TNTN_LOG_DEBUG("raster.crop - max y {} larger than height {} ", max_y, m_height);
        }

        if (min_x < 0) {
            TNTN_LOG_DEBUG("raster.crop - min x {} smaller than zero", min_x);
        }

        if (min_y < 0) {
            TNTN_LOG_DEBUG("raster.crop - min y {} smaller than zero ", min_y);
        }

        // fix bounds
        max_x = max_x > m_width ? m_width : max_x;
        max_y = max_y > m_height ? m_height : max_y;

        min_x = min_x < 0 ? 0 : min_x;
        min_y = min_y < 0 ? 0 : min_y;

        // const int w = m_width;
        // const int h = m_height;

        const int crop_width = max_x - min_x;
        const int crop_height = max_y - min_y;

        dst_raster.allocate(crop_width, crop_height);
        dst_raster.set_all(get_no_data_value());
        dst_raster.set_no_data_value(get_no_data_value());

        dst_raster.set_cell_width(get_cell_width());
        dst_raster.set_cell_height(get_cell_height());

        dst_raster.set_pos_x(col2x(min_x));
        dst_raster.set_pos_y(row2y(max_y - 1));

        // copy pixels across
        for (int r = min_y; r < max_y; r++) {
            const T* pIn = get_ptr(r);
            T* pOut = dst_raster.get_ptr(r - min_y);

            for (int c = min_x; c < max_x; c++) {
                pOut[c - min_x] = pIn[c];
            }
        }
    }

    /**
     crop to sub raster

     row and column index coordinates with origin at TOP left
     automatically sets xpos and ypos as expected (using lower left coordinate system)

     @param cx column of sub image lower left corner
     @param cy row of sub image lower left corner
     @param cw width of sub image
     @param ch height of sub image
     @return the sub image with updated xpos & ypos and correct cellsize parameter
    */
    Raster crop(const unsigned int cx,
        const unsigned int cy,
        const unsigned int cw,
        const unsigned int ch) const
    {
        Raster dst_raster;
        crop(cx, cy, cw, ch, dst_raster);
        return dst_raster;
    }

    /**
     get bounding box of raster

     @return 2d bounding box
    */
    BBox2D get_bounding_box() const
    {
        BBox2D bb;

        bb.min.x = col_ll2x(0);
        bb.min.y = row_ll2y(0);

        bb.max.x = col_ll2x(get_width());
        bb.max.y = row_ll2y(get_height());

        return bb;
    }

    /**
     @return raster width in pixels
    */
    unsigned int get_width() const { return m_width; }

    /**
     @return raster height in pixels
     */
    unsigned int get_height() const { return m_height; }

    /**
     get tile position in geo coordinates
     @return x position of lower left corner
    */
    double get_pos_x() const { return m_xpos; }

    /**
     set tile position in geo coordinates
     @param xpos position of lower left corner
    */
    void set_pos_x(const double xpos) { m_xpos = xpos; }

    /**
     get tile position in geo coordinates
     @return y position of lower left corner
    */
    double get_pos_y() const { return m_ypos; }

    /**
     set tile position in geo coordinates
     @param ypos position of lower left corner
    */
    void set_pos_y(const double ypos) { m_ypos = ypos; }

    /**
     get value that represents missing or no data at this location
     @return no data value
    */
    T get_no_data_value() const { return m_noDataValue; }

    /**
     set value that represents missing or no data at this location
     @param ndv no data value
    */
    void set_no_data_value(T ndv) { m_noDataValue = ndv; }

    /**
     get width or height of pixel or cell
     @return cell size in geo coordinates
    */
    glm::dvec2 get_cell_size() const { return { m_cell_width_in_srs, m_cell_height_in_srs }; }
    double get_cell_width() const { return m_cell_width_in_srs; }
    double get_cell_height() const { return m_cell_height_in_srs; }

    /**
     set width or height of pixel or cell
     @param cs cell size in geo coordinates
    */
    void set_cell_size(const glm::dvec2& cs)
    {
        m_cell_width_in_srs = cs.x;
        m_cell_height_in_srs = cs.y;
    }
    void set_cell_width(const double cs) { m_cell_width_in_srs = cs; }
    void set_cell_height(const double cs) { m_cell_height_in_srs = cs; }

    /**
     get raw pointer to beginning of raster (top left is origin)
     @return raw pointer
    */
    const T* get_ptr() const { return m_data.data(); }
    T* get_ptr() { return m_data.data(); }

    /**
     get raw pointer to beginning of raster row (top left is origin)
     @param r row
     @return raw pointer
    */
    const T* get_ptr(const unsigned int r) const { return m_data.data() + r * m_width; }
    T* get_ptr(const unsigned int r) { return m_data.data() + r * m_width; }

    /**
     get raw pointer to beginning of raster row (lower left is origin)
     @param r row
     @return raw pointer
    */
    const T* get_ptr_ll(const unsigned int r) const { return m_data.data() + (m_height - 1 - r) * m_width; }
    T* get_ptr_ll(const unsigned int r) { return m_data.data() + (m_height - 1 - r) * m_width; }

    /**
     get value at raster position

     @param r row
     @param c column
     @return value
    */
    const T& value(const unsigned int r, const unsigned int c) const
    {
        TNTN_ASSERT(c < m_width);
        TNTN_ASSERT(r < m_height);
        return get_ptr(r)[c];
    }
    T& value(const unsigned int r, const unsigned int c)
    {
        TNTN_ASSERT(c < m_width);
        TNTN_ASSERT(r < m_height);
        return get_ptr(r)[c];
    }

    /**
     get value at raster position (using lower left coordinate system)

     @param r row
     @param c column
     @return value
    */
    const T& value_ll(const unsigned int r, const unsigned int c) const { return get_ptr_ll(r)[c]; }
    T& value_ll(const unsigned int r, const unsigned int c) { return get_ptr_ll(r)[c]; }

    /**
     get x component of geo/world coordinates at column c

     @param c column
     @return x geo coordinates
    */
    double col2x(const unsigned int c) const { return m_xpos + c * m_cell_width_in_srs; }

    int x2col(double x) const
    {
        if (m_cell_width_in_srs > 0) {
            const auto col = int((x - m_xpos) / m_cell_width_in_srs + 0.5);
            TNTN_ASSERT(col >= 0);
            TNTN_ASSERT(col < m_width);
            return col;
        } else {
            return 0;
        }
    }

    int y2row(double y) const
    {
        // y = m_xpos + (r + 0.5) * m_cellsize

        if (m_cell_height_in_srs > 0) {

            int r_ll = int((y - m_ypos) / m_cell_height_in_srs + 0.5);
            int r_tl = m_height - r_ll - 1;
            TNTN_ASSERT(r_tl >= 0);
            TNTN_ASSERT(r_tl < m_height);
            return r_tl;
        } else {
            return 0;
        }
    }

    /**
     get x component of geo/world coordinates at column c

     @param c column
     @return x geo coordinates
    */
    double row2y(const unsigned int r_tl) const
    {
        int r_ll = m_height - 1 - r_tl;
        return m_ypos + r_ll * m_cell_height_in_srs;
    }

    /**
     get y component of geo/world coordinates at row r
     (row in lower left coordinate system)

     @param r row from lower left
     @return y geo coordinates
    */
    double row_ll2y(const unsigned int r_ll) const { return m_ypos + r_ll * m_cell_height_in_srs; }

    /**
     get x component of geo/world coordinates at column c
     (column in lower left coordinate system)

     @param c column from lower left
     @return x geo coordinates
    */
    double col_ll2x(const unsigned int c) const { return col2x(c); }

    /**
     is raster empty
     @return false if empty, true if not
    */
    bool empty() const { return m_width == 0 && m_height == 0; }

    // note:
    // do we need this?
    // since getPtr() is public and so are the parameters via get function
    // I question the need for a general purpose function like this
    // as it can easily be implemented outside this class
    // further, it seems there should be only one correct implementation of this

    // signature for VertexReceiverFn must be (void)(double x, double y, T z)
    template <typename VertexReceiverFn>
    void to_vertices(VertexReceiverFn&& receiver_fn) const
    {
        const double cw = get_cell_width();
        const double ch = get_cell_height();
        const double xpos = get_pos_x();
        const double ypos = get_pos_y();
        const int width = get_width();
        const int height = get_height();
        const auto* p = get_ptr();

        // coordinate system has origin at lower left corner!
        for (int r = 0; r < height; r++) {
            const double y_coordinate = ypos + (height - r - 1) * ch;

            for (int c = 0; c < width; c++) {
                const double x_coordinate = xpos + c * cw;
                receiver_fn(x_coordinate, y_coordinate, p[c]);
            }

            p += width;
        }
    }

    bool is_no_data(const T& a_value) const
    {
        return detail::isnan_helper<T>::isnan(a_value) || a_value == m_noDataValue;
    }

    auto begin() const { return m_data.begin(); }
    auto begin() { return m_data.begin(); }

    auto end() const { return m_data.end(); }
    auto end() { return m_data.end(); }

    const std::vector<T>& asVector() const { return m_data; }

private:
    // raster width and height
    unsigned int m_width = 0;
    unsigned int m_height = 0;

    // tile position in world coordinates
    double m_xpos = 0;
    double m_ypos = 0;

    double m_cell_width_in_srs = 1;
    double m_cell_height_in_srs = 1;

    // value of data that indicates absence of data
    T m_noDataValue;

    std::vector<T> m_data;
};

typedef Raster<double> RasterDouble;
// typedef Raster<unsigned char>   RasterByte; //eg. for grey scale image

} // namespace tntn
