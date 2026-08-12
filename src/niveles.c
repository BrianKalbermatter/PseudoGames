#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "niveles.h"
#include <stdlib.h>

static Nivel niveles[MAX_NIVELES];
static int num_niveles = 0;

static void copiar_str(char *dst, const cJSON *item, int max) {
    if (cJSON_IsString(item) && item->valuestring)
        snprintf(dst, max, "%s", item->valuestring);
    else
        dst[0] = '\0';
}

int cargar_niveles(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return -1;

    num_niveles = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        if (num_niveles >= MAX_NIVELES) break;
        Nivel *n = &niveles[num_niveles];
        memset(n, 0, sizeof(Nivel));

        cJSON *num = cJSON_GetObjectItem(item, "numero");
        if (cJSON_IsNumber(num)) n->numero = num->valueint;

        copiar_str(n->dificultad, cJSON_GetObjectItem(item, "dificultad"), sizeof(n->dificultad));
        copiar_str(n->titulo, cJSON_GetObjectItem(item, "titulo"), sizeof(n->titulo));
        copiar_str(n->enunciado, cJSON_GetObjectItem(item, "enunciado"), sizeof(n->enunciado));
        copiar_str(n->pista, cJSON_GetObjectItem(item, "pista"), sizeof(n->pista));

        cJSON *casos = cJSON_GetObjectItem(item, "casos_prueba");
        n->num_casos = 0;
        if (cJSON_IsArray(casos)) {
            cJSON *c;
            cJSON_ArrayForEach(c, casos) {
                if (n->num_casos >= MAX_CASOS) break;
                CasoPrueba *cp = &n->casos[n->num_casos];
                copiar_str(cp->datos,       cJSON_GetObjectItem(c, "datos"),       sizeof(cp->datos));
                copiar_str(cp->tipo_salida, cJSON_GetObjectItem(c, "tipo_salida"), sizeof(cp->tipo_salida));
                n->num_casos++;
            }
        }
        num_niveles++;
    }

    cJSON_Delete(root);
    return num_niveles;
}

Nivel *obtener_nivel(int numero) {
    for (int i = 0; i < num_niveles; i++) {
        if (niveles[i].numero == numero)
            return &niveles[i];
    }
    return NULL;
}

int total_niveles(void) {
    return num_niveles;
}
