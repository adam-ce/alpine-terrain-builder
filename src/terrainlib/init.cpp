#include "init.h"
#include "log.h"
#include <FreeImage.h>
#include <mutex> //for call_once

#include <gdal.h>

namespace tntn {

std::once_flag g_gdal_initialized_once_flag;

void initialize_gdal_once()
{
    std::call_once(g_gdal_initialized_once_flag, []() {
        LOG_DEBUG("calling GDALAllRegister...");
        GDALAllRegister();
    });
}

std::once_flag g_freeimage_initialized_once_flag;

namespace {
    struct FreeImageDeinitialiserHandler {
        ~FreeImageDeinitialiserHandler()
        {
            FreeImage_DeInitialise();
        }
    };
}

void initialize_freeimage_once()
{
    std::call_once(g_freeimage_initialized_once_flag, []() {
        LOG_DEBUG("calling GDALAllRegister...");
        FreeImage_Initialise();
    });
    static FreeImageDeinitialiserHandler deinitialiser;
}
} // namespace tntn
