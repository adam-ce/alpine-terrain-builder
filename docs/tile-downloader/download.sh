#!/bin/bash

build_path="/home/madam/Documents/work/tuw/alpinemaps/build-terrain-builder-Desktop_Qt_6_2_3_GCC_64bit-Release/src"

while read p; do
    read zoom x y <<<${p//[^0-9]/ }
    echo -e "nice -10 \$build_path/tile-downloader --provider basemap --zoom ${zoom} --x ${x} --y ${y} --url-coordinate-order yx --verbosity 0&"
#     nice -10 $build_path/tile-downloader --provider basemap --zoom ${zoom} --x ${x} --y ${y} --url-coordinate-order yx --verbosity 0&
done <root_tile_list
