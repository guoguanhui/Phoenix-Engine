#include <glm/gtc/matrix_transform.hpp>

#include <engine/shadow_common.h>
#include <engine/camera.h>
#include <engine/strings.h>
#include <engine/common.h>

#include <array>
#include <time.h>
#include <iostream>

void framebufferSizeCallback(GLFWwindow*, int, int);
void cursorPosCallback(GLFWwindow*, double, double);
void scrollCallback(GLFWwindow*, double, double);

void generateRandom3DTextures();
void setupRenderToTexture();
void execShadowMapPass(const phoenix::Shader&, phoenix::Model&);
void execRenderPass(const phoenix::Shader&, phoenix::Model&);
void initPointers();
void deletePointers();

phoenix::Camera* camera;
phoenix::Utils* utils;
phoenix::ShadowCommon* shadowCommon;

float lastX = static_cast<float>(phoenix::SCREEN_WIDTH) / 2.0f;
float lastY = static_cast<float>(phoenix::SCREEN_HEIGHT) / 2.0f;

bool calibratedCursor = false;

unsigned int FBO;
std::array<unsigned int, 1> shadowMaps;
// References to various textures
unsigned int cosinesTexture, sinesTexture;

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(phoenix::SCREEN_WIDTH, phoenix::SCREEN_HEIGHT, "Percentage-Closer Soft Shadows", nullptr, nullptr);
	if (!window)
	{
		std::cerr << phoenix::GLFW_CREATE_WINDOW_ERROR;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cerr << phoenix::GLAD_LOAD_GL_LOADER_ERROR;
		return -1;
	}

	glEnable(GL_DEPTH_TEST);

	initPointers();

	shadowCommon->_floorTexture = utils->loadTexture("../Resources/Textures/shadow_mapping/wood.png");
	shadowCommon->_objectTexture = utils->loadTexture("../Resources/Objects/dragon/albedo.png");
	shadowCommon->_altObjTexture = utils->loadTexture("../Resources/Objects/dragon/blue.png");
	setupRenderToTexture();
	// Generate volume textures and fill them with the cosines and sines of random rotation angles for PCSS
	generateRandom3DTextures();

	phoenix::Shader renderPassShader("../Resources/Shaders/shadow_mapping/render_pass.vs", "../Resources/Shaders/shadow_mapping/render_pass.fs");
	renderPassShader.use();
	renderPassShader.setInt(phoenix::G_DIFFUSE_TEXTURE, 0);
	renderPassShader.setInt(phoenix::G_SHADOW_MAP, 1);
	renderPassShader.setVec3(phoenix::G_LIGHT_COLOR, phoenix::LIGHT_COLOR);
	renderPassShader.setFloat(phoenix::G_AMBIENT_FACTOR, phoenix::AMBIENT_FACTOR);
	renderPassShader.setFloat(phoenix::G_SPECULAR_FACTOR, phoenix::SPECULAR_FACTOR);
	renderPassShader.setInt("gCosinesTexture", 2);
	renderPassShader.setInt("gSinesTexture", 3);
	renderPassShader.setFloat("gCalibratedLightSize", phoenix::CALIBRATED_LIGHT_SIZE);
	phoenix::Shader shadowMapPassShader("../Resources/Shaders/shadow_mapping/shadow_map_pass.vs", "../Resources/Shaders/shadow_mapping/shadow_map_pass.fs");
	phoenix::Shader zBufferShader("../Resources/Shaders/shadow_mapping/z_buffer.vs", "../Resources/Shaders/shadow_mapping/z_buffer.fs");
	zBufferShader.use();
	zBufferShader.setInt(phoenix::G_DEPTH_MAP, 0);
	phoenix::Shader debugLinesShader("../Resources/Shaders/shadow_mapping/debug_lines.vs", "../Resources/Shaders/shadow_mapping/debug_lines.fs");

	phoenix::Model dragon("../Resources/Objects/dragon/dragon.obj");

	while (!glfwWindowShouldClose(window))
	{
		utils->_projection = glm::perspective(glm::radians(camera->_FOV), static_cast<float>(phoenix::SCREEN_WIDTH) / phoenix::SCREEN_HEIGHT, phoenix::PERSPECTIVE_NEAR_PLANE, phoenix::PERSPECTIVE_FAR_PLANE);
		utils->_view = camera->getViewMatrix();

		float currentTime = glfwGetTime();
		shadowCommon->_deltaTime = currentTime - shadowCommon->_lastTimestamp;
		shadowCommon->_lastTimestamp = currentTime;

		shadowCommon->processInput(window, camera, true);

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Shadow map pass
		execShadowMapPass(shadowMapPassShader, dragon);

		glViewport(0, 0, phoenix::SCREEN_WIDTH, phoenix::SCREEN_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Render pass
		if (!shadowCommon->_renderMode || shadowCommon->_renderMode > shadowMaps.size())
		{
			execRenderPass(renderPassShader, dragon);
			shadowCommon->renderDebugLines(debugLinesShader, utils);
		}
		else
		{
			utils->renderQuad(zBufferShader, shadowMaps[shadowCommon->_renderMode - 1]);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	deletePointers();
	glfwTerminate();
	return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void cursorPosCallback(GLFWwindow* window, double x, double y)
{
	if (!calibratedCursor)
	{
		lastX = x;
		lastY = y;
		calibratedCursor = true;
	}

	float xOffset = x - lastX;
	float yOffset = lastY - y; // Reversed for y-coordinates

	lastX = x;
	lastY = y;

	camera->processMouseMovement(xOffset, yOffset);
}

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
	camera->processMouseScroll(yOffset);
}

void generateRandom3DTextures()
{
	std::array<std::array<std::array<float, 32>, 32>, 32> randomCosines;
	std::array<std::array<std::array<float, 32>, 32>, 32> randomSines;
	const int RESOLUTION = 32;
	srand(time(nullptr));
	for (size_t i = 0; i < RESOLUTION; ++i)
	{
		for (size_t j = 0; j < RESOLUTION; ++j)
		{
			for (size_t k = 0; k < RESOLUTION; ++k)
			{
				float randomAngle = static_cast<float>(rand()) / RAND_MAX * glm::pi<float>() * 2;
				randomCosines[i][j][k] = glm::cos(randomAngle) * 0.5 + 0.5;
				randomSines[i][j][k] = glm::sin(randomAngle) * 0.5 + 0.5;
			}
		}
	}

	glGenTextures(1, &cosinesTexture);
	glBindTexture(GL_TEXTURE_3D, cosinesTexture);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, RESOLUTION, RESOLUTION, RESOLUTION, 0, GL_R32F, GL_FLOAT, &randomCosines);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glGenTextures(1, &sinesTexture);
	glBindTexture(GL_TEXTURE_3D, sinesTexture);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, RESOLUTION, RESOLUTION, RESOLUTION, 0, GL_R32F, GL_FLOAT, &randomSines);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

void setupRenderToTexture()
{
	glGenTextures(1, &shadowMaps[0]);
	glBindTexture(GL_TEXTURE_2D, shadowMaps[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, phoenix::SHADOW_MAP_WIDTH, phoenix::SHADOW_MAP_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, phoenix::BORDER_COLOR);

	// Attach the render target to the FBO so we can write to it
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMaps[0], 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void execShadowMapPass(const phoenix::Shader& shader, phoenix::Model& object)
{
	shadowCommon->setLightSpaceVP(shader);
	glViewport(0, 0, phoenix::SHADOW_MAP_WIDTH, phoenix::SHADOW_MAP_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	glClear(GL_DEPTH_BUFFER_BIT);

	shadowCommon->renderScene(utils, shader, object);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void execRenderPass(const phoenix::Shader& shader, phoenix::Model& object)
{
	shadowCommon->setUniforms(shader, camera);
	shadowCommon->setLightSpaceVP(shader);

	shadowCommon->changeColorTexture(shadowCommon->_floorTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowMaps[0]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D, cosinesTexture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_3D, sinesTexture);

	shadowCommon->renderScene(utils, shader, object);
}

void initPointers()
{
	shadowCommon = new phoenix::ShadowCommon(phoenix::LIGHT_POS);
	utils = new phoenix::Utils();
	camera = new phoenix::Camera();
}

void deletePointers()
{
	delete camera;
	delete utils;
	delete shadowCommon;
}