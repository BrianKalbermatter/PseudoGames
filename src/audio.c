#include "audio.h"
#ifndef SIN_AUDIO
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdio.h>
#include <string.h>

// ============================================================
// SDL3_mixer NO es un rename de SDL2_mixer: es una API nueva.
//
// SDL2_mixer tenia UN canal de musica global y N canales de efectos,
// y todas las funciones eran globales (Mix_PlayMusic, Mix_HaltMusic).
//
// SDL3_mixer usa tres objetos explicitos:
//   MIX_Mixer  el dispositivo de audio abierto
//   MIX_Audio  un sonido cargado en memoria (el archivo)
//   MIX_Track  una "boca" que reproduce un MIX_Audio
//
// Un Track es lo que en SDL2 era "el canal". Aca tenemos dos:
// uno para la musica y otro para el click. Ya no hay estado global
// escondido: cada operacion dice sobre QUE track actua.
//
// La interfaz de audio.h no cambio ni una linea: los 15 archivos que
// llaman audio_play() no se enteraron de nada.
// ============================================================

#define PAUSA_REINICIO_MS  (10 * 60 * 1000)   /* 10 minutos */
#define FADE_IN_REINICIO   5000                /* 5 segundos subiendo el volumen */

static MIX_Mixer  *mixer         = NULL;
static MIX_Track  *track_musica  = NULL;
static MIX_Track  *track_sfx     = NULL;
static MIX_Audio  *musica_actual = NULL;
static MIX_Audio  *sfx_btn       = NULL;

static char       path_default[256] = "";  /* ruta de la musica por defecto */
static Uint64     tiempo_fin     = 0;
static int        esperando      = 0;      /* 1 = musica termino, esperando reinicio */
static Uint64     delay_inicio   = 0;      /* tick cuando se programo el delay inicial */
static int        delay_ms       = 0;      /* ms a esperar antes de la primera vez */
static int        esperando_inicio = 0;    /* 1 = aun no arranco por primera vez */
static float      gain_musica    = 0.8f;   /* SDL3 usa ganancia 0.0-1.0, no 0-128 */

/* Callback: el mixer lo llama cuando el track deja de sonar.
   OJO: corre en el hilo de audio, no en el del juego. Por eso aca solo
   se anotan dos variables simples y el trabajo real lo hace audio_tick(). */
static void SDLCALL
musica_termino(void *userdata, MIX_Track *track)
{
    (void)userdata; (void)track;
    if (path_default[0] != '\0') {
        tiempo_fin = SDL_GetTicks();
        esperando  = 1;
    }
}

/* Arranca el track de musica con loops y un fade in opcional.
   En SDL3 los parametros de reproduccion viajan en un SDL_PropertiesID
   en vez de multiplicarse en funciones (PlayMusic, FadeInMusic,
   FadeInMusicPos...). Una sola funcion, opciones con nombre. */
static void
reproducir(int loops, int fade_ms)
{
    SDL_PropertiesID op = SDL_CreateProperties();
    if (op == 0) return;

    SDL_SetNumberProperty(op, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    if (fade_ms > 0)
        SDL_SetNumberProperty(op, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, fade_ms);

    MIX_PlayTrack(track_musica, op);
    SDL_DestroyProperties(op);
}

void
audio_init(void)
{
    if (!MIX_Init()) {
        fprintf(stderr, "SDL3_mixer error: %s\n", SDL_GetError());
        return;
    }

    /* NULL = que el mixer elija el formato del dispositivo por defecto.
       En SDL2 haciamos Mix_OpenAudio(44100, FORMAT, 2, 2048) a mano. */
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!mixer) {
        fprintf(stderr, "No se pudo abrir el mixer: %s\n", SDL_GetError());
        return;
    }

    track_musica = MIX_CreateTrack(mixer);
    track_sfx    = MIX_CreateTrack(mixer);
    if (!track_musica || !track_sfx) {
        fprintf(stderr, "No se pudieron crear los tracks: %s\n", SDL_GetError());
        return;
    }

    MIX_SetTrackGain(track_musica, gain_musica);
    MIX_SetTrackStoppedCallback(track_musica, musica_termino, NULL);

    /* predecode=true: el click es corto y suena muchas veces, conviene
       tenerlo ya decodificado en RAM en vez de decodificar en cada click. */
    sfx_btn = MIX_LoadAudio(mixer, "assets/Audio/zipclick.flac", true);
    if (!sfx_btn)
        fprintf(stderr, "No se pudo cargar sfx_btn: %s\n", SDL_GetError());
}

void
audio_cleanup(void)
{
    if (track_musica) {
        MIX_SetTrackStoppedCallback(track_musica, NULL, NULL);
        MIX_StopTrack(track_musica, 0);
        MIX_DestroyTrack(track_musica);
        track_musica = NULL;
    }
    if (track_sfx)     { MIX_DestroyTrack(track_sfx);     track_sfx = NULL; }
    if (sfx_btn)       { MIX_DestroyAudio(sfx_btn);       sfx_btn = NULL; }
    if (musica_actual) { MIX_DestroyAudio(musica_actual); musica_actual = NULL; }
    if (mixer)         { MIX_DestroyMixer(mixer);         mixer = NULL; }
    MIX_Quit();
}

/* Carga `path` en el track de musica. Devuelve 1 si quedo listo.
   Centraliza el patron "parar, liberar la anterior, cargar la nueva",
   que en la version SDL2 estaba repetido en cuatro funciones. */
static int
cargar_musica(const char *path)
{
    if (!mixer || !track_musica || !path) return 0;

    MIX_StopTrack(track_musica, 0);
    if (musica_actual) { MIX_DestroyAudio(musica_actual); musica_actual = NULL; }

    /* predecode=false: la musica es larga, se decodifica mientras suena. */
    musica_actual = MIX_LoadAudio(mixer, path, false);
    if (!musica_actual) {
        fprintf(stderr, "No se pudo cargar musica '%s': %s\n", path, SDL_GetError());
        return 0;
    }
    if (!MIX_SetTrackAudio(track_musica, musica_actual)) {
        fprintf(stderr, "No se pudo montar la musica: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

static void
recordar_path(const char *path)
{
    if (!path) return;
    strncpy(path_default, path, sizeof(path_default) - 1);
    path_default[sizeof(path_default) - 1] = '\0';
}

/* Reproduce la musica por defecto UNA sola vez.
   Cuando termina, espera 10 min y vuelve con fade in suave. */
void
audio_play(const char *path, int loops)
{
    esperando = 0;
    recordar_path(path);
    if (!cargar_musica(path)) return;

    MIX_SetTrackGain(track_musica, gain_musica);
    reproducir(loops, 0);
}

void
audio_fade_out(int ms)
{
    esperando = 0;  /* cancelar reinicio pendiente mientras esta en otra pantalla */
    if (!track_musica) return;
    /* SDL3 mide el fade en FRAMES de audio, no en milisegundos: cuantas
       muestras dura el desvanecido. MIX_TrackMSToFrames hace la cuenta
       con la frecuencia real del dispositivo abierto. */
    MIX_StopTrack(track_musica, MIX_TrackMSToFrames(track_musica, ms));
}

void
audio_fade_in(const char *path, int ms, int loops)
{
    esperando = 0;
    recordar_path(path);
    if (!cargar_musica(path)) return;

    MIX_SetTrackGain(track_musica, gain_musica);
    reproducir(loops, ms);
}

/* Llamar cada frame desde el game loop principal. */
void
audio_tick(void)
{
    Uint64 ahora = SDL_GetTicks();

    /* ── Primera vez: esperar el delay inicial ── */
    if (esperando_inicio) {
        if (ahora - delay_inicio < (Uint64)delay_ms) return;
        esperando_inicio = 0;
        if (!cargar_musica(path_default)) return;
        reproducir(0, FADE_IN_REINICIO);
        return;
    }

    /* ── Reinicio automatico tras 10 min de silencio ── */
    if (!esperando) return;
    if (path_default[0] == '\0') return;
    if (ahora - tiempo_fin < PAUSA_REINICIO_MS) return;

    esperando = 0;
    if (!cargar_musica(path_default)) return;
    reproducir(0, FADE_IN_REINICIO);
}

/* Programa la primera reproduccion para despues de `ms` milisegundos.
   No suena nada hasta que audio_tick() detecte que paso el tiempo. */
void
audio_play_delayed(const char *path, int ms)
{
    recordar_path(path);
    delay_inicio     = SDL_GetTicks();
    delay_ms         = ms;
    esperando_inicio = 1;
    esperando        = 0;
}

void
audio_sfx_btn(void)
{
    if (!track_sfx || !sfx_btn) return;
    /* Montar y disparar: el click es corto, no necesita loops ni fade. */
    if (MIX_SetTrackAudio(track_sfx, sfx_btn))
        MIX_PlayTrack(track_sfx, 0);
}

void
audio_stop(void)
{
    esperando = 0;
    if (track_musica) MIX_StopTrack(track_musica, 0);
}

void
audio_pausar(void)
{
    if (track_musica) MIX_PauseTrack(track_musica);
}

void
audio_reanudar(void)
{
    if (track_musica) MIX_ResumeTrack(track_musica);
}

int
audio_reproduciendo(void)
{
    if (!track_musica) return 0;
    return MIX_TrackPlaying(track_musica) && !MIX_TrackPaused(track_musica);
}

void
audio_set_volumen(int vol)
{
    if (vol < 0)   vol = 0;
    if (vol > 100) vol = 100;
    /* SDL2 usaba 0..MIX_MAX_VOLUME (128) en enteros.
       SDL3 usa ganancia en float: 0.0 = silencio, 1.0 = volumen original. */
    gain_musica = (float)vol / 100.0f;
    if (track_musica) MIX_SetTrackGain(track_musica, gain_musica);
    if (track_sfx)    MIX_SetTrackGain(track_sfx,    gain_musica);
}

#endif /* SIN_AUDIO */
