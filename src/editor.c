#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "editor.h"
#include "niveles.h"

static const char *base_dir = "soluciones";
static const char *scripts_dir = "scripts";
static const char *data_dir = "data";

static void ruta_solucion(int numero, char *buf, int len) {
    snprintf(buf, len, "%s/nivel_%d.txt", base_dir, numero);
}

void lanzar_editor(int numero) {
    char path[256];
    ruta_solucion(numero, path, sizeof(path));

    /* Create template if file doesn't exist */
    FILE *f = fopen(path, "r");
    if (!f) {
        f = fopen(path, "w");
        if (f) {
            Nivel *n = obtener_nivel(numero);
            fprintf(f, "ACCION %s\n", n ? n->titulo : "sin_titulo");
            fprintf(f, "AMBIENTE\n");
            fprintf(f, "    // Declarar variables aqui\n");
            fprintf(f, "PROCESO\n");
            fprintf(f, "    // Escribir el algoritmo aqui\n");
            fprintf(f, "FIN_ACCION\n");
            fclose(f);
        }
    } else {
        fclose(f);
    }

    const char *editor = getenv("EDITOR");
    if (!editor || editor[0] == '\0') editor = "nano";

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s", editor, path);
    system(cmd);
}

int validar_sintaxis(int numero, char *salida, int salida_len) {
    char path[256];
    ruta_solucion(numero, path, sizeof(path));

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "bash %s/validador.sh %s 2>&1", scripts_dir, path);

    FILE *p = popen(cmd, "r");
    if (!p) {
        snprintf(salida, salida_len, "Error: no se pudo ejecutar el validador");
        return -1;
    }

    salida[0] = '\0';
    int pos = 0;
    char line[256];
    while (fgets(line, sizeof(line), p)) {
        int n = snprintf(salida + pos, salida_len - pos, "%s", line);
        if (n > 0) pos += n;
        if (pos >= salida_len - 1) break;
    }

    int status = pclose(p);
    int ret = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return ret;
}

int ejecutar_prueba(int numero) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "bash %s/verificar.sh %d %s/niveles.json", scripts_dir, numero, data_dir);

    int status = system(cmd);
    int ret = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return ret;
}

char *leer_solucion(int numero) {
    char path[256];
    ruta_solucion(numero, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}
