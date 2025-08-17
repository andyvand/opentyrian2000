#ifndef __LIBWAV_H__
#define __LIBWAV_H__

// "RIFF"
#define WAV_CHUNKID_RIFF 0x46464952

// "fmt "
#define WAV_CHUNKID_FORMAT 0x20746D66

// "data"
#define WAV_CHUNKID_DATA 0x61746164

// HASH4
#ifndef __HASH4_H__
#define __HASH4_H__
union _hash4
{
	unsigned int hash;
	char str[4];
};
#endif /* __HASH4_H__ */

enum wav_error
{
	WAV_FILE_NOT_OPENED = -4,
	WAV_INVALID_FILE,
	WAV_UNKNOWN_CHUNKID,
	WAV_ERROR,
	WAV_OK = 0
};

struct _wav_header
{
	union _hash4 riff_type;
};

typedef struct _wav_header wav_header;

struct _wav_format
{
	unsigned short format;
	unsigned short channels;
	unsigned int samplerate;
	unsigned int bytespersec;
	unsigned short blockalign;
	unsigned short bitwidth;
};

typedef struct _wav_format wav_format;

union _wav_chunk_content
{
	wav_header header;
	wav_format format;
	int *data;
};

struct _wav_chunk
{
	union _hash4 chunk_id;
	unsigned int chunk_size;
	union _wav_chunk_content content;
};

typedef struct _wav_chunk wav_chunk;

struct _wav_file
{
	wav_header header;
	wav_format format;
	size_t datablocks;
	int *data;
};

typedef struct _wav_file wav_file;

// WAV_FORMAT
enum wav_error wav_format_write (const wav_format*, FILE*);
enum wav_error wav_format_read (wav_format*, FILE*);
void wav_format_print (const wav_format*);

// WAV_HEADER
enum wav_error wav_header_write (const wav_header*, FILE*);
enum wav_error wav_header_read (wav_header*, FILE*);
void wav_header_print (const wav_header*);

// WAV_CHUNK
void wav_chunk_init (wav_chunk*, const unsigned int, const unsigned int, const void*);
enum wav_error wav_chunk_write (const wav_chunk*, FILE*);
enum wav_error wav_chunk_read (wav_chunk*, FILE*);
void wav_chunk_print (const wav_chunk*);

// WAV_FILE
void wav_free (wav_file*);
enum wav_error wav_write (const wav_file*, const char*);
enum wav_error wav_read (wav_file*, const char*);

#endif /* __LIBWAV_H__ */
