CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT("hack")

SET(CMAKE_CXX_FLAGS "-std=c++2a -m32 -lpthread -Wall -fpic -O2 -ggdb --no-gnu-unique")
SET(CMAKE_C_FLAGS "-m32 -lpthread -Wall -fpic -O2 --no-gnu-unique")
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

FILE(GLOB SOURCES "src/*.cpp"
	"src/ImGui/*cpp"
	"src/ImGui/imgui_draw.cpp"
	"src/ImGui/examples/*.cpp"
	"src/ImGui/examples/libs/gl3w/GL/*.c")

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src/ImGui/")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src/ImGui/examples/")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src/ImGui/examples/libs/")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src/ImGui/examples/libs/gl3w/")

ADD_SUBDIRECTORY(libraries/fmt) # fmt::format(format string, args...) -> std::format(format string, args...)
ADD_LIBRARY(${CMAKE_PROJECT_NAME} SHARED ${SOURCES})
TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} fmt dl SDL2)
