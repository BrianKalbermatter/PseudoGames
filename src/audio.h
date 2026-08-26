#ifndef AUDIO_H
#define AUDIO_H

#ifdef SIN_AUDIO
/* Stubs vacios para builds sin SDL3_mixer (ej: Windows) */
static inline void audio_init(void)             {}
static inline void audio_sfx_btn(void)          {}
static inline void audio_cleanup(void)          {}
static inline void audio_play(const char *p, int l) { (void)p; (void)l; }
static inline void audio_play_delayed(const char *p, int ms) { (void)p; (void)ms; }
static inline void audio_fade_out(int ms)       { (void)ms; }
static inline void audio_fade_in(const char *p, int ms, int l) { (void)p; (void)ms; (void)l; }
static inline void audio_stop(void)             {}
static inline void audio_pausar(void)           {}
static inline void audio_reanudar(void)         {}
static inline int  audio_reproduciendo(void)    { return 0; }
static inline void audio_tick(void)             {}
static inline void audio_set_volumen(int v)     { (void)v; }
#else

/* Inicializa SDL3_mixer. Llamar una sola vez en main(). */
void audio_init(void);

/* Reproduce el efecto de sonido de boton. */
void audio_sfx_btn(void);

/* Libera recursos. Llamar al cerrar el juego. */
void audio_cleanup(void);

/* Reproduce musica en loop (loops=-1 = infinito). */
void audio_play(const char *path, int loops);

/* Programa la primera reproduccion para despues de `ms` milisegundos.
   Util para no arrancar la musica inmediatamente al inicio. */
void audio_play_delayed(const char *path, int ms);

/* Fade out de la musica actual en `ms` milisegundos. */
void audio_fade_out(int ms);

/* Fade in de una nueva musica en `ms` milisegundos, en loop. */
void audio_fade_in(const char *path, int ms, int loops);

/* Parar la musica inmediatamente. */
void audio_stop(void);

/* Pausa / reanuda sin perder la posicion. */
void audio_pausar(void);
void audio_reanudar(void);

/* Devuelve 1 si hay musica reproduciendose (no pausada, no parada). */
int audio_reproduciendo(void);

/* Llamar cada frame desde el game loop. Gestiona el reinicio automatico
   despues de 10 min de silencio cuando la musica por defecto termina. */
void audio_tick(void);

/* Cambia el volumen global (0-100). */
void audio_set_volumen(int vol);

#endif /* SIN_AUDIO */
#endif /* AUDIO_H */
