#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "progreso.h"

#define MAX_COMPLETADOS 64

static int completados[MAX_COMPLETADOS];
static int n_completados = 0;
static char progreso_path[512];
static int s_intro_vista    = 0;
static int s_tutorial_visto = 0;

int cargar_progreso(const char *path) {
    snprintf(progreso_path, sizeof(progreso_path), "%s", path);
    n_completados = 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0; /* no file yet, fresh start */

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
    if (!root) return 0;

    cJSON *arr = cJSON_GetObjectItem(root, "completados");
    if (cJSON_IsArray(arr)) {
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (n_completados < MAX_COMPLETADOS && cJSON_IsNumber(item))
                completados[n_completados++] = item->valueint;
        }
    }

    cJSON *intro = cJSON_GetObjectItem(root, "intro_vista");
    if (cJSON_IsBool(intro) && cJSON_IsTrue(intro))
        s_intro_vista = 1;

    cJSON *tut = cJSON_GetObjectItem(root, "tutorial_visto");
    if (cJSON_IsBool(tut) && cJSON_IsTrue(tut))
        s_tutorial_visto = 1;

    cJSON_Delete(root);
    return n_completados;
}

int guardar_progreso(const char *path) {
    const char *p = (path && path[0]) ? path : progreso_path;

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateIntArray(completados, n_completados);
    cJSON_AddItemToObject(root, "completados", arr);
    cJSON_AddBoolToObject(root, "intro_vista",    s_intro_vista);
    cJSON_AddBoolToObject(root, "tutorial_visto", s_tutorial_visto);

    /* historial */
    cJSON *hist = cJSON_CreateArray();
    for (int i = 0; i < n_completados; i++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "nivel", completados[i]);
        /* we don't track date per-entry retroactively, use today */
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char fecha[32];
        strftime(fecha, sizeof(fecha), "%Y-%m-%d", tm);
        cJSON_AddStringToObject(entry, "fecha", fecha);
        cJSON_AddItemToArray(hist, entry);
    }
    cJSON_AddItemToObject(root, "historial", hist);

    char *json = cJSON_Print(root);
    cJSON_Delete(root);

    FILE *f = fopen(p, "w");
    if (!f) { free(json); return -1; }
    fputs(json, f);
    fclose(f);
    free(json);
    return 0;
}

int esta_completado(int numero) {
    for (int i = 0; i < n_completados; i++) {
        if (completados[i] == numero) return 1;
    }
    return 0;
}

int marcar_completado(int numero) {
    if (esta_completado(numero)) return 0;
    if (n_completados >= MAX_COMPLETADOS) return -1;
    completados[n_completados++] = numero;
    return guardar_progreso(NULL);
}

int nivel_desbloqueado(int numero) {
    if (numero <= 1) return 1;
    return esta_completado(numero - 1);
}

int total_completados(void) {
    return n_completados;
}

int intro_ya_vista(void)       { return s_intro_vista; }
void marcar_intro_vista(void)  { s_intro_vista = 1; guardar_progreso(NULL); }

int tutorial_ya_visto(void)        { return s_tutorial_visto; }
void marcar_tutorial_visto(void)   { s_tutorial_visto = 1; guardar_progreso(NULL); }

int resetear_progreso(void) {
    n_completados = 0;
    return guardar_progreso(NULL);
}
