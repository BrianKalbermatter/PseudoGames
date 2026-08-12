#ifndef SCREEN_C_EDITOR_H
#define SCREEN_C_EDITOR_H

#include "shell.h"

void ceditor_init        (ShellCtx *ctx, Tab *tab);
void ceditor_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e);
void ceditor_draw        (ShellCtx *ctx, Tab *tab, SDL_Rect area);
void ceditor_cleanup     (ShellCtx *ctx, Tab *tab);

#endif
