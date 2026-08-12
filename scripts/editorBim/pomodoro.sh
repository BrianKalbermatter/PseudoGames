#!/usr/bin/env bash

WORK_TIME=$(( 25 * 60 ))
SHORT_BREAK=$(( 5 * 60 ))
LONG_BREAK=$(( 15 * 60 ))

R='\033[0m'
BOLD='\033[1m'
DIM='\033[2m'
RED='\033[1;31m'
GRN='\033[1;32m'
YLW='\033[1;33m'
BLU='\033[1;34m'
MGT='\033[1;35m'
CYN='\033[1;36m'
WHT='\033[1;37m'
BRED='\033[41;1;37m'
BGRN='\033[42;1;37m'

PAUSED=0
QUIT=0
ALARM_SET=0
ALARM_TIME=""
POMODORO_COUNT=0
SAVED_STTY=""
MUSIC_PID=0
MUSIC_TITLE=""

init_digits() {
    D_0_0=" ███ "; D_0_1="█   █"; D_0_2="█   █"; D_0_3="█   █"; D_0_4=" ███ "
    D_1_0="  █  "; D_1_1=" ██  "; D_1_2="  █  "; D_1_3="  █  "; D_1_4=" ███ "
    D_2_0=" ███ "; D_2_1="    █"; D_2_2=" ███ "; D_2_3="█    "; D_2_4="█████"
    D_3_0=" ███ "; D_3_1="    █"; D_3_2="  ██ "; D_3_3="    █"; D_3_4=" ███ "
    D_4_0="█   █"; D_4_1="█   █"; D_4_2="█████"; D_4_3="    █"; D_4_4="    █"
    D_5_0="█████"; D_5_1="█    "; D_5_2="████ "; D_5_3="    █"; D_5_4="████ "
    D_6_0=" ███ "; D_6_1="█    "; D_6_2="████ "; D_6_3="█   █"; D_6_4=" ███ "
    D_7_0="█████"; D_7_1="    █"; D_7_2="   █ "; D_7_3="  █  "; D_7_4="  █  "
    D_8_0=" ███ "; D_8_1="█   █"; D_8_2=" ███ "; D_8_3="█   █"; D_8_4=" ███ "
    D_9_0=" ███ "; D_9_1="█   █"; D_9_2=" ████"; D_9_3="    █"; D_9_4=" ███ "
    D_c_0="     "; D_c_1="  █  "; D_c_2="     "; D_c_3="  █  "; D_c_4="     "
    D_s_0="     "; D_s_1="     "; D_s_2="     "; D_s_3="     "; D_s_4="     "
}

get_digit_row() {
    local char="$1"
    local row="$2"
    local key
    case "$char" in
        ':') key="c" ;;
        ' ') key="s" ;;
        *)   key="$char" ;;
    esac
    local varname="D_${key}_${row}"
    echo "${!varname}"
}

draw_big_text() {
    local text="$1"
    local color="${2:-$CYN}"
    local row
    for (( row=0; row<5; row++ )); do
        local line=""
        local i
        for (( i=0; i<${#text}; i++ )); do
            local char="${text:$i:1}"
            line+="$(get_digit_row "$char" "$row") "
        done
        printf "${color}%s${R}\n" "$line"
    done
}

term_width()  { tput cols  2>/dev/null || echo 80; }
term_height() { tput lines 2>/dev/null || echo 24; }

clrscr() {
    tput clear 2>/dev/null || printf '\033[2J\033[H'
}

# Mover cursor al inicio SIN limpiar pantalla.
# Se usa dentro de los loops para evitar el parpadeo:
# en lugar de borrar+redibujar, sobreescribimos el contenido anterior.
go_home() { printf '\033[H'; }

# Borrar desde el cursor hasta el final de la pantalla.
# Se llama al terminar de dibujar un frame para limpiar
# cualquier residuo de frames anteriores (si el contenido se acortó).
frame_end() { printf '\033[J'; }

goto() {
    tput cup "$1" "$2" 2>/dev/null || printf "\033[%d;%dH" $(($1+1)) $(($2+1))
}

hide_cursor() { tput civis 2>/dev/null || printf '\033[?25l'; }
show_cursor() { tput cnorm 2>/dev/null || printf '\033[?25h'; }

center_pad() {
    local text="$1"
    local clean
    clean=$(printf "%s" "$text" | sed 's/\x1b\[[0-9;]*m//g')
    local w
    w=$(term_width)
    local pad=$(( (w - ${#clean}) / 2 ))
    [[ $pad -lt 0 ]] && pad=0
    printf "%${pad}s" ""
}

draw_hline() {
    local char="${1:-═}"
    local w
    w=$(term_width)
    printf '%*s\n' "$w" '' | tr ' ' "$char"
}

seconds_to_hms() {
    local t=$1
    printf "%02d:%02d:%02d" $(( t/3600 )) $(( (t%3600)/60 )) $(( t%60 ))
}

seconds_to_ms() {
    local t=$1
    printf "%02d:%02d" $(( t/60 )) $(( t%60 ))
}

now_time() { date "+%H:%M:%S"; }
now_hm()   { date "+%H:%M"; }
now_date() { date "+%A %d/%m/%Y"; }

draw_progress() {
    local cur=$1 total=$2 width=$3
    local color="${4:-$GRN}"
    local filled=$(( cur * width / total ))
    [[ $filled -gt $width ]] && filled=$width
    local empty=$(( width - filled ))
    printf "${color}["
    local i
    for (( i=0; i<filled; i++ )); do printf "█"; done
    for (( i=0; i<empty;  i++ )); do printf "░"; done
    printf "] %d%%${R}" $(( cur * 100 / total ))
}

stop_music() {
    if [[ $MUSIC_PID -ne 0 ]] && kill -0 "$MUSIC_PID" 2>/dev/null; then
        kill "$MUSIC_PID" 2>/dev/null
        wait "$MUSIC_PID" 2>/dev/null
    fi
    MUSIC_PID=0
    MUSIC_TITLE=""
}

cleanup() {
    stop_music
    show_cursor
    [[ -n "$SAVED_STTY" ]] && stty "$SAVED_STTY" 2>/dev/null
    tput cup "$(term_height)" 0 2>/dev/null
    printf "${R}\n"
    echo "Pomodoros completados: $POMODORO_COUNT"
    exit 0
}

trap cleanup INT TERM EXIT

draw_short_header() {
    local mode="${1:-}"
    local w
    w=$(term_width)
    printf "${MGT}"
    draw_hline "═"
    local title="  🍅 POMODORO ASCII"
    [[ -n "$mode" ]] && title+="  │  ${mode}"
    title+="  │  $(now_time)"
    printf "%s\n" "$title"
    draw_hline "═"
    printf "${R}"
}

draw_menu() {
    clrscr
    local w
    w=$(term_width)
    printf "${MGT}"
    draw_hline "═"
    printf "${R}"
    echo ""
    printf "${RED}"
    center_pad "██████╗  ██████╗ ███╗   ███╗ ██████╗ ██████╗  ██████╗ ██████╗  ██████╗"
    echo "██████╗  ██████╗ ███╗   ███╗ ██████╗ ██████╗  ██████╗ ██████╗  ██████╗"
    center_pad "██╔══██╗██╔═══██╗████╗ ████║██╔═══██╗██╔══██╗██╔═══██╗██╔══██╗██╔═══██╗"
    echo "██╔══██╗██╔═══██╗████╗ ████║██╔═══██╗██╔══██╗██╔═══██╗██╔══██╗██╔═══██╗"
    center_pad "██████╔╝██║   ██║██╔████╔██║██║   ██║██║  ██║██║   ██║██████╔╝██║   ██║"
    echo "██████╔╝██║   ██║██╔████╔██║██║   ██║██║  ██║██║   ██║██████╔╝██║   ██║"
    center_pad "██╔═══╝ ██║   ██║██║╚██╔╝██║██║   ██║██║  ██║██║   ██║██╔══██╗██║   ██║"
    echo "██╔═══╝ ██║   ██║██║╚██╔╝██║██║   ██║██║  ██║██║   ██║██╔══██╗██║   ██║"
    center_pad "██║     ╚██████╔╝██║ ╚═╝ ██║╚██████╔╝██████╔╝╚██████╔╝██║  ██║╚██████╔╝"
    echo "██║     ╚██████╔╝██║ ╚═╝ ██║╚██████╔╝██████╔╝╚██████╔╝██║  ██║╚██████╔╝"
    center_pad "╚═╝      ╚═════╝ ╚═╝     ╚═╝ ╚═════╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝ ╚═════╝"
    echo "╚═╝      ╚═════╝ ╚═╝     ╚═╝ ╚═════╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝ ╚═════╝"
    printf "${R}"
    echo ""
    printf "${MGT}"; draw_hline "═"; printf "${R}"
    echo ""
    printf "  ${YLW}%s  %s${R}\n" "$(now_date)" "$(now_time)"
    echo ""

    local pad="    "
    printf "${WHT}${pad}╔══════════════════════════════════════╗${R}\n"
    printf "${WHT}${pad}║         MENU  PRINCIPAL              ║${R}\n"
    printf "${WHT}${pad}╠══════════════════════════════════════╣${R}\n"
    printf "${WHT}${pad}║  ${GRN}[1]${WHT}  🍅  Pomodoro (configurable)    ║${R}\n"
    printf "${WHT}${pad}║  ${GRN}[2]${WHT}  ⏱   Cronometro               ║${R}\n"
    printf "${WHT}${pad}║  ${GRN}[3]${WHT}  ⏳  Temporizador              ║${R}\n"
    printf "${WHT}${pad}║  ${GRN}[4]${WHT}  ⏰  Alarma                    ║${R}\n"
    printf "${WHT}${pad}║  ${GRN}[5]${WHT}  📊  Estadisticas              ║${R}\n"
    printf "${WHT}${pad}║  ${GRN}[9]${WHT}  🎵  YouTube Music             ║${R}\n"
    if [[ $MUSIC_PID -ne 0 ]] && kill -0 "$MUSIC_PID" 2>/dev/null; then
        printf "${WHT}${pad}║  ${RED}[0]${WHT}  ⏹   Detener musica           ║${R}\n"
    fi
    printf "${WHT}${pad}║  ${RED}[q]${WHT}      Salir                     ║${R}\n"
    printf "${WHT}${pad}╚══════════════════════════════════════╝${R}\n"
    echo ""
    printf "  ${DIM}Pomodoros hoy: ${BOLD}${YLW}${POMODORO_COUNT}${R}\n"
    if [[ $MUSIC_PID -ne 0 ]] && kill -0 "$MUSIC_PID" 2>/dev/null; then
        printf "  ${GRN}♪ Sonando: ${YLW}%s${R}\n" "$MUSIC_TITLE"
    fi
    echo ""
    printf "  ${WHT}Opcion: ${R}"
}

show_timer_screen() {
    local remaining=$1
    local total=$2
    local label="${3:-TIMER}"
    local color="${4:-$CYN}"

    go_home
    draw_short_header "$label"
    echo ""

    local time_str
    time_str=$(seconds_to_ms "$remaining")

    local w
    w=$(term_width)
    local big_w=36
    local pad=$(( (w - big_w) / 2 ))
    [[ $pad -lt 0 ]] && pad=0

    local row
    for (( row=0; row<5; row++ )); do
        printf "%${pad}s" ""
        local i
        for (( i=0; i<${#time_str}; i++ )); do
            local char="${time_str:$i:1}"
            printf "${color}%s ${R}" "$(get_digit_row "$char" "$row")"
        done
        printf "\n"
    done

    echo ""

    local elapsed=$(( total - remaining ))
    local bar_w=$(( w - 16 ))
    [[ $bar_w -lt 10 ]] && bar_w=10
    printf "  "
    draw_progress "$elapsed" "$total" "$bar_w" "$color"
    echo ""
    echo ""

    if [[ $PAUSED -eq 1 ]]; then
        printf "  ${YLW}${BOLD}⏸  PAUSADO${R}   ${DIM}%s${R}\n" "$label"
    else
        printf "  ${GRN}${BOLD}▶  CORRIENDO${R}  ${DIM}%s${R}\n" "$label"
    fi

    echo ""
    printf "  ${DIM}[p] pausa/reanudar   [q] volver al menu${R}\n"
    frame_end
}

run_countdown() {
    local total=$1
    local label="${2:-TIMER}"
    local color="${3:-$CYN}"
    local remaining=$total

    PAUSED=0
    QUIT=0

    local saved_stty
    saved_stty=$(stty -g 2>/dev/null || echo "")
    stty -echo -icanon min 0 time 0 2>/dev/null
    hide_cursor
    clrscr

    while [[ $remaining -ge 0 && $QUIT -eq 0 ]]; do
        show_timer_screen "$remaining" "$total" "$label" "$color"

        local key=""
        if read -r -s -n 1 -t 1 key 2>/dev/null; then
            case "$key" in
                p|P)
                    if [[ $PAUSED -eq 0 ]]; then PAUSED=1; else PAUSED=0; fi
                    ;;
                q|Q|$'\x1b')
                    QUIT=1
                    ;;
            esac
        fi

        if [[ $PAUSED -eq 0 ]]; then
            (( remaining-- )) || true
        fi

        [[ $remaining -lt 0 ]] && break
    done

    [[ -n "$saved_stty" ]] && stty "$saved_stty" 2>/dev/null
    show_cursor

    if [[ $QUIT -eq 0 ]]; then
        timer_finished "$label"
        return 0
    fi
    return 1
}

timer_finished() {
    local label="${1:-}"
    local w
    w=$(term_width)

    clrscr
    echo ""
    printf "${GRN}"; draw_hline "█"; printf "${R}"
    echo ""
    center_pad "  ✓  TIEMPO COMPLETADO  ✓  "
    printf "${BOLD}${GRN}  ✓  TIEMPO COMPLETADO  ✓  ${R}\n"
    echo ""
    center_pad "$label"
    printf "${YLW}%s${R}\n" "$label"
    echo ""
    printf "${GRN}"; draw_hline "█"; printf "${R}"

    local i
    for (( i=0; i<3; i++ )); do
        printf '\a'
        sleep 0.4
    done

    echo ""
    printf "  ${DIM}Presiona cualquier tecla...${R}"
    read -r -s -n 1
}

mode_pomodoro() {
    clrscr
    draw_short_header "POMODORO"
    echo ""
    printf "  ${WHT}╔══════════════════════════════════╗${R}\n"
    printf "  ${WHT}║   CONFIGURAR SESION POMODORO     ║${R}\n"
    printf "  ${WHT}╚══════════════════════════════════╝${R}\n"
    echo ""

    local work_min short_min long_min

    while true; do
        printf "  ${GRN}Minutos de trabajo (1-120): ${R}"
        read -r work_min
        if [[ "$work_min" =~ ^[0-9]+$ && $work_min -ge 1 && $work_min -le 120 ]]; then
            break
        fi
        printf "  ${RED}⚠ Numero entre 1 y 120${R}\n"
    done

    while true; do
        printf "  ${GRN}Minutos de descanso corto (1-30): ${R}"
        read -r short_min
        if [[ "$short_min" =~ ^[0-9]+$ && $short_min -ge 1 && $short_min -le 30 ]]; then
            break
        fi
        printf "  ${RED}⚠ Numero entre 1 y 30${R}\n"
    done

    while true; do
        printf "  ${GRN}Minutos de descanso largo (1-60): ${R}"
        read -r long_min
        if [[ "$long_min" =~ ^[0-9]+$ && $long_min -ge 1 && $long_min -le 60 ]]; then
            break
        fi
        printf "  ${RED}⚠ Numero entre 1 y 60${R}\n"
    done

    local work_secs=$(( work_min * 60 ))
    local short_secs=$(( short_min * 60 ))
    local long_secs=$(( long_min * 60 ))

    echo ""
    printf "  ${DIM}Trabajo: ${YLW}%d min${R}  ${DIM}Descanso corto: ${YLW}%d min${R}  ${DIM}Descanso largo: ${YLW}%d min${R}\n" \
        "$work_min" "$short_min" "$long_min"
    printf "  ${WHT}[Enter] comenzar  [q] cancelar: ${R}"
    local confirm=""
    read -r confirm
    [[ "$confirm" == "q" || "$confirm" == "Q" ]] && return

    local session=1

    while true; do
        if run_countdown "$work_secs" "TRABAJO #${session} (${work_min}min)" "$RED"; then
            (( POMODORO_COUNT++ )) || true
        else
            return
        fi

        local break_time break_label break_color
        if (( POMODORO_COUNT % 4 == 0 )); then
            break_time=$long_secs
            break_label="DESCANSO LARGO (${long_min}min)"
            break_color="$MGT"
        else
            break_time=$short_secs
            break_label="DESCANSO CORTO (${short_min}min)"
            break_color="$GRN"
        fi

        clrscr
        draw_short_header "POMODORO"
        echo ""
        printf "  ${YLW}¿Tomar descanso? [Enter=si / q=no]: ${R}"
        local ans=""
        read -r -t 15 ans || true
        [[ "$ans" == "q" || "$ans" == "Q" ]] && return

        if ! run_countdown "$break_time" "$break_label" "$break_color"; then
            return
        fi

        (( session++ )) || true
    done
}

mode_stopwatch() {
    PAUSED=0
    QUIT=0

    local saved_stty
    saved_stty=$(stty -g 2>/dev/null || echo "")
    stty -echo -icanon min 0 time 0 2>/dev/null
    hide_cursor
    clrscr

    local start_time
    start_time=$(date +%s)
    local pause_start=0
    local pause_total=0
    local elapsed=0

    while [[ $QUIT -eq 0 ]]; do
        local now
        now=$(date +%s)

        if [[ $PAUSED -eq 0 ]]; then
            elapsed=$(( now - start_time - pause_total ))
        fi

        local time_str
        time_str=$(seconds_to_hms "$elapsed")

        go_home
        draw_short_header "CRONOMETRO"
        echo ""

        local w
        w=$(term_width)
        local big_w=48
        local pad=$(( (w - big_w) / 2 ))
        [[ $pad -lt 0 ]] && pad=0

        local row
        for (( row=0; row<5; row++ )); do
            printf "%${pad}s" ""
            local i
            for (( i=0; i<${#time_str}; i++ )); do
                local char="${time_str:$i:1}"
                printf "${CYN}%s ${R}" "$(get_digit_row "$char" "$row")"
            done
            printf "\n"
        done

        echo ""

        if [[ $PAUSED -eq 1 ]]; then
            printf "  ${YLW}${BOLD}⏸  PAUSADO${R}\n"
        else
            printf "  ${GRN}${BOLD}▶  CORRIENDO${R}\n"
        fi

        echo ""
        printf "  ${DIM}[p] pausa/reanudar   [r] reiniciar   [q] menu${R}\n"
        frame_end

        local key=""
        if read -r -s -n 1 -t 1 key 2>/dev/null; then
            case "$key" in
                p|P)
                    if [[ $PAUSED -eq 0 ]]; then
                        PAUSED=1
                        pause_start=$now
                    else
                        PAUSED=0
                        pause_total=$(( pause_total + now - pause_start ))
                    fi
                    ;;
                r|R)
                    start_time=$(date +%s)
                    pause_total=0
                    elapsed=0
                    PAUSED=0
                    ;;
                q|Q|$'\x1b')
                    QUIT=1
                    ;;
            esac
        fi
    done

    [[ -n "$saved_stty" ]] && stty "$saved_stty" 2>/dev/null
    show_cursor
}

mode_custom_timer() {
    clrscr
    draw_short_header "TEMPORIZADOR"
    echo ""
    printf "  ${WHT}╔══════════════════════════════╗${R}\n"
    printf "  ${WHT}║   TEMPORIZADOR PERSONALIZADO ║${R}\n"
    printf "  ${WHT}╚══════════════════════════════╝${R}\n"
    echo ""

    local minutes seconds

    while true; do
        printf "  ${GRN}Minutos (0-99): ${R}"
        read -r minutes
        if [[ "$minutes" =~ ^[0-9]+$ && $minutes -le 99 ]]; then
            break
        fi
        printf "  ${RED}⚠ Numero entre 0 y 99${R}\n"
    done

    while true; do
        printf "  ${GRN}Segundos (0-59): ${R}"
        read -r seconds
        if [[ "$seconds" =~ ^[0-9]+$ && $seconds -le 59 ]]; then
            break
        fi
        printf "  ${RED}⚠ Numero entre 0 y 59${R}\n"
    done

    local total=$(( minutes * 60 + seconds ))

    if [[ $total -le 0 ]]; then
        printf "  ${RED}⚠ El tiempo debe ser mayor que 0${R}\n"
        sleep 2
        return
    fi

    run_countdown "$total" "${minutes}m ${seconds}s" "$BLU"
}

mode_alarm() {
    clrscr
    draw_short_header "ALARMA"
    echo ""
    printf "  ${WHT}╔═══════════════════════════╗${R}\n"
    printf "  ${WHT}║   CONFIGURAR ALARMA       ║${R}\n"
    printf "  ${WHT}╚═══════════════════════════╝${R}\n"
    echo ""
    printf "  ${DIM}Hora actual: ${WHT}%s${R}\n\n" "$(now_time)"

    local alarm_input
    while true; do
        printf "  ${GRN}Hora de alarma (HH:MM): ${R}"
        read -r alarm_input
        if [[ "$alarm_input" =~ ^([01][0-9]|2[0-3]):[0-5][0-9]$ ]]; then
            break
        fi
        printf "  ${RED}⚠ Formato invalido. Usa HH:MM (ej: 14:30)${R}\n"
    done

    ALARM_TIME="$alarm_input"
    ALARM_SET=1
    PAUSED=0
    QUIT=0

    local saved_stty
    saved_stty=$(stty -g 2>/dev/null || echo "")
    stty -echo -icanon min 0 time 0 2>/dev/null
    hide_cursor
    clrscr

    while [[ $QUIT -eq 0 ]]; do
        local current_hm
        current_hm=$(now_hm)
        local time_str
        time_str=$(date "+%H:%M")

        go_home
        draw_short_header "ALARMA"
        echo ""

        local w
        w=$(term_width)
        local pad=$(( (w - 36) / 2 ))
        [[ $pad -lt 0 ]] && pad=0

        local row
        for (( row=0; row<5; row++ )); do
            printf "%${pad}s" ""
            local i
            for (( i=0; i<${#time_str}; i++ )); do
                local char="${time_str:$i:1}"
                printf "${YLW}%s ${R}" "$(get_digit_row "$char" "$row")"
            done
            printf "\n"
        done

        echo ""
        printf "  ${GRN}⏰ Alarma a las: ${BOLD}${YLW}%s${R}\n\n" "$ALARM_TIME"
        printf "  ${DIM}[q] cancelar alarma${R}\n"
        frame_end

        if [[ "$current_hm" == "$ALARM_TIME" ]]; then
            [[ -n "$saved_stty" ]] && stty "$saved_stty" 2>/dev/null
            show_cursor
            alarm_triggered
            ALARM_SET=0
            ALARM_TIME=""
            return
        fi

        local key=""
        if read -r -s -n 1 -t 1 key 2>/dev/null; then
            case "$key" in
                q|Q|$'\x1b')
                    QUIT=1
                    ;;
            esac
        fi
    done

    [[ -n "$saved_stty" ]] && stty "$saved_stty" 2>/dev/null
    show_cursor
    ALARM_SET=0
    ALARM_TIME=""
}

alarm_triggered() {
    local saved_stty
    saved_stty=$(stty -g 2>/dev/null || echo "")
    stty -echo -icanon min 0 time 0 2>/dev/null
    hide_cursor

    local blink=0
    local key=""
    clrscr

    while ! read -r -s -n 1 -t 0.5 key 2>/dev/null; do
        go_home
        local w
        w=$(term_width)

        if (( blink % 2 == 0 )); then
            printf "${BRED}"
        else
            printf "${BGRN}"
        fi

        echo ""
        echo ""
        local msg="  ⏰  ¡ A L A R M A !  ⏰  "
        center_pad "$msg"
        printf "%s\n" "$msg"
        echo ""
        local t
        t=$(now_time)
        local tmsg="  SON LAS  ${t}  "
        center_pad "$tmsg"
        printf "%s\n" "$tmsg"
        echo ""
        echo ""
        printf "${R}"

        printf '\a'
        (( blink++ )) || true

        printf "\n  ${DIM}Presiona cualquier tecla para silenciar...${R}"
        frame_end
    done

    [[ -n "$saved_stty" ]] && stty "$saved_stty" 2>/dev/null
    show_cursor
    clrscr
    printf "\n  ${GRN}✓ Alarma silenciada${R}\n"
    sleep 1
}

mode_ytmusic() {
    clrscr
    draw_short_header "YOUTUBE MUSIC"
    echo ""
    printf "  ${WHT}╔══════════════════════════════════════╗${R}\n"
    printf "  ${WHT}║         🎵  YOUTUBE MUSIC            ║${R}\n"
    printf "  ${WHT}╚══════════════════════════════════════╝${R}\n"
    echo ""

    if ! command -v yt-dlp &>/dev/null; then
        printf "  ${RED}⚠  yt-dlp no esta instalado${R}\n"
        printf "  ${DIM}   pacman -S yt-dlp${R}\n\n"
        printf "  ${DIM}Presiona cualquier tecla...${R}"
        read -r -s -n 1
        return
    fi

    if ! command -v mpv &>/dev/null; then
        printf "  ${RED}⚠  mpv no esta instalado${R}\n"
        printf "  ${DIM}   pacman -S mpv${R}\n\n"
        printf "  ${DIM}Presiona cualquier tecla...${R}"
        read -r -s -n 1
        return
    fi

    if [[ $MUSIC_PID -ne 0 ]] && kill -0 "$MUSIC_PID" 2>/dev/null; then
        printf "  ${GRN}♪ Sonando: ${YLW}%s${R}\n\n" "$MUSIC_TITLE"
        printf "  ${GRN}[1]${R} Cambiar cancion\n"
        printf "  ${RED}[0]${R} Detener musica\n"
        printf "  ${DIM}[q]${R} Volver al menu\n\n"
        printf "  Opcion: "
        local opt
        read -r opt
        case "$opt" in
            0) stop_music; return ;;
            1) stop_music ;;
            *) return ;;
        esac
    fi

    local query
    printf "  ${GRN}¿Que musica queres escuchar? ${R}"
    read -r query

    [[ -z "$query" ]] && return

    clrscr
    draw_short_header "YOUTUBE MUSIC"
    echo ""
    printf "  ${CYN}🔍 Buscando: ${YLW}%s${R}\n\n" "$query"
    printf "  ${DIM}Obteniendo audio...${R}\n\n"

    local url
    url=$(yt-dlp --get-url --format bestaudio "ytsearch1:${query}" 2>/dev/null | head -1)

    if [[ -z "$url" ]]; then
        printf "  ${RED}⚠  No se encontro resultado para: %s${R}\n\n" "$query"
        printf "  ${DIM}Presiona cualquier tecla...${R}"
        read -r -s -n 1
        return
    fi

    mpv --no-video --really-quiet --no-terminal "$url" </dev/null &>/dev/null &
    MUSIC_PID=$!
    disown "$MUSIC_PID"
    MUSIC_TITLE="$query"

    printf "  ${GRN}▶  Reproduciendo en background: ${YLW}%s${R}\n\n" "$query"
    printf "  ${DIM}Volviendo al menu en 2 segundos...${R}\n"
    sleep 2
}

mode_stats() {
    clrscr
    draw_short_header "ESTADISTICAS"
    echo ""

    local total_min=$(( POMODORO_COUNT * 25 ))

    printf "  ${WHT}╔══════════════════════════════════════╗${R}\n"
    printf "  ${WHT}║           ESTADISTICAS               ║${R}\n"
    printf "  ${WHT}╠══════════════════════════════════════╣${R}\n"
    printf "  ${WHT}║  ${GRN}🍅 Pomodoros:     ${YLW}%-4d${WHT}               ║${R}\n" "$POMODORO_COUNT"
    printf "  ${WHT}║  ${GRN}⏱  Tiempo total:  ${YLW}%d min${WHT}            ║${R}\n" "$total_min"
    printf "  ${WHT}║  ${GRN}📅 Fecha:         ${YLW}%-20s${WHT} ║${R}\n" "$(date '+%d/%m/%Y')"
    printf "  ${WHT}║  ${GRN}🕐 Hora:          ${YLW}%-8s${WHT}              ║${R}\n" "$(now_time)"
    printf "  ${WHT}╚══════════════════════════════════════╝${R}\n"
    echo ""
    printf "  ${DIM}Presiona cualquier tecla para volver...${R}"
    read -r -s -n 1
}

main() {
    init_digits
    SAVED_STTY=$(stty -g 2>/dev/null || echo "")

    while true; do
        draw_menu

        local choice=""
        read -r choice

        case "$choice" in
            1) mode_pomodoro    ;;
            2) mode_stopwatch   ;;
            3) mode_custom_timer ;;
            4) mode_alarm       ;;
            5) mode_stats        ;;
            9) mode_ytmusic     ;;
            0) stop_music       ;;
            q|Q)
                clrscr
                printf "\n  ${GRN}¡Hasta luego! 🍅 Pomodoros: ${YLW}${POMODORO_COUNT}${R}\n\n"
                exit 0
                ;;
        esac
    done
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
