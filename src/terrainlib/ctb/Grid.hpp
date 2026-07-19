#ifndef CTBGRID_HPP
#define CTBGRID_HPP

/*******************************************************************************
 * Copyright 2014 GeoData <geodata@soton.ac.uk>
 * Copyright 2022 Adam Celarek <family name at cg tuwien ac at>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *******************************************************************************/

/**
 * @file Grid.hpp
 * @brief This defines and declares the `Grid` class
 */

#include <cmath>
#include <optional>

#include "ogr_spatialref.h"

#include "types.hpp"
#include <radix/tile.h>
#include <radix/geometry.h>

namespace ctb {
class Grid;
}

/**
 * @brief A generic grid for cutting tile sets
 *
 * This class models a grid for use in cutting up an area into zoom levels and
 * tiles.  It provides functionality such as relating a coordinate in a native
 * coordinate reference system (CRS) to a tile (see `Grid::crsToTile`) and
 * getting the CRS bounds of a tile (see `Grid::tileBounds`).
 *
 * The `Grid` class should be able to model most grid systems. The
 * `GlobalMercator` and `GlobalGeodetic` subclasses implement the specific Tile
 * Mapping Service grid profiles.
 *
 * The code here generalises the logic in the `gdal2tiles.py` script available
 * with the GDAL library.
 *
 * Tile identifiers use Google/Mapbox/XYZ coordinates: the origin is north-west and y grows south.
 * CRS and pixel coordinates inside Grid retain their conventional positive-north orientation.
 */
class ctb::Grid {
public:
    enum class Srs {
        SphericalMercator = 3857,
        WGS84 = 4326
    };

    /// Initialise a grid tile
    Grid(i_tile gridSize,
         const radix::tile::SrsBounds extent,
         const OGRSpatialReference &srs,
         int epsgCode,
         std::vector<radix::tile::Id> rootTiles,
         double zoomFactor)
        : mGridSize(gridSize), mExtent(extent), mSRS(srs), mEpsgCode(epsgCode), mInitialResolution((extent.size().x / rootTiles.size()) / gridSize), mXOriginShift(extent.size().x / 2), mYOriginShift(extent.size().y / 2), mZoomFactor(zoomFactor), mRootTiles(rootTiles) {
        mSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    /// Override the equality operator
    bool operator==(const Grid& other) const
    {
        return mGridSize == other.mGridSize
            && mExtent == other.mExtent
            && mSRS.IsSame(&(other.mSRS))
            && mInitialResolution == other.mInitialResolution
            && mXOriginShift == other.mXOriginShift
            && mYOriginShift == other.mYOriginShift
            && mZoomFactor == other.mZoomFactor;
    }

    /// Get the resolution for a particular zoom level
    [[nodiscard]] inline double resolution(i_zoom zoom) const
    {
        return mInitialResolution / pow(mZoomFactor, zoom);
    }

    /**
     * @brief Get the zoom level for a particular resolution
     *
     * If the resolution does not exactly match a zoom level then the zoom level
     * is 'rounded up' to the next level.
     */
    [[nodiscard]] inline i_zoom zoomForResolution(double resolution) const
    {
        // if mZoomFactor == 2 the following is the same as using:
        // log2(mInitialResolution) - log2(resolution)
        return i_zoom(ceil((std::log(mInitialResolution) / std::log(mZoomFactor)) - (std::log(resolution) / std::log(mZoomFactor))));
    }

    /// Get the tile covering a pixel location
    [[nodiscard]] inline TilePoint pixelsToTile(const PixelPoint& pixel) const
    {
        const auto tx = i_tile(pixel.x / mGridSize);
        const auto ty = i_tile(pixel.y / mGridSize);

        return { tx, ty };
    }

    /// Convert pixel coordinates at a given zoom level to CRS coordinates
    [[nodiscard]] inline CRSPoint pixelsToCrs(const PixelPoint& pixel, i_zoom zoom) const
    {
        double res = resolution(zoom);

        return {
            (pixel.x * res) - mXOriginShift,
            (pixel.y * res) - mYOriginShift
        };
    }

    /// Get the pixel location represented by a CRS point and zoom level
    [[nodiscard]] inline PixelPoint crsToPixels(const CRSPoint& coord, i_zoom zoom) const
    {
        const auto res = resolution(zoom);
        const auto px = (mXOriginShift + coord.x) / res;
        const auto py = (mYOriginShift + coord.y) / res;

        return { px, py };
    }

    /// Get the tile coordinate in which a location falls at a specific zoom level
    [[nodiscard]] inline radix::tile::Id crsToTile(const CRSPoint &coord, i_zoom zoom) const {
        const PixelPoint pixel = crsToPixels(coord, zoom);
        const TilePoint tile = pixelsToTile(pixel);
        const auto tile_count = i_tile(1u << zoom);

        return {zoom, {tile.x, tile_count - tile.y - 1}};
    }

    /// Get the CRS bounds of a particular tile
    /// border_se should be true if a border should be included on the south eastern corner
    /// e.g., for the cesium raster terrain format (https://github.com/CesiumGS/cesium/wiki/heightmap-1%2E0)
    [[nodiscard]] inline radix::tile::SrsBounds srsBounds(const radix::tile::Id &tile_id, bool border_se) const {
        const auto tile_count = i_tile(1u << tile_id.zoom_level);
        const auto grid_y = tile_count - tile_id.coords.y - 1;
        // get the pixels coordinates representing the tile bounds
        const PixelPoint pxMinLeft(tile_id.coords.x * mGridSize, grid_y * mGridSize);
        const PixelPoint pxMaxRight((tile_id.coords.x + 1) * mGridSize + border_se, (grid_y + 1) * mGridSize + border_se);

        // convert pixels to native coordinates
        const CRSPoint minLeft = pixelsToCrs(pxMinLeft, tile_id.zoom_level);
        const CRSPoint maxRight = pixelsToCrs(pxMaxRight, tile_id.zoom_level);

        return { minLeft, maxRight };
    }

    [[nodiscard]] const std::vector<radix::tile::Id> &rootTiles() const {
        return this->mRootTiles;
    }

    // Searches for the smallest tile that encompasses the given bounding box.
    [[nodiscard]] std::optional<radix::tile::Id> findSmallestEncompassingTile(const radix::tile::SrsBounds &bounds) const {
        // We dont want to recurse indefinetely if the bounds are empty.
        if (bounds.width() == 0 || bounds.height() == 0) {
            throw std::invalid_argument("bounds cannot be empty");
        }

        const std::array<glm::dvec2, 4> points = {
            bounds.min,
            bounds.max,
            glm::dvec2(bounds.min.x, bounds.max.y),
            glm::dvec2(bounds.max.x, bounds.min.y)};


        // We start at the root tiles and repeatedly check every subtile until we find one that contains all
        // of the bounding box.
        for (const radix::tile::Id &root_tile : this->rootTiles()) {
            // Get the bounds of the current root tile.
            const radix::tile::SrsBounds root_tile_bounds = this->srsBounds(root_tile, false);

            // Check if all of the target bounding box is inside this tile.
            bool all_points_inside = true;
            for (const auto &point : points) {
                if (!root_tile_bounds.contains_inclusive(point)) {
                    all_points_inside = false;
                    break;
                }
            }

            // If not all points are inside, continue to the next root tile.
            if (!all_points_inside) {
                continue;
            }

            // Now find the smallest tile under this root.
            radix::tile::Id current_smallest_encompassing_tile = root_tile;
            while (true) {
                const std::array<radix::tile::Id, 4> children = current_smallest_encompassing_tile.children();

                bool all_points_inside_any_child = false;
                for (const auto &child : children) {
                    // Get the bounds of the current child
                    const radix::tile::SrsBounds tile_bounds = this->srsBounds(child, false);

                    // Check if all of the target bounding box is inside this tile.
                    bool all_points_inside = true;
                    for (const auto &point : points) {
                        if (!tile_bounds.contains_inclusive(point)) {
                            all_points_inside = false;
                            break;
                        }
                    }

                    // If all points are inside, we can recurse.
                    if (!all_points_inside) {
                        continue;
                    }

                    all_points_inside_any_child = true;
                    current_smallest_encompassing_tile = child;
                    break;
                }

                // Check if all of the points are contained in any children, so we can stop the recursion.
                if (!all_points_inside_any_child) {
                    break;
                }
            }

            return current_smallest_encompassing_tile;
        }

        return std::nullopt;
    }

    // Searches for the smallest tile that encompasses the given bounding box.
    [[nodiscard]] std::optional<radix::tile::Id> findLargestContainedTile(const radix::tile::SrsBounds &bounds) const {
        const std::optional<radix::tile::Id> root = this->findSmallestEncompassingTile(bounds);
        if (!root.has_value()) {
            return std::nullopt;
        }

        std::vector<radix::tile::Id> tiles_to_inspect;
        const std::array<radix::tile::Id, 4> root_children = root->children();
        tiles_to_inspect.insert(tiles_to_inspect.end(), root_children.begin(), root_children.end());

        std::optional<radix::tile::Id> current_candidate;
        while (!tiles_to_inspect.empty()) {
            const radix::tile::Id tile = tiles_to_inspect.back();
            tiles_to_inspect.pop_back();

            if (current_candidate.has_value() && current_candidate->zoom_level <= tile.zoom_level) {
                // skip all tiles that dont have a chance of replacing the current candidate
                continue;
            }

            const radix::tile::SrsBounds tile_bounds = this->srsBounds(tile, false);
            const std::array<glm::dvec2, 4> points = {
                tile_bounds.min,
                tile_bounds.max,
                glm::dvec2(tile_bounds.min.x, tile_bounds.max.y),
                glm::dvec2(tile_bounds.max.x, tile_bounds.min.y)};

            const bool target_bounds_overlapping_tile = radix::geometry::intersect(tile_bounds, bounds);
            bool target_bounds_fully_inside_tile = false;
            if (target_bounds_overlapping_tile) {
                target_bounds_fully_inside_tile = true;

                for (const glm::dvec2 &point : points) {
                    if (!bounds.contains_inclusive(point)) {
                        target_bounds_fully_inside_tile = false;
                        break;
                    }
                }
            }

            if (target_bounds_fully_inside_tile) {
                assert(!current_candidate.has_value() || current_candidate->zoom_level > tile.zoom_level);
                current_candidate = tile;
                continue;
            }

            if (target_bounds_overlapping_tile) {
                const std::array<radix::tile::Id, 4> children = tile.children();
                tiles_to_inspect.insert(tiles_to_inspect.end(), children.begin(), children.end());
            }
        }

        return current_candidate;
    }

    /// Get the tile size associated with this grid
    [[nodiscard]] inline i_tile tileSize() const
    {
        return mGridSize;
    }

    /// Get the tile size associated with this grid
    [[nodiscard]] inline const OGRSpatialReference& getSRS() const
    {
        return mSRS;
    }

    /// Get the extent covered by the grid in CRS coordinates
    [[nodiscard]] inline const radix::tile::SrsBounds &getExtent() const {
        return mExtent;
    }

    [[nodiscard]] inline int getEpsgCode() const
    {
        return mEpsgCode;
    }

private:
    /// The tile size associated with this grid
    i_tile mGridSize;

    /// The area covered by the grid
    radix::tile::SrsBounds mExtent;

    /// The spatial reference system covered by the grid
    OGRSpatialReference mSRS;
    int mEpsgCode = -1;

    double mInitialResolution; ///< The initial resolution of this particular profile
    double mXOriginShift; ///< The shift in CRS coordinates to get to the origin from minx
    double mYOriginShift; ///< The shift in CRS coordinates to get to the origin from miny

    /// By what factor will the scale increase at each zoom level?
    double mZoomFactor;

    /// A list of all the root tiles of the grid.
    std::vector<radix::tile::Id> mRootTiles;
};

#endif /* CTBGRID_HPP */
