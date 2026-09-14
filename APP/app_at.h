#ifndef APP_AT_H
#define APP_AT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void App_ATTask(void *argument);
void App_AT_HandleKey0(void);
void App_AT_HandleKey1(void);
void App_AT_HandleKey2(void);
void App_AT_UpdateNetworkState(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_AT_H */
