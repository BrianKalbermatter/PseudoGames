#ifndef PROGRESO_H
#define PROGRESO_H

int cargar_progreso(const char *path);
int guardar_progreso(const char *path);
int esta_completado(int numero);
int marcar_completado(int numero);
int nivel_desbloqueado(int numero);
int total_completados(void);
int resetear_progreso(void);

int intro_ya_vista(void);
void marcar_intro_vista(void);

int tutorial_ya_visto(void);
void marcar_tutorial_visto(void);

#endif
