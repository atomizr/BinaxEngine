// main.cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>

// -------------------------- Константы --------------------------
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const float PI = 3.14159265359f;

// -------------------------- Глобальные состояния --------------------------
struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 2.5f;
    float sensitivity = 0.1f;
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right;
    bool mouseCaptured = false;

    void updateVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, up));
    }
};

struct Material {
    glm::vec3 albedo = glm::vec3(0.8f, 0.2f, 0.2f);
    float roughness = 0.4f;
    float metallic = 0.2f;
};

struct Light {
    glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    glm::vec3 color = glm::vec3(1.0f, 0.9f, 0.8f);
    float intensity = 1.0f;
};

Camera camera;
Material material;
Light sunLight;
bool showImGuiDemo = false;

// -------------------------- Шейдеры --------------------------
GLuint compileShader(const std::string& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        spdlog::error("Shader compilation failed: {}", infoLog);
        return 0;
    }
    return shader;
}

GLuint createProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vertex = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragment = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        spdlog::error("Program linking failed: {}", infoLog);
        return 0;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

// Вершинный шейдер геометрического прохода
const char* geomVertexShader = R"(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 WorldPos;
out vec3 Normal;

void main() {
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)";

// Фрагментный шейдер геометрического прохода
const char* geomFragmentShader = R"(
#version 460 core
layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec3 gAlbedo;
layout(location = 3) out vec2 gRoughnessMetallic;

uniform vec3 uAlbedo;
uniform float uRoughness;
uniform float uMetallic;

in vec3 WorldPos;
in vec3 Normal;

void main() {
    gPosition = WorldPos;
    gNormal = normalize(Normal);
    gAlbedo = uAlbedo;
    gRoughnessMetallic = vec2(uRoughness, uMetallic);
}
)";

// Шейдер освещения (deferred lighting)
const char* lightVertexShader = R"(
#version 460 core
layout(location = 0) in vec2 aPos;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aPos * 0.5 + 0.5;
}
)";

const char* lightFragmentShader = R"(
#version 460 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gRoughnessMetallic;

uniform vec3 uViewPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform float uLightIntensity;

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// PBR functions
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

void main() {
    // Получаем данные из G-buffer
    vec3 WorldPos = texture(gPosition, TexCoord).rgb;
    vec3 Normal = texture(gNormal, TexCoord).rgb;
    vec3 Albedo = texture(gAlbedo, TexCoord).rgb;
    vec2 RoughnessMetallic = texture(gRoughnessMetallic, TexCoord).rg;
    float Roughness = RoughnessMetallic.r;
    float Metallic = RoughnessMetallic.g;

    // Если фон (глубина = 1) - просто выводим цвет фона
    if (WorldPos == vec3(0.0)) {
        FragColor = vec4(0.1, 0.1, 0.15, 1.0);
        return;
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(uViewPos - WorldPos);

    // Направленный свет
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Albedo, Metallic);

    // BRDF
    float NDF = DistributionGGX(N, H, Roughness);
    float G = GeometrySmith(N, V, L, Roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - Metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = kD * Albedo / PI;

    vec3 radiance = uLightColor * uLightIntensity;

    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    // Ambient (простой)
    vec3 ambient = vec3(0.03) * Albedo;
    vec3 color = ambient + Lo;

    // Tone mapping (простой)
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}
)";

// -------------------------- Создание геометрии --------------------------
void createCube(GLuint& VAO, GLuint& VBO, GLuint& EBO) {
    float vertices[] = {
        // positions          // normals
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,

        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,

         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f
    };
    unsigned int indices[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        8,9,10, 10,11,8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void createQuad(GLuint& VAO, GLuint& VBO) {
    float vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    unsigned int indices[] = {0,1,2, 2,3,0};

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    GLuint EBO;
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// -------------------------- Загрузка конфигурации из JSON --------------------------
void loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        nlohmann::json j;
        file >> j;
        if (j.contains("albedo")) {
            material.albedo = glm::vec3(j["albedo"][0], j["albedo"][1], j["albedo"][2]);
        }
        if (j.contains("roughness")) material.roughness = j["roughness"];
        if (j.contains("metallic")) material.metallic = j["metallic"];
        if (j.contains("light_direction")) {
            sunLight.direction = glm::vec3(j["light_direction"][0], j["light_direction"][1], j["light_direction"][2]);
        }
        if (j.contains("light_color")) {
            sunLight.color = glm::vec3(j["light_color"][0], j["light_color"][1], j["light_color"][2]);
        }
        if (j.contains("light_intensity")) sunLight.intensity = j["light_intensity"];
        spdlog::info("Loaded config from {}", filename);
    } else {
        spdlog::warn("Config file not found, using defaults");
    }
}

void saveConfig(const std::string& filename) {
    nlohmann::json j;
    j["albedo"] = {material.albedo.r, material.albedo.g, material.albedo.b};
    j["roughness"] = material.roughness;
    j["metallic"] = material.metallic;
    j["light_direction"] = {sunLight.direction.x, sunLight.direction.y, sunLight.direction.z};
    j["light_color"] = {sunLight.color.r, sunLight.color.g, sunLight.color.b};
    j["light_intensity"] = sunLight.intensity;
    std::ofstream file(filename);
    file << j.dump(4);
    spdlog::info("Saved config to {}", filename);
}

// -------------------------- Обработка ввода --------------------------
void processInput(GLFWwindow* window, float deltaTime) {
    // Движение камеры
    float speed = camera.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        speed *= 2.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.position += camera.front * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.position -= camera.front * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.position -= camera.right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.position += camera.right * speed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera.position += camera.up * speed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera.position -= camera.up * speed;

    // Захват мыши по ПКМ
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!camera.mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            camera.mouseCaptured = true;
        }
    } else {
        if (camera.mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            camera.mouseCaptured = false;
        }
    }

    // Вращение камеры (если захвачена)
    if (camera.mouseCaptured) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        static double lastX = SCR_WIDTH/2, lastY = SCR_HEIGHT/2;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            double xoffset = xpos - lastX;
            double yoffset = lastY - ypos; // инвертировано
            camera.yaw += xoffset * camera.sensitivity;
            camera.pitch += yoffset * camera.sensitivity;
            camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);
            camera.updateVectors();
        }
        lastX = xpos;
        lastY = ypos;
    } else {
        // Сброс последних координат, чтобы не было скачка
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        static double lastX = xpos, lastY = ypos;
        lastX = xpos;
        lastY = ypos;
    }
}

// -------------------------- Основная функция --------------------------
int main() {
    // Инициализация spdlog
    spdlog::set_level(spdlog::level::debug);

    // Инициализация GLFW
    if (!glfwInit()) {
        spdlog::error("Failed to initialize GLFW");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Deferred PBR Engine", nullptr, nullptr);
    if (!window) {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // Загрузка GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        spdlog::error("Failed to initialize GLAD");
        return -1;
    }

    // Вывод информации об OpenGL
    spdlog::info("OpenGL version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    spdlog::info("GLSL version: {}", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

    // Настройка ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    // Загрузка конфигурации
    loadConfig("config.json");

    // Настройка G-buffer
    GLuint gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    GLuint gPosition, gNormal, gAlbedo, gRoughnessMetallic;
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

    glGenTextures(1, &gRoughnessMetallic);
    glBindTexture(GL_TEXTURE_2D, gRoughnessMetallic);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gRoughnessMetallic, 0);

    GLuint attachments[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(4, attachments);

    // Буфер глубины
    GLuint rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        spdlog::error("G-buffer framebuffer not complete!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Создание шейдерных программ
    GLuint geomProgram = createProgram(geomVertexShader, geomFragmentShader);
    GLuint lightProgram = createProgram(lightVertexShader, lightFragmentShader);

    // Геометрия
    GLuint cubeVAO, cubeVBO, cubeEBO;
    createCube(cubeVAO, cubeVBO, cubeEBO);

    GLuint quadVAO, quadVBO;
    createQuad(quadVAO, quadVBO);

    // Настройка состояния OpenGL
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    // Основной цикл
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    camera.updateVectors();

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Обработка ввода
        processInput(window, deltaTime);

        // ----- Рендеринг в G-buffer -----
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(geomProgram);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(geomProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(geomProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(geomProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3fv(glGetUniformLocation(geomProgram, "uAlbedo"), 1, glm::value_ptr(material.albedo));
        glUniform1f(glGetUniformLocation(geomProgram, "uRoughness"), material.roughness);
        glUniform1f(glGetUniformLocation(geomProgram, "uMetallic"), material.metallic);

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Можно добавить ещё объекты, но для демонстрации один куб

        // ----- Освещение (deferred) -----
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(lightProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedo);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, gRoughnessMetallic);

        glUniform1i(glGetUniformLocation(lightProgram, "gPosition"), 0);
        glUniform1i(glGetUniformLocation(lightProgram, "gNormal"), 1);
        glUniform1i(glGetUniformLocation(lightProgram, "gAlbedo"), 2);
        glUniform1i(glGetUniformLocation(lightProgram, "gRoughnessMetallic"), 3);

        glUniform3fv(glGetUniformLocation(lightProgram, "uViewPos"), 1, glm::value_ptr(camera.position));
        glUniform3fv(glGetUniformLocation(lightProgram, "uLightDir"), 1, glm::value_ptr(sunLight.direction));
        glUniform3fv(glGetUniformLocation(lightProgram, "uLightColor"), 1, glm::value_ptr(sunLight.color));
        glUniform1f(glGetUniformLocation(lightProgram, "uLightIntensity"), sunLight.intensity);

        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // ----- ImGui -----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Окно настроек
        ImGui::Begin("PBR Settings");
        ImGui::ColorEdit3("Albedo", glm::value_ptr(material.albedo));
        ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Sun Light");
        ImGui::SliderFloat3("Direction", glm::value_ptr(sunLight.direction), -1.0f, 1.0f);
        ImGui::ColorEdit3("Color", glm::value_ptr(sunLight.color));
        ImGui::SliderFloat("Intensity", &sunLight.intensity, 0.0f, 5.0f);
        if (ImGui::Button("Save Config")) {
            saveConfig("config.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Config")) {
            loadConfig("config.json");
        }
        ImGui::End();

        // ImGui::ShowDemoWindow(&showImGuiDemo);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Обмен буферов
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Очистка ресурсов
    glDeleteFramebuffers(1, &gBuffer);
    glDeleteTextures(1, &gPosition);
    glDeleteTextures(1, &gNormal);
    glDeleteTextures(1, &gAlbedo);
    glDeleteTextures(1, &gRoughnessMetallic);
    glDeleteRenderbuffers(1, &rboDepth);
    glDeleteProgram(geomProgram);
    glDeleteProgram(lightProgram);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}