CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT("hack")

SET(CMAKE_CXX_FLAGS "-std=c++2a -m32 -ldl -lpthread -Wall -fpic -O2")

FILE(GLOB SOURCES "src/*.cpp")

ADD_SUBDIRECTORY(libraries/fmt) # fmt::format(format string, args...) -> std::format(format string, args...)
ADD_LIBRARY(${CMAKE_PROJECT_NAME} SHARED ${SOURCES})
TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} fmt)
