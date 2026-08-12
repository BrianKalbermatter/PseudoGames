#ifndef CONFIG_H
#define CONFIG_H

void config_cargar(void);
void config_guardar(void);

int config_get_lluvia(void);   /* 0=Lenta, 1=Normal, 2=Rapida */
int config_get_brillo(void);   /* 0=Bajo,  1=Normal, 2=Alto   */
int config_get_volumen(void);  /* 0-100 */

void config_set_lluvia(int v);
void config_set_brillo(int v);
void config_set_volumen(int v);

#endif
