#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <utility>
#include "tools/Polygon.h"
#include "tools/Cube.h"
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include "stb_image.h"
#include <random>
#include <chrono>

using namespace std;
using namespace glm;

int SCR_WIDTH = 1280;
int SCR_HEIGHT = 720;

Camera camera(vec3(9.0f, 2.0f, 20.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool speedUp = false;
bool stopMotion = false;
float orbitSpeed = 0.1f;
float acceleratedSpeed = 2.0f;

bool speedUpG = false;
bool speedUpH = false;

bool stopRotation = false;

bool earthInShadow = false;
bool moonInShadow = false;

float effectiveOrbitTime = 0.0f;

void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

pair<GLuint, int> createOrbitVAO(int segments, float radiusX, float radiusZ) {
    vector<vec3> verts;
    verts.reserve(segments);
    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * glm::pi<float>() * float(i) / float(segments);
        float x = radiusX * cos(theta);
        float z = radiusZ * sin(theta);
        verts.emplace_back(x, 0.0f, z);
    }
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(vec3), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return { VAO, static_cast<int>(verts.size()) };
}

vector<GLuint> loadTextures(vector<string> paths, GLuint wrapOption = GL_REPEAT, GLuint filterOption = GL_LINEAR) {
    vector<GLuint> textures;
    for (const string& path : paths) {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapOption);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapOption);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterOption);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterOption);
        int width, height, nrChannels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else {
            cout << "Failed to load texture: " << path << endl;
        }
        stbi_image_free(data);
        textures.push_back(texture);
    }
    return textures;
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Solar System", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    stbi_set_flip_vertically_on_load(true);
    Shader shader("./shaders/vs/L5.vs", "./shaders/fs/HW-model.fs");
    Model sphere("./models/ball.glb");
    vector<string> texturePaths = {
        "./textures/sun.jpg",
        "./textures/earth.jpg",
        "./textures/moon.jpg"
    };
    vector<GLuint> textures = loadTextures(texturePaths);

    int numStars = 300;
    float starDistance = 200.0f;
    float starSize = 0.4f;
    std::random_device rd;
    std::mt19937 generator(rd() ^ std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<float> distribution(-starDistance, starDistance);

    vector<Cube> stars;
    for (int i = 0; i < numStars; i++) {
        float x = distribution(generator);
        float y = distribution(generator);
        float z = distribution(generator);
        Cube star(vec3(0.0f), starSize, vec3(1.0f));
        mat4 modelStar = translate(mat4(1.0f), vec3(x, y, z));
        star.transformation(modelStar);
        stars.push_back(star);
    }

    auto earthOrbit = createOrbitVAO(256, 20.0f, 10.0f);
    auto moonOrbit = createOrbitVAO(128, 3.0f, 3.0f);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        float time = currentFrame;

        processInput(window);

        if (!stopMotion) {
            float speedMultiplier = (speedUpH || speedUpG || speedUp) ? (acceleratedSpeed / orbitSpeed) : 1.0f;
            effectiveOrbitTime += deltaTime * orbitSpeed * speedMultiplier;
        }

        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.use();
        shader.setInt("textureSample", 0);
        shader.setVec3("objectColor", vec3(1.0f));
        mat4 projection = perspective(radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
        shader.setMat4("projection", projection);
        shader.setMat4("view", camera.GetViewMatrix());
        shader.setVec3("viewPos", camera.Position);

        shader.setVec3("objectColor", vec3(1.0f));
        for (auto& star : stars) {
            star.draw(shader);
        }

        mat4 modelSun = mat4(1.0f);
        modelSun = scale(modelSun, vec3(4.0f));
        vec3 sunPos = vec3(modelSun * vec4(0, 0, 0, 1));

        shader.setMat4("model", modelSun);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        shader.setVec3("objectColor", vec3(4.0f, 3.5f, 2.5f));

        shader.setVec3("lightPos", sunPos);
        shader.setVec3("pointLights[0].position", sunPos);
        shader.setVec3("pointLights[0].ambient", vec3(0.3f, 0.25f, 0.1f));
        shader.setVec3("pointLights[0].diffuse", vec3(1.3f, 1.1f, 0.8f));
        shader.setVec3("pointLights[0].specular", vec3(1.3f, 1.1f, 0.9f));
        shader.setFloat("pointLights[0].constant", 1.0f);
        shader.setFloat("pointLights[0].linear", 0.007f);
        shader.setFloat("pointLights[0].quadratic", 0.0002f);

        shader.setVec3("pointLights[1].position", vec3(1000.0f));
        shader.setVec3("pointLights[1].ambient", vec3(0.0f));
        shader.setVec3("pointLights[1].diffuse", vec3(0.0f));
        shader.setVec3("pointLights[1].specular", vec3(0.0f));
        shader.setFloat("pointLights[1].constant", 1.0f);
        shader.setFloat("pointLights[1].linear", 0.09f);
        shader.setFloat("pointLights[1].quadratic", 0.032f);
        sphere.Draw(shader);

        vec3 earthPos = vec3(
            20.0f * cos(effectiveOrbitTime),
            0.0f,
            10.0f * sin(effectiveOrbitTime)
        );

        float earthRotationSpeed = stopRotation ? 0.0f : 0.5f;
        float moonRotationSpeed = stopRotation ? 0.0f : 0.5f;

        shader.setVec3("objectColor", vec3(0.8f, 0.8f, 0.8f));
        glBindTexture(GL_TEXTURE_2D, 0);
        glLineWidth(2.5f);
        glBindVertexArray(earthOrbit.first);
        shader.setMat4("model", mat4(1.0f));
        glDrawArrays(GL_LINE_LOOP, 0, earthOrbit.second);
        glBindVertexArray(0);

        float moonOrbitAngle = effectiveOrbitTime * 2.0f;
        vec3 moonPos = earthPos + vec3(
            3.0f * cos(moonOrbitAngle),
            0.0f,
            3.0f * sin(moonOrbitAngle)
        );

        shader.setVec3("pointLights[2].position", moonPos);
        shader.setFloat("pointLights[2].constant", 1.0f);
        shader.setFloat("pointLights[2].linear", 0.03f);
        shader.setFloat("pointLights[2].quadratic", 0.001f);
        shader.setVec3("pointLights[2].ambient", vec3(0.01f));
        shader.setVec3("pointLights[2].diffuse", vec3(0.06f));
        shader.setVec3("pointLights[2].specular", vec3(0.08f));

        if (moonInShadow) {
            shader.setVec3("pointLights[2].ambient", vec3(0.03f));
            shader.setVec3("pointLights[2].diffuse", vec3(0.25f));
            shader.setVec3("pointLights[2].specular", vec3(0.30f));
        }


        mat4 modelEarth = mat4(1.0f);
        modelEarth = translate(modelEarth, earthPos);
        modelEarth = rotate(modelEarth, time * earthRotationSpeed, vec3(0, 1, 0));
        modelEarth = scale(modelEarth, vec3(1.0f));
        shader.setMat4("model", modelEarth);
        glBindTexture(GL_TEXTURE_2D, textures[1]);

        if (earthInShadow) {
            shader.setVec3("objectColor", vec3(0.1f, 0.1f, 0.1f));
        }
        else {
            shader.setVec3("objectColor", vec3(1.0f));
        }

        sphere.Draw(shader);

        glBindVertexArray(moonOrbit.first);
        mat4 moonOrbitModel = translate(mat4(1.0f), earthPos);
        shader.setMat4("model", moonOrbitModel);
        shader.setVec3("objectColor", vec3(0.7f, 0.7f, 0.7f));
        glDrawArrays(GL_LINE_LOOP, 0, moonOrbit.second);
        glBindVertexArray(0);

        mat4 modelMoon = mat4(1.0f);
        modelMoon = translate(modelMoon, moonPos);
        modelMoon = scale(modelMoon, vec3(0.4f));
        modelMoon = rotate(modelMoon, time * moonRotationSpeed, vec3(0.0f, 1.0f, 0.0f));
        shader.setMat4("model", modelMoon);
        glBindTexture(GL_TEXTURE_2D, textures[2]);

        if (moonInShadow) {
            shader.setVec3("objectColor", vec3(0.1f, 0.1f, 0.1f));
        }
        else {
            shader.setVec3("objectColor", vec3(1.0f));
        }

        sphere.Draw(shader);


        if ((speedUpH || speedUpG) && !stopRotation) {

            float tolerance = 0.005f;

            vec3 sunPos_XZ = vec3(0.0f, 0.0f, 0.0f);

            vec3 V_SE = normalize(earthPos - sunPos_XZ);
            vec3 V_EM = normalize(moonPos - earthPos);

            float dotProduct = dot(V_SE, V_EM);

            if (speedUpG) {
                if (dotProduct < -1.0f + tolerance) {
                    stopRotation = true;
                    stopMotion = true;
                    earthInShadow = true;
                    speedUpG = false;
                }
            }

            if (speedUpH) {
                if (dotProduct > 1.0f - tolerance) {
                    stopRotation = true;
                    stopMotion = true;
                    moonInShadow = true;
                    speedUpH = false;
                }
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &earthOrbit.first);
    glDeleteVertexArrays(1, &moonOrbit.first);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
        speedUpH = true;
        speedUpG = false;
        stopMotion = false;
        stopRotation = false;
        earthInShadow = false;
    }

    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
        speedUpG = true;
        speedUpH = false;
        stopMotion = false;
        stopRotation = false;
        moonInShadow = false;
    }

    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
        speedUpH = false;
        speedUpG = false;
        stopMotion = false;
        stopRotation = false;
        speedUp = false;
        earthInShadow = false;
        moonInShadow = false;
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}
