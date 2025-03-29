/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct SDL_Condition {
    SemaphoreHandle_t semaphore;
};

SDL_Condition *SDL_CreateCondition(void)
{
    SDL_Condition *cond = (SDL_Condition *)SDL_malloc(sizeof(*cond));
    if (!cond) {
        return NULL;
    }

    cond->semaphore = xSemaphoreCreateBinary();
    if (!cond->semaphore) {
        SDL_free(cond);
        return NULL;
    }

    return cond;
}

void SDL_DestroyCondition(SDL_Condition *cond)
{
    if (cond) {
        if (cond->semaphore) {
            vSemaphoreDelete(cond->semaphore);
        }
        SDL_free(cond);
    }
}

int SDL_SignalCondition(SDL_Condition *cond)
{
    if (!cond) {
        return SDL_InvalidParamError("cond");
    }

    xSemaphoreGive(cond->semaphore);
    return 0;
}

int SDL_BroadcastCondition(SDL_Condition *cond)
{
    if (!cond) {
        return SDL_InvalidParamError("cond");
    }

    while (xSemaphoreGive(cond->semaphore) == pdPASS) {
        // Broadcasting: keep releasing the semaphore for all waiting tasks
    }

    return 0;
}

int SDL_WaitConditionTimeoutNS(SDL_Condition *cond, SDL_Mutex *mutex, Sint64 timeoutNS)
{
    if (!cond) {
        return SDL_InvalidParamError("cond");
    }

    SDL_UnlockMutex(mutex);
    int result = xSemaphoreTake(cond->semaphore, pdMS_TO_TICKS(timeoutNS / 1000000));
    SDL_LockMutex(mutex);

    return (result == pdPASS) ? 0 : SDL_MUTEX_TIMEDOUT;
}
