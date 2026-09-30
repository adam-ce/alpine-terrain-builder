#!/bin/bash

# *****************************************************************************
# AlpineMaps.org
# Copyright (C) 2022 Adam Celarek-Litofcenko
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# *****************************************************************************

austria_dem="/home/madam/rajaton/raw/Oe_2020/OeRect_01m_gt_31287.img"
interpolation="cubic"

gdalwarp  -te_srs EPSG:4326 -te 15.661254 47.976894 17.03693 48.36381 -t_srs EPSG:4326 -ts 4931 2660 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem vienna_20m_epsg4326.tif
gdalwarp  -te_srs EPSG:4326 -te 15.661254 47.976894 17.03693 48.36381 -t_srs EPSG:3857 -ts 4931 2660 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem vienna_20m_epsg3857.tif
gdalwarp  -te_srs EPSG:4326 -te 15.661254 47.976894 17.03693 48.36381 -ts 5030 2362 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem vienna_20m_mgi.tif

gdalwarp  -te_srs EPSG:4326 -te 12.1977235 46.9704596 12.990698 47.231140 -t_srs EPSG:4326 -ts 5189 2428 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem tauern_10m_epsg4326.tif
gdalwarp  -te_srs EPSG:4326 -te 12.1977235 46.9704596 12.990698 47.231140 -t_srs EPSG:3857 -ts 5189 2428 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem tauern_10m_epsg3857.tif
gdalwarp  -te_srs EPSG:4326 -te 12.1977235 46.9704596 12.990698 47.231140 -ts 5116 2402 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem tauern_10m_mgi.tif

gdalwarp  -te_srs EPSG:4326 -te 9.075279423 46.077736134 17.589679640 49.298348983 -t_srs EPSG:4326 -ts 6390 3579 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem at_100m_epsg4326.tif
gdalwarp  -te_srs EPSG:4326 -te 9.075279423 46.077736134 17.589679640 49.298348983 -t_srs EPSG:3857 -ts 6390 3579 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem at_100m_epsg3857.tif
# yes, mgi bounds are different, because they are without warping. it does not matter with the smaller datasets for the unit tests, but here it does.
gdalwarp  -te_srs EPSG:4326 -te 9.32327011002036 46.07773613448158 17.58967964015434 49.22215812387113 -ts 6200 3500 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem at_100m_mgi.tif

gdalwarp  -te_srs EPSG:4326 -te 9.075279423 46.077736134 17.589679640 49.298348983 -t_srs EPSG:4326 -ts 4289 3579 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem at_x149m_y100m_epsg4326.tif

gdalwarp  -te_srs EPSG:4326 -te 10.094526105 46.836309748 10.142863721 46.851832725 -t_srs EPSG:4326 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem pizbuin_1m_epsg4326.tif
gdalwarp  -te_srs EPSG:4326 -te 10.094526105 46.836309748 10.142863721 46.851832725 -t_srs EPSG:3857 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem pizbuin_1m_epsg3857.tif
gdalwarp  -te_srs EPSG:4326 -te 10.094526105 46.836309748 10.142863721 46.851832725 -r $interpolation -of GTiff -co COMPRESS=DEFLATE -co PREDICTOR=2 -co ZLEVEL=9 -overwrite $austria_dem pizbuin_1m_mgi.tif
