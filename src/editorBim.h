#ifndef EDITOR_BIM_H
#define EDITOR_BIM_H

#include "shell.h"

void editor_bim_init        (ShellCtx *ctx, Tab *tab);
void editor_bim_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e);
void editor_bim_draw        (ShellCtx *ctx, Tab *tab, SDL_Rect area);
void editor_bim_cleanup     (ShellCtx *ctx, Tab *tab);

#endif
