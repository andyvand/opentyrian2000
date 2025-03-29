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
#include "freertos/task.h"
#include "esp_log.h"
#include "thread/SDL_thread_c.h"

static void SDL_RunTask(void *data)
{
    SDL_Thread *thread = (SDL_Thread *)data;

    if (thread && thread->userfunc) {
        thread->userfunc(thread->userdata);  // Execute the user function without storing the result
    }

    vTaskDelete(NULL);  // End the task
}

int SDL_SYS_CreateThread(SDL_Thread *thread,
                         SDL_FunctionPointer pfnBeginThread,
                         SDL_FunctionPointer pfnEndThread)
{
    if (!thread) {
        ESP_LOGE("SDL_SYS_CreateThread", "Invalid thread");
        return SDL_SetError("Invalid thread");
    }

    if (!thread->userfunc) {
        ESP_LOGE("SDL_SYS_CreateThread", "Invalid thread - user function is NULL");
        return SDL_SetError("Invalid thread - user function");
    }

    ESP_LOGI("SDL_SYS_CreateThread", "Creating thread: %p", (void *)thread);
    ESP_LOGI("SDL_SYS_CreateThread", "Thread function: %p", (void *)thread->userfunc);
    ESP_LOGI("SDL_SYS_CreateThread", "Thread name: %s", thread->name ? thread->name : "NULL");

    BaseType_t result = xTaskCreate(
        SDL_RunTask,  // Thread function
        thread->name ? thread->name : "SDL_Thread",  // Task name
        thread->stacksize ? thread->stacksize : CONFIG_ESP_MAIN_TASK_STACK_SIZE,  // Stack size
        (void *)thread,  // Task input (userdata)
        tskIDLE_PRIORITY + 1,  // Priority
        &thread->handle  // Task handle (of type TaskHandle_t)
    );

    if (result != pdPASS) {
        ESP_LOGE("SDL_SYS_CreateThread", "Failed to create thread");
        return SDL_SetError("Failed to create thread");
    }

    // Use uintptr_t to convert the pointer to an integer of the appropriate size
    thread->threadid = (SDL_ThreadID)(uintptr_t)thread->handle;  // Store the TaskHandle_t as SDL_ThreadID
    return 0;
}

void SDL_SYS_SetupThread(const char *name)
{
    // No-op for ESP-IDF since task naming is handled at creation
}

SDL_ThreadID SDL_GetCurrentThreadID(void)
{
    return (SDL_ThreadID)(uintptr_t)xTaskGetCurrentTaskHandle();  // Return TaskHandle_t as SDL_ThreadID
}

int SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    // Adjust thread priority as needed (can be expanded)
    return 0;
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    if (thread && thread->handle) {
        vTaskDelete(thread->handle);  // Properly pass the TaskHandle_t
    }
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    // No-op since FreeRTOS does not require explicit detachment
}
