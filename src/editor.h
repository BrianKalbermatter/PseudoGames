#ifndef EDITOR_H
#define EDITOR_H

void lanzar_editor(int numero);
int validar_sintaxis(int numero, char *salida, int salida_len);
int ejecutar_prueba(int numero);
char *leer_solucion(int numero);

#endif
