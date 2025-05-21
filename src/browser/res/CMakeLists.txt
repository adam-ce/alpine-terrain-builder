# Default Resource Library
# Resources are accessed in the code by
# 1. Including the CMRC header file:                                    #include <cmrc/cmrc.hpp>
# 2. Placing the following macro at the global scope of the code file:  CMRC_DECLARE(<lib_namespace>);
# 3. Getting a handle for the resource library:                         auto r = cmrc::<lib_namespace>::get_filesystem();
# 4. Accessing a resource:                                              auto data = r.open(<resource_path>);
#
# For usage see: https://github.com/vector-of-bool/cmrc/tree/master#usage

cmrc_add_resource_library(
    resources-lib
    NAMESPACE res

# ADD NEW RESOURCES AFTER HERE
    shaders/octree_lines.vert
    shaders/octree_lines.frag
    # shaders/select.csh
)
