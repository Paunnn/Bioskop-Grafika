#ifndef UTIL_H
#define UTIL_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

unsigned int createShader(const char* vsSource, const char* fsSource);


GLFWcursor* loadImageToCursor(const char* filePath);


unsigned int loadTexture(const char* filePath);

#endif