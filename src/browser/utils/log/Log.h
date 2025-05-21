#pragma once

#include <spdlog/spdlog.h>
#include <string>

class Log
{
private:
    static std::shared_ptr<spdlog::logger> Logger;
    static std::shared_ptr<spdlog::logger> GLLogger;
    static std::shared_ptr<spdlog::logger> GLFWLogger;
    static std::shared_ptr<spdlog::logger> PXLogger;
    static std::shared_ptr<spdlog::logger> FTLogger;

public:
    static void Init(spdlog::level::level_enum logLevel);

    inline static std::shared_ptr<spdlog::logger> &GetLogger()
    {
        return Logger;
    }

    inline static std::shared_ptr<spdlog::logger> &GetGLLogger()
    {
        return GLLogger;
    }

    inline static std::shared_ptr<spdlog::logger> &GetGLFWLogger()
    {
        return GLFWLogger;
    }
};

#define LOG_TRACE(...) ::Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::Log::GetLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...) ::Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) ::Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Log::GetLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...) ::Log::GetLogger()->critical(__VA_ARGS__)

#define LOG_GL_TRACE(...) ::Log::GetGLLogger()->trace(__VA_ARGS__)
#define LOG_GL_DEBUG(...) ::Log::GetGLLogger()->debug(__VA_ARGS__)
#define LOG_GL_INFO(...) ::Log::GetGLLogger()->info(__VA_ARGS__)
#define LOG_GL_WARN(...) ::Log::GetGLLogger()->warn(__VA_ARGS__)
#define LOG_GL_ERROR(...) ::Log::GetGLLogger()->error(__VA_ARGS__)
#define LOG_GL_FATAL(...) ::Log::GetGLLogger()->critical(__VA_ARGS__)

#define LOG_GLFW_TRACE(...) ::Log::GetGLFWLogger()->trace(__VA_ARGS__)
#define LOG_GLFW_DEBUG(...) ::Log::GetGLFWLogger()->debug(__VA_ARGS__)
#define LOG_GLFW_INFO(...) ::Log::GetGLFWLogger()->info(__VA_ARGS__)
#define LOG_GLFW_WARN(...) ::Log::GetGLFWLogger()->warn(__VA_ARGS__)
#define LOG_GLFW_ERROR(...) ::Log::GetGLFWLogger()->error(__VA_ARGS__)
#define LOG_GLFW_FATAL(...) ::Log::GetGLFWLogger()->critical(__VA_ARGS__)

#define LOG_FATAL_AND_EXIT(...) \
    do                          \
    {                           \
        LOG_FATAL(__VA_ARGS__); \
        exit(EXIT_FAILURE);     \
    } while (false)
#define LOG_GL_FATAL_AND_EXIT(...) \
    do                             \
    {                              \
        LOG_GL_FATAL(__VA_ARGS__); \
        exit(EXIT_FAILURE);        \
    } while (false)
#define LOG_GLFW_FATAL_AND_EXIT(...) \
    do                               \
    {                                \
        LOG_GLFW_FATAL(__VA_ARGS__); \
        exit(EXIT_FAILURE);          \
    } while (false)
