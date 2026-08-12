#include "audio.h"
#ifndef SIN_AUDIO
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>

#define PAUSA_REINICIO_MS  (10 * 60 * 1000)   /* 10 minutos */
#define FADE_IN_REINICIO   5000                /* 5 segundos subiendo el volumen */

static Mix_Music  *musica_actual  = NULL;
static Mix_Chunk  *sfx_btn        = NULL;
static char       path_default[256] = "";  /* ruta de la musica por defecto */
static Uint32     tiempo_fin     = 0;
static int        esperando      = 0;      /* 1 = musica termino, esperando reinicio */
static Uint32     delay_inicio   = 0;      /* tick cuando se programo el delay inicial */
static int        delay_ms       = 0;      /* ms a esperar antes de la primera vez */
static int        esperando_inicio = 0;    /* 1 = aun no arranco por primera vez */

/* Callback: SDL_mixer lo llama cuando la musica llega al final */
static void
musica_termino(void)
{
    if (path_default[0] != '\0') {
        tiempo_fin = SDL_GetTicks();
        esperando  = 1;
    }
}

void
audio_init(void)
{
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "SDL2_mixer error: %s\n", Mix_GetError());
        return;
    }
    Mix_VolumeMusic(80);
    Mix_HookMusicFinished(musica_termino);

    sfx_btn = Mix_LoadWAV("assets/Audio/zipclick.flac");
    if (!sfx_btn)
        fprintf(stderr, "No se pudo cargar sfx_btn: %s\n", Mix_GetError());
}

void
audio_cleanup(void)
{
    Mix_HookMusicFinished(NULL);
    if (sfx_btn) { Mix_FreeChunk(sfx_btn); sfx_btn = NULL; }
    if (musica_actual) {
        Mix_HaltMusic();
        Mix_FreeMusic(musica_actual);
        musica_actual = NULL;
    }
    Mix_CloseAudio();
}

/* Reproduce la musica por defecto UNA sola vez.
   Cuando termina, espera 10 min y vuelve con fade in suave. */
void
audio_play(const char *path, int loops)
{
    if (musica_actual) {
        Mix_HaltMusic();
        Mix_FreeMusic(musica_actual);
        musica_actual = NULL;
    }
    esperando = 0;

    /* guardar la ruta si es la musica principal (loops == -1 o 0) */
    if (path) {
        strncpy(path_default, path, sizeof(path_default) - 1);
        path_default[sizeof(path_default) - 1] = '\0';
    }

    musica_actual = Mix_LoadMUS(path);
    if (!musica_actual) {
        fprintf(stderr, "No se pudo cargar musica '%s': %s\n", path, Mix_GetError());
        return;
    }
    /* loops=0 → reproducir una vez y esperar callback */
    Mix_PlayMusic(musica_actual, loops);
}

void
audio_fade_out(int ms)
{
    esperando = 0;  /* cancelar reinicio pendiente mientras esta en otra pantalla */
    Mix_FadeOutMusic(ms);
}

void
audio_fade_in(const char *path, int ms, int loops)
{
    if (musica_actual) {
        Mix_FreeMusic(musica_actual);
        musica_actual = NULL;
    }
    esperando = 0;

    if (path) {
        strncpy(path_default, path, sizeof(path_default) - 1);
        path_default[sizeof(path_default) - 1] = '\0';
    }

    musica_actual = Mix_LoadMUS(path);
    if (!musica_actual) {
        fprintf(stderr, "No se pudo cargar musica '%s': %s\n", path, Mix_GetError());
        return;
    }
    Mix_FadeInMusic(musica_actual, loops, ms);
}

/* Llamar cada frame desde el game loop principal. */
void
audio_tick(void)
{
    Uint32 ahora = SDL_GetTicks();

    /* ── Primera vez: esperar el delay inicial ── */
    if (esperando_inicio) {
        if (ahora - delay_inicio < (Uint32)delay_ms) return;
        esperando_inicio = 0;
        if (musica_actual) { Mix_FreeMusic(musica_actual); musica_actual = NULL; }
        musica_actual = Mix_LoadMUS(path_default);
        if (!musica_actual) return;
        Mix_VolumeMusic(0);
        Mix_FadeInMusic(musica_actual, 0, FADE_IN_REINICIO);
        return;
    }

    /* ── Reinicio automatico tras 10 min de silencio ── */
    if (!esperando) return;
    if (path_default[0] == '\0') return;
    if (ahora - tiempo_fin < PAUSA_REINICIO_MS) return;

    esperando = 0;
    if (musica_actual) { Mix_FreeMusic(musica_actual); musica_actual = NULL; }
    musica_actual = Mix_LoadMUS(path_default);
    if (!musica_actual) return;
    Mix_VolumeMusic(0);
    Mix_FadeInMusic(musica_actual, 0, FADE_IN_REINICIO);
}

/* Programa la primera reproduccion para despues de `ms` milisegundos.
   No suena nada hasta que audio_tick() detecte que paso el tiempo. */
void
audio_play_delayed(const char *path, int ms)
{
    if (path) {
        strncpy(path_default, path, sizeof(path_default) - 1);
        path_default[sizeof(path_default) - 1] = '\0';
    }
    delay_inicio     = SDL_GetTicks();
    delay_ms         = ms;
    esperando_inicio = 1;
    esperando        = 0;
}

void
audio_sfx_btn(void)
{
    if (sfx_btn)
        Mix_PlayChannel(-1, sfx_btn, 0);
}

void
audio_stop(void)
{
    esperando = 0;
    Mix_HaltMusic();
}

void
audio_pausar(void)
{
    Mix_PauseMusic();
}

void
audio_reanudar(void)
{
    Mix_ResumeMusic();
}

int
audio_reproduciendo(void)
{
    return Mix_PlayingMusic() && !Mix_PausedMusic();
}

void
audio_set_volumen(int vol)
{
    if (vol < 0)   vol = 0;
    if (vol > 100) vol = 100;
    int v = vol * MIX_MAX_VOLUME / 100;
    Mix_VolumeMusic(v);
    Mix_Volume(-1, v);
}

#endif /* SIN_AUDIO */
