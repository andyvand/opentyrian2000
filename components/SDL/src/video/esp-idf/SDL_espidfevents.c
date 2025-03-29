

#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PRIVATE

#include "events/SDL_events_c.h"
#include "SDL_espidfvideo.h"
#include "SDL_espidfevents.h"
#include "SDL_espidftouch.h"

void ESPIDF_PumpEvents(SDL_VideoDevice *_this)
{
    // Placeholder for actual event polling implementation
    // Depending on the platform, poll for touchscreen or other events here.

    ESPIDF_PumpTouchEvent();
}

#endif /* SDL_VIDEO_DRIVER_PRIVATE */
