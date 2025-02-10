#ifndef __MIDIPROC_H__
#define __MIDIPROC_H__

#pragma once
// C API
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif
#ifdef __cplusplus
// define the DLL export if we're on windows


extern "C" {
#endif

// we are only creating functions which implement the processor

// HMidiProcessor, handle for MidiProcessor
typedef struct MIDIProcessor * HMidiProcessor;
typedef struct MIDIContainer * HMIDIContainer;
typedef struct MIDITrack * HMIDITrack;


/**
 * @brief One-shot function to process the data and serialize it to a Standard MIDI File
 * 
 * @param data The data to process
 * @param data_size The size of the data
 * @param file_extension The file extension of the data
 * @param data_out A pointer to the output data (must be freed by the caller)
 * 
 * @return The size of the output data
 * 
*/
EXPORT size_t MIDPROC_process_and_serialize_to_smf(const uint8_t * data, size_t data_size, const char * file_extension, uint8_t ** data_out);

/**
 * @brief Create a new container
 * 
 * @return A handle to the container
*/
EXPORT HMIDIContainer MIDPROC_Container_Create(void);

/**
 * @brief Delete a container
 * 
 * @param processor The container to delete
*/
EXPORT void MIDPROC_Container_Delete(HMIDIContainer processor);


/**
 * @brief Process the data and store it in the container
 * 
 * @param data The data to process
 * @param data_size The size of the data
 * @param file_extension The file extension of the data (e.g. "mid", "rcp", "lds", etc.)
 * @param container The container to store the data in (must be created with MIDPROC_Container_Create())
*/
EXPORT bool MIDPROC_Process(const uint8_t * data, size_t data_size, const char * file_extension, HMIDIContainer container);

/**
 * @brief Serialize the container as a Standard MIDI File
 * 
 * @param container The container to serialize
 * @param data_out A pointer to the output data (allocated by function, must be freed by the caller)
 * @param data_out_size The size of the output data
*/
EXPORT void MIDPROC_Container_SerializeAsSMF(HMIDIContainer container, uint8_t ** data_out, size_t * data_out_size);
EXPORT uint32_t MIDPROC_Container_GetFormat(HMIDIContainer container);
EXPORT uint32_t MIDPROC_Container_GetTrackCount(HMIDIContainer container);
EXPORT uint32_t MIDPROC_Container_GetChannelCount(HMIDIContainer container, size_t subSongIndex);
EXPORT uint32_t MIDPROC_Container_GetLoopBeginTimestamp(HMIDIContainer container, size_t subSongIndex, bool ms);
EXPORT uint32_t MIDPROC_Container_GetLoopEndTimestamp(HMIDIContainer container, size_t subSongIndex, bool ms);
//GetDuration
EXPORT uint32_t MIDPROC_Container_GetDuration(HMIDIContainer container, size_t subSongIndex, bool ms);
EXPORT size_t MIDPROC_Container_GetSubSongCount(HMIDIContainer container);
EXPORT size_t MIDPROC_Container_GetSubSong(HMIDIContainer container, size_t index);
EXPORT void MIDPROC_Container_PromoteToType1(HMIDIContainer container);
EXPORT void MIDPROC_Container_DetectLoops(HMIDIContainer container, bool detectXMILoops, bool detectMarkerLoops, bool detectRPGMakerLoops, bool detectTouhouLoops);

// TODO: the rest
#ifdef __cplusplus
}
#endif

#endif /* __MIDIPROC_H__ */
