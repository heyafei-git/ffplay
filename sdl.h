//
// Created by heyafei on 2021/6/1.
//

#ifndef BJY_SDL_H
#define BJY_SDL_H

#include "opt.h"
#include "modle.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_RendererInfo renderer_info;
extern SDL_AudioDeviceID audio_dev;

extern int default_width;
extern int default_height;

void sdl_init();

#endif //BJY_SDL_H
