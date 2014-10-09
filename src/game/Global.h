#ifndef GLOBAL_H
#define GLOBAL_H

#include "core/Core.h"

class FontManager;
class ShaderManager;
class TextureManager;
class MeshManager;
class StoneManager;
class InputManager;
class Console;

struct Global
{
    core::TimeBase timeBase;
    
    bool quit = false;

    #ifdef CLIENT

    int displayWidth;
    int displayHeight;

    FontManager * fontManager = nullptr;
    ShaderManager * shaderManager = nullptr;
    TextureManager * textureManager = nullptr;
    MeshManager * meshManager = nullptr;
    InputManager * inputManager = nullptr;
    Console * console = nullptr;

    #endif // #ifdef CLIENT

    StoneManager * stoneManager = nullptr;
};

extern Global global;

#endif // #ifndef GLOBAL_H
