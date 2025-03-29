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

struct SDL_Semaphore {
    SemaphoreHandle_t semaphore;
};

SDL_Semaphore *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_Semaphore *sem = (SDL_Semaphore *)SDL_malloc(sizeof(*sem));
    if (!sem) {
        return NULL;
    }

    sem->semaphore = xSemaphoreCreateCounting(portMAX_DELAY, initial_value);
    if (!sem->semaphore) {
        SDL_free(sem);
        return NULL;
    }

    return sem;
}

void SDL_DestroySemaphore(SDL_Semaphore *sem)
{
    if (sem) {
        if (sem->semaphore) {
            vSemaphoreDelete(sem->semaphore);
        }
        SDL_free(sem);
    }
}

int SDL_WaitSemaphoreTimeoutNS(SDL_Semaphore *sem, Sint64 timeoutNS)
{
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }

    BaseType_t result = xSemaphoreTake(sem->semaphore, pdMS_TO_TICKS(timeoutNS / 1000000));
    return (result == pdPASS) ? 0 : SDL_MUTEX_TIMEDOUT;
}

Uint32 SDL_GetSemaphoreValue(SDL_Semaphore *sem)
{
    if (!sem) {
        return 0;
    }

    return uxSemaphoreGetCount(sem->semaphore);
}

int SDL_SignalSemaphore(SDL_Semaphore *sem)
{
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }

    xSemaphoreGive(sem->semaphore);
    return 0;
}
