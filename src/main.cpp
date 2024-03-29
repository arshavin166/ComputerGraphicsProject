#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

//FUNCTIONS-------------------------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
unsigned int loadCubemap(vector<std::string> faces);
void renderQuad();
void renderCube();

//SETTINGS--------------------------------------------------------------------------------------------------------------
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

//CAMERA----------------------------------------------------------------------------------------------------------------
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

//GLOBAL VARIABLES------------------------------------------------------------------------------------------------------
bool firstMouse = true;
bool blinn = false;
bool blinnON = false;
bool bloom = true;
bool bloomON = false;
bool hdr = false;
bool hdrON = false;
bool gammaEnabled = false;
bool gammaON = false;
float exposure = 0.77f;
bool grayscale = false;
bool grayscaleON = false;

//TIMING----------------------------------------------------------------------------------------------------------------
float deltaTime = 0.0f;
float lastFrame = 0.0f;

//LIGHTS----------------------------------------------------------------------------------------------------------------
struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

};

struct DirLight {
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct SpotLight {
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

};

//PROGRAM STATE---------------------------------------------------------------------------------------------------------
struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    ProgramState(): camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;
bool cameraInfo = false;

void DrawImGui(ProgramState *programState);

int main() {
//GLFW: INITIALIZATION AND CONFIGURATION--------------------------------------------------------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

//GLFW: WINDOW CREATION/------------------------------------------------------------------------------------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "nismo znali da je nevidljiv", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    //tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

//GLAD: LOAD ALL OPENGL FUNCTION POINERS--------------------------------------------------------------------------------

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

//IMGUI INITIALIZATION--------------------------------------------------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

//GLOBAL OPENGL STATE---------------------------------------------------------------------------------------------------
    glEnable(GL_DEPTH_TEST);

//BLENDING--------------------------------------------------------------------------------------------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//FACE-CULLING----------------------------------------------------------------------------------------------------------
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CW);

//SHADERS---------------------------------------------------------------------------------------------------------------
    Shader modelShader("resources/shaders/model_lighting.vs", "resources/shaders/model_lighting.fs");
    Shader blendingShader("resources/shaders/model_lighting.vs", "resources/shaders/blending.fs" );
    Shader cubemapShader("resources/shaders/cubemaps.vs", "resources/shaders/cubemaps.fs");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader lightShader("resources/shaders/model_lighting.vs", "resources/shaders/lightBullet.fs");
    Shader blurShader("resources/shaders/blur.vs", "resources/shaders/blur.fs");
    Shader bloomShader("resources/shaders/bloom.vs", "resources/shaders/bloom.fs");

//MODELS----------------------------------------------------------------------------------------------------------------
    //set stbi false
    stbi_set_flip_vertically_on_load(false);

    // grass
    Model grassModel("resources/objects/grass/grass.obj");
    grassModel.SetShaderTextureNamePrefix("material.");

    Model airdefModel("resources/objects/defense/zsu.obj");
    grassModel.SetShaderTextureNamePrefix("material.");

    Model airplane1Model("resources/objects/airplane1/F-16D.obj");
    airplane1Model.SetShaderTextureNamePrefix("material.");

    Model airplane2Model("resources/objects/airplane2/Harrier.obj");
    airplane2Model.SetShaderTextureNamePrefix("material.");

    Model rocketModel("resources/objects/rocket/Missile AIM-120 D [AMRAAM].obj");
    rocketModel.SetShaderTextureNamePrefix("material.");

    Model moonModel("resources/objects/moon/Moon 2K.obj");
    moonModel.SetShaderTextureNamePrefix("material.");

    //set stbi true
    stbi_set_flip_vertically_on_load(true);
//HDR/BLOOM-------------------------------------------------------------------------------------------------------------

    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++){
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorBuffers[i]);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D_MULTISAMPLE, colorBuffers[i], 0);
    }

    unsigned int textureColorBufferMultiSampled;
    glGenTextures(1, &textureColorBufferMultiSampled);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureColorBufferMultiSampled);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, SCR_WIDTH, SCR_HEIGHT, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, textureColorBufferMultiSampled, 0);

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    //which color attachment we'll use for rendering
    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);
    //is framebuffer complete?
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++){
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, pingpongColorbuffers[i]);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, pingpongColorbuffers[i], 0);
        //are framebuffers complete?
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

//LIGHTS----------------------------------------------------------------------------------------------------------------
    //directional light
    DirLight directional;
    directional.direction = glm::vec3(-0.7f, -1.0f, -0.4f);
    directional.ambient = glm::vec3(0.09f);
    directional.diffuse = glm::vec3(0.4f);
    directional.specular = glm::vec3(0.5f);

    //pointlights
    std::vector<glm::vec3> pointLightPositions;
    pointLightPositions.push_back(glm::vec3(-14.0f, 213.0f, 55.0f));
    pointLightPositions.push_back(glm::vec3(-13.0f, 207.0f, 53.0f));
    pointLightPositions.push_back(glm::vec3(-18.0f, 205.0f, 54.0f));
    pointLightPositions.push_back(glm::vec3(-21.0f, 190.0f, 59.0f));
    pointLightPositions.push_back(glm::vec3(-10.0f, 178.0f, 55.0f));
    pointLightPositions.push_back(glm::vec3(0.0f, 164.0f, 52.0f));
    pointLightPositions.push_back(glm::vec3(11.0f, 150.0f, 50.0f));
    pointLightPositions.push_back(glm::vec3(23.0f, 139.0f, 48.5f));
    pointLightPositions.push_back(glm::vec3(28.0f, 125.0f, 48.2f));
    pointLightPositions.push_back(glm::vec3(36.0f, 115.0f, 49.57f));
    pointLightPositions.push_back(glm::vec3(48.0f, 101.0f, 52.2f));
    pointLightPositions.push_back(glm::vec3(54.0f, 91.0f, 46.1f));
    pointLightPositions.push_back(glm::vec3(62.0f, 79.0f, 45.3f));
    pointLightPositions.push_back(glm::vec3(70.0f, 62.0f, 43.0f));
    pointLightPositions.push_back(glm::vec3(68.0f, 52.0f, 47.0f));
    pointLightPositions.push_back(glm::vec3(73.0f, 43.0f, 49.0f));
    pointLightPositions.push_back(glm::vec3(77.0f, 33.0f, 52.0f));
    pointLightPositions.push_back(glm::vec3(81.0f, 22.0f, 54.0f));

    PointLight pointLights;
    pointLights.ambient = glm::vec3(1.0f, 0.05f, 0.01f);
    pointLights.diffuse = glm::vec3(1.0f, 0.05f, 0.01f);
    pointLights.specular = glm::vec3(5.5f, 3.7f, 1.0f);

    //spotlight
    SpotLight spotlight;
    spotlight.position = programState->camera.Position;
    spotlight.direction = programState->camera.Front;
    spotlight.ambient = glm::vec3(0.2f);
    spotlight.diffuse = glm::vec3(1.0f, 0.894f, 0.627f);
    spotlight.specular = glm::vec3(1.0f, 0.894f, 0.627f);
    spotlight.cutOff = glm::cos(glm::radians(12.5f));
    spotlight.outerCutOff = glm::cos(glm::radians(17.0f));


//SKYBOX----------------------------------------------------------------------------------------------------------------
    float skyboxVertices[] = {
            //  positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    vector<std::string> faces
            {
                    FileSystem::getPath("resources/textures/skybox/right.tga"),
                    FileSystem::getPath("resources/textures/skybox/left.tga"),
                    FileSystem::getPath("resources/textures/skybox/up.tga"),
                    FileSystem::getPath("resources/textures/skybox/down.tga"),
                    FileSystem::getPath("resources/textures/skybox/front.tga"),
                    FileSystem::getPath("resources/textures/skybox/back.tga")
            };
    unsigned int cubemapTexture = loadCubemap(faces);

//SHADERS CONFIGURATION-------------------------------------------------------------------------------------------------
    cubemapShader.use();
    cubemapShader.setInt("texture1", 0);
    blendingShader.use();
    blendingShader.setInt("texture1", 0);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);
    lightShader.use();
    blurShader.use();
    blurShader.setInt("image", 0);
    bloomShader.use();
    bloomShader.setInt("scene", 0);
    bloomShader.setInt("bloomBlur", 1);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    //draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

//RENDER LOOP-----------------------------------------------------------------------------------------------------------
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        //render
//        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //don't forget to enable shader before setting uniforms
        modelShader.use();

        //view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 500.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        modelShader.setFloat("material.shininess", 16.0f);

        //Directional light
        modelShader.setVec3("directional.direction", directional.direction);
        modelShader.setVec3("directional.ambient", directional.ambient);
        modelShader.setVec3("directional.diffuse", directional.diffuse);
        modelShader.setVec3("directional.specular", directional.specular);

        //Point Lights
        for(unsigned int i = 0; i < pointLightPositions.size(); i++){
            modelShader.setVec3("pointlight[" + std::to_string(i) + "].position", pointLightPositions[i]);
            modelShader.setVec3("pointlight[" + std::to_string(i) + "].ambient", pointLights.ambient * 1.0f);
            modelShader.setVec3("pointlight[" + std::to_string(i) + "].diffuse", pointLights.diffuse * 0.05f);
            modelShader.setVec3("pointlight[" + std::to_string(i) + "].specular", pointLights.specular * 0.01f);
        }

        //Spotlight
        modelShader.setVec3("spotlight.position", programState->camera.Position);
        modelShader.setVec3("spotlight.direction", programState->camera.Front);
        modelShader.setVec3("spotlight.ambient", spotlight.ambient);
        modelShader.setVec3("spotlight.diffuse", spotlight.diffuse);
        modelShader.setFloat("spotlight.cutOff", spotlight.cutOff);
        modelShader.setFloat("spotlight.outerCutOff", spotlight.outerCutOff);
        modelShader.setBool("blinn", blinn);

//      light bullets
        glm::mat4 model = glm::mat4(1.0f);
        lightShader.setMat4("projection", projection);
        lightShader.setMat4("view", view);
        for (unsigned int i = 0; i < 20; i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, pointLightPositions[i]);
            model = glm::scale(model, glm::vec3(0.45f)); // Make it a smaller cube
            lightShader.setMat4("model", model);
            lightShader.setVec3("lightColor",glm::vec3(1.0f, 0.0f, 0.0f));
            renderCube();
        }

        //render grass
        glm::mat4 modelGrass = glm::mat4(1.0f);
        modelGrass = glm::translate(modelGrass, glm::vec3(0.0f, -20.0f, 0.0f));
        float angle1 = 90.0f * 3.14159f / 180.0f;
        modelGrass = glm::rotate(modelGrass, angle1, glm::vec3(-1.0f, 0.0f, 0.0f)); // rotate
        modelGrass = glm::scale(modelGrass, glm::vec3(glm::vec3(1.0f)));
        modelShader.setMat4("model", modelGrass);
        grassModel.Draw(modelShader);

        //render airplane1
        glm::mat4 modelf16 = glm::mat4(1.0f);
        modelf16 = glm::translate(modelf16, glm::vec3(-18, 180.0f, -241.0f));
        modelf16 = glm::rotate(modelf16, -35.0f * 3.14159f / 160.0f , glm::vec3(0.0f, 0.0f, 1.0f));
        modelf16 = glm::scale(modelf16, glm::vec3(glm::vec3(4.5f)));
        modelShader.setMat4("model", modelf16);
        airplane1Model.Draw(modelShader);

        //render rocket
        glm::mat4 modelRocket = glm::mat4(1.0f);
        modelRocket = glm::translate(modelRocket, glm::vec3(-20, 181.5f, -80.0f));
        modelRocket = glm::rotate(modelRocket, (float)glm::radians(-90.0), glm::vec3(0.0f, 0.0f, 1.0f));
        modelRocket = glm::rotate(modelRocket, (float)glm::radians(90.0), glm::vec3(1.0f, 0.0f, 0.0f));
        modelRocket = glm::scale(modelRocket, glm::vec3(glm::vec3(0.07f)));
        modelShader.setMat4("model", modelRocket);
        rocketModel.Draw(modelShader);

        //render airplane2
        glm::mat4 modelHarrier = glm::mat4(1.0f);
        modelHarrier = glm::translate(modelHarrier, glm::vec3(-10, 180.0f, 55.0f));
        modelHarrier = glm::rotate(modelHarrier, -35.0f * 3.14159f / 160.0f , glm::vec3(0.0f, 0.0f, 1.0f));
        modelHarrier = glm::scale(modelHarrier, glm::vec3(glm::vec3(4.3f)));
        modelShader.setMat4("model", modelHarrier);
        airplane2Model.Draw(modelShader);

        //reder defense
        glm::mat4 modelZsu = glm::mat4(1.0f);
        modelZsu = glm::translate(modelZsu, glm::vec3(115.0f, -14.0f, 34.0f));
        modelZsu = glm::rotate(modelZsu, (float)glm::radians(-90.0), glm::vec3(0.0f, 1.0, 0.0f));
        modelZsu = glm::rotate(modelZsu, (float)glm::radians(180.0), glm::vec3(1.0f, 0.0f, 0.0f));
        modelZsu = glm::scale(modelZsu, glm::vec3(glm::vec3(0.65f)));
        modelShader.setMat4("model", modelZsu);
        airdefModel.Draw(modelShader);

        //blending
        blendingShader.use();
        blendingShader.setVec3("viewPosition", programState->camera.Position);
        blendingShader.setFloat("material.shininess", 32.0f);
        blendingShader.setMat4("projection", projection);
        blendingShader.setMat4("view", view);
        blendingShader.setVec3("dirLight.direction", glm::vec3(-0.547f, -0.727f, 0.415f));
        blendingShader.setVec3("dirLight.ambient", glm::vec3(0.35f));
        blendingShader.setVec3("dirLight.diffuse", glm::vec3(0.4f));
        blendingShader.setVec3("dirLight.specular", glm::vec3(0.2f));

        //render moon
        glm::mat4 modelMoon= glm::mat4(1.0f);
        modelMoon = glm::translate(modelMoon,glm::vec3(-57.0f, 300.0f, 28.0f));
        modelMoon = glm::scale(modelMoon, glm::vec3(4.0f));
        modelMoon = glm::rotate( modelMoon,glm::radians(90.0f), glm::vec3(1.0f,0.0f , 0.0f));
        modelMoon = glm::rotate(modelMoon,glm::radians(currentFrame*20), glm::vec3(0.0f ,1.0f, 0.0f));
        modelMoon = glm::rotate(modelMoon,glm::radians(currentFrame*40), glm::vec3(1.0f , 0.0f,0.0f));
        blendingShader.setMat4("model", modelMoon);
        moonModel.Draw(blendingShader);

        //render skybox
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        //skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        //bloom, hdr
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 12;
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setInt("horizontal", horizontal);
            blurShader.setInt("SCR_WIDTH", SCR_WIDTH);
            blurShader.setInt("SCR_HEIGHT", SCR_HEIGHT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        bloomShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, pingpongColorbuffers[!horizontal]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureColorBufferMultiSampled);
        bloomShader.setInt("SCR_WIDTH", SCR_WIDTH);
        bloomShader.setInt("SCR_HEIGHT", SCR_HEIGHT);

        bloomShader.setInt("hdr", hdr);
        bloomShader.setInt("bloom", bloom);
        bloomShader.setFloat("exposure", exposure);
        bloomShader.setInt("gammaEnabled",gammaEnabled);
        bloomShader.setInt("grayscale", grayscale);
        renderQuad();

        std::cout << "hdr: " << (hdr ? "on" : "off") << std::endl;
        std::cout << "bloom: " << (bloom ? "on" : "off") << "| exposure: " << exposure << std::endl;
        std::cout << (gammaEnabled ? "Gamma enabled" : "Gamma disabled") << std::endl;


        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        //glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    //glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
    return 0;
}

//process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly-------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);

    //GAMMA correction
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS && !gammaON){
        gammaEnabled = !gammaEnabled;
        gammaON = true;
    }
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_RELEASE){
        gammaON = false;
    }

    //Blinn-Phong light
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS && !blinnON)
    {
        blinn = !blinn;
        blinnON = true;
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_RELEASE)
    {
        blinnON = false;
    }

    //Bloom
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bloomON)
    {
        bloom = !bloom;
        bloomON = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        bloomON = false;
    }

    //HDR
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS && !hdrON)
    {
        hdr = !hdr;
        hdrON = true;
    }
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_RELEASE)
    {
        hdrON = false;
    }

    //EXPOSURE
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS){
        if (exposure > 0.0f)
            exposure -= 0.001f;
        else
            exposure = 0.0f;
    }
    else if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS){
        exposure += 0.001f;
    }

    //GRAYSCALE
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS && !grayscaleON){
        grayscale = !grayscale;
        grayscaleON = true;
    }
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_RELEASE){
        grayscaleON= false;
    }
}

//GLFW: whenever the window size changed (by OS or user resize) this callback function executes-------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

//GLFW: whenever the mouse moves, this callback is called---=-----------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

//GLFW: whenever the mouse scroll wheel scrolls, this callback is called------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

//CUBEMAP LOADING-------------------------------------------------------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

//DRAW IMGUI------------------------------------------------------------------------------------------------------------
void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        static float f = 0.0f;
        ImGui::Begin("Universe");
        ImGui::Text("Universe");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
        //ImGui::DragFloat3("Backpack position", (float*)&programState->backpackPosition);
        //ImGui::DragFloat("Backpack scale", &programState->backpackScale, 0.05, 0.1, 4.0);

        //ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        //ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 1.0);
        //ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 1.0);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

//KEYCALLBACK-----------------------------------------------------------------------------------------------------------
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}
//REDNDER QUAD----------------------------------------------------------------------------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad(){
    if (quadVAO == 0){
        float quadVertices[] = {
                // positions            // texCoords
                -1.0f,  1.0f,  0.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 0.0f,
                1.0f, -1.0f,  1.0f, 0.0f,

                -1.0f,  1.0f,  0.0f, 1.0f,
                1.0f, -1.0f,  1.0f, 0.0f,
                1.0f,  1.0f,  1.0f, 1.0f
        };

        //setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

//DRAW LIGHTCUBES - BULLETS---------------------------------------------------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube(){
    // initialize (if necessary)
    if (cubeVAO == 0){
        float vertices[] = {
                //position                      //normals                           //texture coords
                // back face
                -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
                // front face
                -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
                -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                // left face
                -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
                -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                // right face
                1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left
                // bottom face
                -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
                1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                // top face
                -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right
                1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}