cmake_minimum_required(VERSION 3.10)

project(vkapp)

list(APPEND CMAKE_PREFIX_PATH "C:/src/vcpkg/installed/x64-windows/share/glfw3")
list(APPEND CMAKE_PREFIX_PATH "C:/src/vcpkg/installed/x64-windows/share/glm")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Specify C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
	main.cpp
	Mesh.cpp
	Utils.h
	VulkanRenderer.cpp
	VulkanRenderer.h)

target_include_directories(${PROJECT_NAME} PRIVATE
		${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE glfw ${Vulkan_LIBRARIES})