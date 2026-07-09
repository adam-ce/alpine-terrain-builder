#include <filesystem>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <tl/expected.hpp>
#include <gdal_priv.h>

#include "mesh/SimpleMesh.h"
#include "mesh/cgal.h"
