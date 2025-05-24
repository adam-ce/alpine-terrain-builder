#include "Application.h"
#include <sstream>

#include <log.h>

#include <glm/glm.hpp>

// CMRC Resource Compiler
#include <cmrc/cmrc.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "shader/Shader.h"
#include "shader/ShaderProgram.h"
#include "Buffer.h"
#include "Camera.h"
#include "geometry/UnitCube.h"
#include "octree/Space.h"
#include "rendering/OctreeRenderManager.h"

CMRC_DECLARE(res);

Application::Application(std::string title, int width, int height)
    : m_title(title), m_width(width), m_height(height) {

    m_nav_mode = false;

    m_refining_factor = 0.5f;

    WindowConfig c = {
        .width = 1280,
        .height = 720,
        .title = "Alpenite Browser",
        .resizeable = true,
        .msaa_samples = 4,
        .opengl_version = {4, 6},
        .opengl_core_profile = true
    };

    m_window = std::make_unique<Window>(c);

    init_glad();
    init_gl();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window->handle(), true);          // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();

    if (c.msaa_samples > 0) {
        glEnable(GL_MULTISAMPLE);
    }

    m_movement_speed = 100000.0f;
    m_roll_speed = 0.02f;
    m_mouse_sensitivity = 50.0f;
}

void Application::run() {
    auto RES = cmrc::res::get_filesystem();

    double last_frame_time = glfwGetTime();

    LOG_INFO("Setting up key event callbacks");
    m_window->register_key_event(GLFW_PRESS, GLFW_KEY_TAB, [this]() {
        toggle_nav_mode();
    });

    m_window->register_key_event(GLFW_PRESS, GLFW_KEY_ESCAPE, [this]() {
        if (m_nav_mode) {
            toggle_nav_mode();
        } else {
            m_window->set_should_close(true);
        }
    });

    m_window->register_scroll_event([this](glm::dvec2 scroll) {
        float old_speed = m_movement_speed;

        double factor = 1.0f;

        if (scroll.y > 0) {
            factor = 1.2f;
        } else if (scroll.y < 0) {
            factor = 0.8f;
        }

        m_movement_speed = glm::max(1.0f, m_movement_speed * (float)factor);
        LOG_DEBUG("Movement Speed: {} >> {}", old_speed, m_movement_speed);
    });

    LOG_INFO("Setting up shaders");
    auto vsc_octree_lines = RES.open("shaders/octree_lines.vert");
    Shader vs_octree_lines(GL_VERTEX_SHADER);
    vs_octree_lines.compile(std::string_view(vsc_octree_lines.begin(), vsc_octree_lines.end()));

    auto fsc_octree_lines = RES.open("shaders/octree_lines.frag");
    Shader fs_octree_lines(GL_FRAGMENT_SHADER);
    fs_octree_lines.compile(std::string_view(fsc_octree_lines.begin(), fsc_octree_lines.end()));

    ShaderProgram sp_octree_lines;

    sp_octree_lines.attach(vs_octree_lines);
    sp_octree_lines.attach(fs_octree_lines);
    sp_octree_lines.link();
    sp_octree_lines.use();

    Uniform<glm::mat4> U_projection = sp_octree_lines.get_uniform<glm::mat4>("projection");
    Uniform<glm::mat4> U_view = sp_octree_lines.get_uniform<glm::mat4>("view");

    LOG_INFO("Setting up camera");
    CameraConfig camera_config = {
        .fov_deg = 90.0f,
        .aspect_ratio = m_window->getAspectRatio(),
        .near_plane = 1000.0f,
        .far_plane = 200000000.0f,

        .position = glm::vec3(10000000.0f),
        .target = glm::vec3(0.0f),
        .up = glm::vec3(0.0f, 1.0f, 0.0f),
    };
    m_camera = std::make_unique<Camera>(camera_config);

    U_projection.set(m_camera->projection_matrix());
    U_view.set(m_camera->view_matrix());

    m_window->register_framebuffer_resize_event([this, &U_projection](glm::ivec2 new_size) {
        m_camera->set_aspect_ratio((float)new_size.x / (float)new_size.y);

        glViewport(0, 0, new_size.x, new_size.y);

        U_projection.set(m_camera->projection_matrix());
    });

    LOG_INFO("Setting up lines");

    std::vector<glm::vec3> vertices = UnitCube::vertices();
    std::vector<unsigned int> indices = UnitCube::line_indices();

    octree::OctreeRenderManager octree_render_manager(octree::Space::earth());

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    Buffer cube_ibo(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
    Buffer cube_vbo;
    Buffer cube_instance_active_buffer;
    Buffer cube_instance_model_buffer;

    cube_vbo.set_data(vertices);
    cube_ibo.set_data(indices);

    // VERTICES
    cube_vbo.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    
    // INSTANCE ACTIVE
    cube_instance_active_buffer.bind();
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), NULL);
    glVertexAttribDivisor(1, 1);
    
    // INSTANCE MODEL MATRICES
    cube_instance_model_buffer.bind();

    size_t vec4_size = sizeof(glm::vec4);

    // LOC 2: COLUMN 0
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)0);
    glVertexAttribDivisor(2, 1);

    // LOC 3: COLUMN 1
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)(1 * vec4_size));
    glVertexAttribDivisor(3, 1);

    // LOC 4: COLUMN 2
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)(2 * vec4_size));
    glVertexAttribDivisor(4, 1);

    // LOC 5: COLUMN 3
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)(3 * vec4_size));
    glVertexAttribDivisor(5, 1);

    // INDICES
    cube_ibo.bind();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    m_last_draw_amount = 0;

    while (!m_window->should_close()) {

        m_window->poll_events();

        const double current_frame_time = glfwGetTime();
        const double frame_delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_settings_window();

        update_camera(frame_delta_time, U_view);

        octree::OctreeRenderIntent rendering_intent = octree_render_manager.generate_octree_render_intent(octree::Id::root(), m_camera->get_position(), false, m_refining_factor);

        m_last_draw_amount = rendering_intent.instance_count;

        cube_instance_active_buffer.set_data(rendering_intent.instances_active);
        cube_instance_model_buffer.set_data(rendering_intent.instances_model_mats);

        if (rendering_intent.min_scene_distance.has_value() && m_camera->get_near() != (float)rendering_intent.min_scene_distance.value() * 0.5f) {
            m_camera->set_near(rendering_intent.min_scene_distance.value() * 0.5f);
        }
        if (rendering_intent.max_scene_distance.has_value() && m_camera->get_far() != (float)rendering_intent.max_scene_distance.value() * 1.5f) {
            m_camera->set_far(rendering_intent.max_scene_distance.value() * 1.5f);
        }

        if (m_camera->is_view_matrix_outdated()) {
            U_view.set(m_camera->view_matrix());
        }
        if (m_camera->is_projection_matrix_outdated()) {
            U_projection.set(m_camera->projection_matrix());
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //glLineWidth(2.0f);
        glDrawElementsInstanced(GL_LINES, indices.size(), GL_UNSIGNED_INT, 0, rendering_intent.instance_count);

        // RENDER IMGUI AFTER OUR RENDERS
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        m_window->swapBuffers();
    }
}

void Application::update_camera(float frame_delta_time, Uniform<glm::mat4> U_view) {
    if (!m_camera) {
        LOG_WARN("Trying to update non-initialized camera!");
        return;
    }

    if (!m_nav_mode) {
        return;
    }

    /*glm::dvec2 scroll_delta = m_window->get_accumulated_scroll_delta();
    m_movement_speed = glm::max(m_movement_speed + glm::sign((float)scroll_delta.y), 0.0f);*/



    float movement_speed = m_movement_speed;
    float roll_speed = m_roll_speed;
    float mouse_sensitivity = m_mouse_sensitivity;

    if (m_window->is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
        movement_speed *= 5.0f;
        roll_speed *= 1.5f;
    }

    float roll = 0.0f;
    if (m_window->is_key_pressed(GLFW_KEY_Q)) {
        roll -= 1.00f;
    }
    if (m_window->is_key_pressed(GLFW_KEY_E)) {
        roll += 1.00f;
    }

    const glm::dvec2 cursor_position_delta = m_window->get_accumulated_cursor_delta();
    glm::vec3 camera_rotation_delta = glm::vec3(-cursor_position_delta.x, -cursor_position_delta.y, roll * roll_speed) * (float)frame_delta_time * mouse_sensitivity;

    if (glm::abs(camera_rotation_delta.x) + glm::abs(camera_rotation_delta.y) + glm::abs(camera_rotation_delta.z) != 0.0f) {
        m_camera->rotate(camera_rotation_delta.x, camera_rotation_delta.y, camera_rotation_delta.z);
    }

    glm::vec3 local_movement_delta(0.0f);

    if (m_window->is_key_pressed(GLFW_KEY_W) || m_window->is_key_pressed(GLFW_KEY_UP)) {
        local_movement_delta.z += 1.0f;
    }
    if (m_window->is_key_pressed(GLFW_KEY_S) || m_window->is_key_pressed(GLFW_KEY_DOWN)) {
        local_movement_delta.z -= 1.0f;
    }

    if (m_window->is_key_pressed(GLFW_KEY_D) || m_window->is_key_pressed(GLFW_KEY_RIGHT)) {
        local_movement_delta.x += 1.0f;
    }
    if (m_window->is_key_pressed(GLFW_KEY_A) || m_window->is_key_pressed(GLFW_KEY_LEFT)) {
        local_movement_delta.x -= 1.0f;
    }

    if (m_window->is_key_pressed(GLFW_KEY_SPACE)) {
        local_movement_delta.y += 1.0f;
    }
    if (m_window->is_key_pressed(GLFW_KEY_LEFT_CONTROL)) {
        local_movement_delta.y -= 1.0f;
    }

    float movement_magnitude = glm::length(local_movement_delta);
    local_movement_delta /= movement_magnitude;

    if (movement_magnitude != 0.0f) {
        m_camera->move_local(local_movement_delta * (float)frame_delta_time * movement_speed);
    }
}

Application::~Application() {
  LOG_INFO("Exiting");
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void Application::toggle_nav_mode() {
    m_nav_mode = !m_nav_mode;

    ImGuiIO& io = ImGui::GetIO();

    if (m_nav_mode && io.WantCaptureKeyboard) {
        // When Dear ImGui wants to capture the keyboard, do not switch to nav mode
        m_nav_mode = false;
    }

    if (m_nav_mode) {
        LOG_DEBUG("Entering Nav Mode");

        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;

        m_window->set_capture_mouse(true);
        m_window->clear_accumulated_cursor_delta();
    } else {
        LOG_DEBUG("Exiting Nav Mode");

        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;

        m_window->set_capture_mouse(false);
    }
}

void Application::init_glad() {
    // Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        LOG_ERROR_AND_EXIT("Failed to initialize OpenGL context");
    }

    // Successfully loaded OpenGL
    LOG_INFO("Loaded OpenGL {0}.{1}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
}

void Application::init_gl() {
#ifdef _DEBUG
  glEnable(GL_DEBUG_OUTPUT);
  // set debug callback
  glDebugMessageCallback(gl_debug_callback, nullptr);
#endif // _DEBUG

  glViewport(0, 0, m_width, m_height);
}

void Application::draw_settings_window() {
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

    float window_width = glm::clamp(main_viewport->Size.x * 0.2f, 250.0f, 350.0f);
    float window_height = main_viewport->Size.y;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiCond_Always);

    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoResize;

    if (!ImGui::Begin("Settings", NULL, flags)) {
        ImGui::End();
        return;
    }

    draw_octree_settings_section();
    draw_camera_settings_section();

    ImGui::End();
}

void Application::draw_camera_settings_section() {
    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (!ImGui::CollapsingHeader("Camera")) {
        return;
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::TreeNode("Position")) {
        bool cam_pos_edited = false;
        glm::dvec3 cam_pos = m_camera->get_position();

        ImGui::PushItemWidth(-80);

        ImGui::InputDouble("X", &cam_pos.x);

        if (ImGui::IsItemEdited()) {
            cam_pos_edited = true;
        }

        //ImGui::SetNextItemWidth(double_input_width);
        ImGui::InputDouble("Y", &cam_pos.y);

        if (ImGui::IsItemEdited()) {
            cam_pos_edited = true;
        }

        //ImGui::SetNextItemWidth(double_input_width);
        ImGui::InputDouble("Z", &cam_pos.z);

        if (ImGui::IsItemEdited() || cam_pos_edited) {
            cam_pos_edited = true;

            m_camera->set_position(cam_pos);
        }

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::TreeNode("Orientation")) {
        ImGui::SeparatorText("Euler Angles");
        bool euler_edited = false;
        glm::vec3 euler = glm::degrees(m_camera->get_rotation_euler());

        ImGui::SliderFloat("Pitch (X)", &euler.x, -180.0f, 180.0f);
        if (ImGui::IsItemEdited()) {
            euler_edited = true;
        }

        ImGui::SliderFloat("Yaw (Y)", &euler.y, -90.0f, 90.0f);
        if (ImGui::IsItemEdited()) {
            euler_edited = true;
        }

        ImGui::SliderFloat("Roll (Z)", &euler.z, -180.0f, 180.0f);
        if (ImGui::IsItemEdited() || euler_edited) {
            euler_edited = true;

            m_camera->set_rotation_euler(glm::radians(euler));
        }

        ImGui::SeparatorText("Quaternion");
        bool quat_edited = false;
        glm::quat quat = m_camera->get_rotation_quat();

        ImGui::SliderFloat("X", &quat.x, -1.0, 1.0);
        if (ImGui::IsItemEdited()) {
            quat_edited = true;
        }

        ImGui::SliderFloat("Y", &quat.y, -1.0, 1.0);
        if (ImGui::IsItemEdited()) {
            quat_edited = true;
        }

        ImGui::SliderFloat("Z", &quat.z, -1.0, 1.0);
        if (ImGui::IsItemEdited()) {
            quat_edited = true;
        }

        ImGui::SliderFloat("W", &quat.w, -1.0, 1.0);
        if (ImGui::IsItemEdited() || quat_edited) {
            quat_edited = true;

            m_camera->set_rotation_quat(glm::normalize(quat));
        }

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::TreeNode("Projection")) {
        float fov = m_camera->get_fov();

        ImGui::SliderFloat("FOV", &fov, 0.1f, 120.0f);
        if (ImGui::IsItemEdited()) {
            m_camera->set_fov(fov);
        }

        ImGui::Separator();

        float near = m_camera->get_near();
        ImGui::InputFloat("Near Plane", &near);
        if (ImGui::IsItemEdited()) {
            m_camera->set_near(near);
        }

        float far = m_camera->get_far();
        ImGui::InputFloat("Far Plane", &far);
        if (ImGui::IsItemEdited()) {
            m_camera->set_far(far);
        }

        ImGui::Separator();

        ImGui::TreePop();
    }
}

void Application::draw_octree_settings_section() {
    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (!ImGui::CollapsingHeader("Octree")) {
        return;
    }

    ImGui::PushItemWidth(-80);

    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::TreeNode("Stats")) {

        ImGui::Text("Nodes Rendered: %d", m_last_draw_amount);

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::TreeNode("Filtering")) {

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::TreeNode("Refining")) {
        std::array<std::string, 3> metrics = { "Distance", "Level", "DGNSDNFOL"};
        size_t selected_idx = 0;

        if (ImGui::BeginCombo("Metric", metrics[selected_idx].c_str())) {
            for (int i = 0; i < metrics.size(); i++) {
                bool selected = selected_idx == i;

                if (ImGui::Selectable(metrics[i].c_str(), selected)) {
                    selected_idx = i;
                    selected = true;
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::SliderFloat("Factor", &m_refining_factor, 0.0f, 2.0f)) {

        }

        ImGui::TreePop();
    }

    ImGui::PopItemWidth();
}

void Application::gl_debug_callback(GLenum source, GLenum type,
                                              GLuint id, GLenum severity,
                                              GLsizei length,
                                              const GLchar *message,
                                              const GLvoid *userParam) {
  std::stringstream stringStream;
  std::string sourceString;
  std::string typeString;
  std::string severityString;

  switch (source) {
  case GL_DEBUG_SOURCE_API: {
    sourceString = "API";
    break;
  }
  case GL_DEBUG_SOURCE_APPLICATION: {
    sourceString = "Application";
    break;
  }
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM: {
    sourceString = "Window System";
    break;
  }
  case GL_DEBUG_SOURCE_SHADER_COMPILER: {
    sourceString = "Shader Compiler";
    break;
  }
  case GL_DEBUG_SOURCE_THIRD_PARTY: {
    sourceString = "Third Party";
    break;
  }
  case GL_DEBUG_SOURCE_OTHER: {
    sourceString = "Other";
    break;
  }
  default: {
    sourceString = "Unknown";
    break;
  }
  }

  switch (type) {
  case GL_DEBUG_TYPE_ERROR: {
    typeString = "Error";
    break;
  }
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: {
    typeString = "Deprecated Behavior";
    break;
  }
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: {
    typeString = "Undefined Behavior";
    break;
  }
  case GL_DEBUG_TYPE_PORTABILITY_ARB: {
    typeString = "Portability";
    break;
  }
  case GL_DEBUG_TYPE_PERFORMANCE: {
    typeString = "Performance";
    break;
  }
  case GL_DEBUG_TYPE_OTHER: {
    typeString = "Other";
    break;
  }
  default: {
    typeString = "Unknown";
    break;
  }
  }

  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH: {
    severityString = "High";
    break;
  }
  case GL_DEBUG_SEVERITY_MEDIUM: {
    severityString = "Medium";
    break;
  }
  case GL_DEBUG_SEVERITY_LOW: {
    severityString = "Low";
    break;
  }
  case GL_DEBUG_SEVERITY_NOTIFICATION: {
    severityString = "Notification";
    break;
  }
  default: {
    severityString = "Unknown";
    break;
  }
  }

  stringStream << message;
  stringStream << " [Source = " << sourceString;
  stringStream << ", Type = " << typeString;
  stringStream << ", Severity = " << severityString;
  stringStream << ", ID = " << id << "]";

  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH: {
    LOG_ERROR(stringStream.str());
    break;
  }
  case GL_DEBUG_SEVERITY_MEDIUM: {
    LOG_WARN(stringStream.str());
    break;
  }
  case GL_DEBUG_SEVERITY_LOW: {
    LOG_INFO(stringStream.str());
    break;
  }
  case GL_DEBUG_SEVERITY_NOTIFICATION: {
    LOG_DEBUG(stringStream.str());
    break;
  }
  default: {
    LOG_TRACE(stringStream.str());
    break;
  }
  }
}
