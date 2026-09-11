#include "svc_state.h"

static SystemState_t system_state = SYSTEM_STATE_OFFLINE;

void SvcState_Init(void)
{
    system_state = SYSTEM_STATE_OFFLINE;
}

void SvcState_Set(SystemState_t state)
{
    system_state = state;
}

SystemState_t SvcState_Get(void)
{
    return system_state;
}

void SvcState_ToggleOnline(void)
{
    if (system_state == SYSTEM_STATE_ONLINE)
    {
        system_state = SYSTEM_STATE_OFFLINE;
    }
    else
    {
        system_state = SYSTEM_STATE_ONLINE;
    }
}

bool SvcState_IsOnline(void)
{
    return (system_state == SYSTEM_STATE_ONLINE);
}

bool SvcState_IsOFFline(void)
{
    return (system_state == SYSTEM_STATE_OFFLINE);
}
