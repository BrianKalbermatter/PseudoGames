#ifndef SCREEN_VERIFICADOR_H
#define SCREEN_VERIFICADOR_H

#include "niveles.h"

/* Corre todos los casos de prueba del nivel de forma silenciosa.
   Retorna 1 si todos los casos pasan, 0 si alguno falla. */
int verificar_nivel(Nivel *n, const char *nombre_nivel);

#endif
