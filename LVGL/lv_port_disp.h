#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void lv_port_disp_init(void);
void disp_enable_update(void);
void disp_disable_update(void);

#ifdef __cplusplus
}
#endif

#endif
