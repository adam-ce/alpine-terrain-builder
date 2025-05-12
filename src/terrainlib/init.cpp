#include <mutex>

#include <gdal.h>

#include "init.h"
#include "log.h"

std::once_flag g_gdal_initialized_once_flag;

void initialize_gdal_once() {
    std::call_once(g_gdal_initialized_once_flag, []() {
        LOG_DEBUG("calling GDALAllRegister...");
        GDALAllRegister();
    });
}
