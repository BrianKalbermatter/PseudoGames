#include "screenVerificador.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TMP_PAED  "/tmp/sol_verificar.paed"

/* El interprete de PAED, INSTALADO en el sistema.
 *
 * Antes esto era "./Frankly/paed": el interprete escrito en bash, buscado por
 * una ruta relativa a esta carpeta. Dejo de funcionar cuando PAED paso a ser un
 * proyecto aparte y Frankly/ se fue con el. Ahora se llama al binario, que se
 * resuelve por PATH:
 *
 *     cd ../paed && make install PREFIX=$HOME/.local
 *
 * Si no esta instalado, la shell responde "command not found" con codigo 127 y
 * la verificacion da NO SUPERADO — no se cuelga.
 *
 * El "< /dev/null" NO es decoracion. El interprete en C ejecuta LEER de verdad,
 * y LEER lee de stdin. Sin esta redireccion, una solucion con LEER dejaria al
 * interprete esperando que alguien tipee... en el stdin de una ventana SDL, que
 * nadie esta mirando: el editor se colgaria sin decir por que. Con /dev/null la
 * entrada llega vacia y LEER falla al instante con "la entrada se termino".
 *
 * El "2>&1" queda: los errores del interprete son parte de lo que se juzga, y
 * ademas su codigo de salida distinto de cero ya marca el caso como fallado.
 */
#define PAED_CMD  "paed " TMP_PAED " < /dev/null 2>&1"

/* Parsea "C=100, R=0.04" y genera lineas "    C := 100;\n" */
static void
generar_inyeccion(const char *datos, char *out, int out_max)
{
    out[0] = '\0';
    char copia[256];
    strncpy(copia, datos, sizeof(copia)-1);
    copia[sizeof(copia)-1] = '\0';

    char *tok = strtok(copia, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            char var[64], val[64];
            strncpy(var, tok, sizeof(var)-1); var[sizeof(var)-1] = '\0';
            strncpy(val, eq+1, sizeof(val)-1); val[sizeof(val)-1] = '\0';
            int vl = (int)strlen(var)-1;
            while (vl >= 0 && var[vl] == ' ') var[vl--] = '\0';
            char linea[128];
            snprintf(linea, sizeof(linea), "    %s := %s;\n", var, val);
            strncat(out, linea, out_max - (int)strlen(out) - 1);
        }
        tok = strtok(NULL, ",");
    }
}

/* Crea /tmp/sol_verificar.paed inyectando las vars despues de PROCESO */
static int
preparar_archivo(const char *path_alumno, const char *datos)
{
    FILE *src = fopen(path_alumno, "r");
    if (!src) return 0;

    FILE *dst = fopen(TMP_PAED, "w");
    if (!dst) { fclose(src); return 0; }

    char inyeccion[1024];
    generar_inyeccion(datos, inyeccion, sizeof(inyeccion));

    char linea[512];
    while (fgets(linea, sizeof(linea), src)) {
        fputs(linea, dst);
        char *p = linea;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "PROCESO", 7) == 0)
            fputs(inyeccion, dst);
    }
    fclose(src);
    fclose(dst);
    return 1;
}

static int es_numero(const char *s) {
    if (!s || !*s) return 0;
    char *end;
    strtod(s, &end);
    while (*end == ' ' || *end == '\n' || *end == '\r') end++;
    return *end == '\0';
}

static int es_entero(const char *s) {
    if (!es_numero(s)) return 0;
    return strchr(s, '.') == NULL;
}

int
verificar_nivel(Nivel *n, const char *nombre_nivel)
{
    if (!n || n->num_casos == 0) return 0;

    char path_alumno[128];
    snprintf(path_alumno, sizeof(path_alumno), "saves/%s.paed", nombre_nivel);

    for (int i = 0; i < n->num_casos; i++) {
        CasoPrueba *cp = &n->casos[i];

        if (!preparar_archivo(path_alumno, cp->datos))
            return 0;

        FILE *p = popen(PAED_CMD, "r");
        if (!p) return 0;

        char output_buf[1024] = {0};
        int n_leido = (int)fread(output_buf, 1, sizeof(output_buf)-1, p);
        output_buf[n_leido] = '\0';
        int exit_code = pclose(p);
        if (exit_code != 0) return 0;

        /* Tomar la ultima linea como resultado */
        char last[256] = {0};
        char tmp[1024];
        strncpy(tmp, output_buf, sizeof(tmp)-1);
        char *line = strtok(tmp, "\n");
        while (line) { strncpy(last, line, sizeof(last)-1); line = strtok(NULL, "\n"); }

        int ll = (int)strlen(last)-1;
        while (ll >= 0 && (last[ll]=='\n'||last[ll]=='\r'||last[ll]==' ')) last[ll--] = '\0';

        int tipo_ok = 0;
        if      (strcmp(cp->tipo_salida, "real")   == 0) tipo_ok = es_numero(last);
        else if (strcmp(cp->tipo_salida, "entero")  == 0) tipo_ok = es_entero(last);
        else                                               tipo_ok = (last[0] != '\0');

        if (!tipo_ok) return 0;
    }
    return 1;
}
