#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <iostream>
#include <stdexcept>
#include <vector>

#include "VulkanRenderer.h"

GLFWwindow* initWindow(std::string name = "Vulkan Window", int width = 800, int height = 600);

int main() 
{
	GLFWwindow* window = initWindow();
	VulkanRenderer vkRenderer{};

	if (vkRenderer.init(window) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	};

	// main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		vkRenderer.draw();
	}

	// Clean after ourselves
	vkRenderer.cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}

GLFWwindow* initWindow(std::string name, int width, int height)
{
	// Init GLFW
	glfwInit();

	// Declare that we don't use openGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Make the window unresizable for now to make things simpler
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	return glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
}