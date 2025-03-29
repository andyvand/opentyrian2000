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

struct SDL_Mutex
{
    int recursive;
    SDL_ThreadID owner;
    SemaphoreHandle_t sem;  // Use FreeRTOS semaphore here
};

SDL_Mutex *SDL_CreateMutex(void)
{
    SDL_Mutex *mutex = (SDL_Mutex *)SDL_calloc(1, sizeof(*mutex));
    if (mutex) {
        mutex->sem = xSemaphoreCreateRecursiveMutex();
        if (!mutex->sem) {
            SDL_free(mutex);
            return NULL;
        }
        mutex->recursive = 0;
        mutex->owner = 0;
    }
    return mutex;
}

void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        if (mutex->sem) {
            vSemaphoreDelete(mutex->sem);
        }
        SDL_free(mutex);
    }
}

void SDL_LockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS
{
    if (mutex != NULL) {
        SDL_ThreadID this_thread = SDL_GetCurrentThreadID();
        if (mutex->owner == this_thread) {
            ++mutex->recursive;
        } else {
            xSemaphoreTakeRecursive(mutex->sem, portMAX_DELAY);
            mutex->owner = this_thread;
            mutex->recursive = 0;
        }
    }
}

int SDL_TryLockMutex(SDL_Mutex *mutex)
{
    int retval = 0;
    if (mutex) {
        SDL_ThreadID this_thread = SDL_GetCurrentThreadID();
        if (mutex->owner == this_thread) {
            ++mutex->recursive;
        } else {
            if (xSemaphoreTakeRecursive(mutex->sem, 0) == pdPASS) {
                mutex->owner = this_thread;
                mutex->recursive = 0;
                retval = 0;
            } else {
                retval = SDL_MUTEX_TIMEDOUT;
            }
        }
    }
    return retval;
}

void SDL_UnlockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS
{
    if (mutex != NULL) {
        if (SDL_GetCurrentThreadID() != mutex->owner) {
            SDL_assert(!"Tried to unlock a mutex we don't own!");
            return;
        }

        if (mutex->recursive) {
            --mutex->recursive;
        } else {
            mutex->owner = 0;
            xSemaphoreGiveRecursive(mutex->sem);
        }
    }
}
