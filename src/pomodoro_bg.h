#ifndef POMODORO_BG_H
#define POMODORO_BG_H

#define POM_ROWS 50
#define POM_COLS 120

// Grilla VT accesible desde screenPomodoro para renderizar
extern char pom_grid[POM_ROWS][POM_COLS];
extern int  pom_row, pom_col;

void pom_init(void);             // lanzar el proceso en fondo
void pom_cleanup(void);          // terminar el proceso al salir del juego
void pom_send(const char *s, int n);  // enviar tecla al script
void pom_tick(void);             // leer output pendiente (llamar cada frame)

#endif
