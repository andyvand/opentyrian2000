/* Copyright 2016 - 2017 Marc Volker Dickmann */
#include <stdio.h>
#include <stdlib.h>
#include "libwav.h"

static enum wav_error
wav_content_read (wav_chunk *chunk, FILE *f)
{
	const size_t items = chunk->chunk_size / sizeof (int);
	
	return (fread (chunk->content.data, sizeof (int), items, f) == items ? WAV_OK : WAV_ERROR);
}

// WAV_HEADER

enum wav_error
wav_header_write (const wav_header *header, FILE *f)
{
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	else if (fwrite (header, sizeof (wav_header), 1, f) != 1)
	{
		return WAV_ERROR;
	}
	return WAV_OK;
}

enum wav_error
wav_header_read (wav_header *header, FILE *f)
{
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	else if (fread (header, sizeof (wav_header), 1, f) != 1)
	{
		return WAV_ERROR;
	}
	return WAV_OK;
}

void
wav_header_print (const wav_header *header)
{
	printf ("Riff Type:\t%.4s\n", header->riff_type.str);
}

// WAV_FORMAT

enum wav_error
wav_format_write (const wav_format *format, FILE *f)
{
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	else if (fwrite (format, sizeof (wav_format), 1, f) != 1)
	{
		return WAV_ERROR;
	}
	return WAV_OK;
}

enum wav_error
wav_format_read (wav_format *format, FILE *f)
{
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	else if (fread (format, sizeof (wav_format), 1, f) != 1)
	{
		return WAV_ERROR;
	}
	return WAV_OK;
}

void
wav_format_print (const wav_format *format)
{
	printf ("Format:\t\t%hu\n", format->format);
	printf ("Channels:\t%hu\n", format->channels);
	printf ("Samplerate:\t%u\n", format->samplerate);
	printf ("Bytespersec:\t%u\n", format->bytespersec);
	printf ("Blockalign:\t%hu\n", format->blockalign);
	printf ("Bitwidth:\t%hu\n", format->bitwidth);
}

// WAV_CHUNK

void
wav_chunk_init (wav_chunk *chunk, const unsigned int id, const unsigned int size, const void *content)
{
	chunk->chunk_id.hash = id;
	chunk->chunk_size = size;
	
	// Init the content based on the chunkid:
	switch (chunk->chunk_id.hash)
	{
		case WAV_CHUNKID_RIFF:
			chunk->content.header = *(wav_header*) content;
			break;
		case WAV_CHUNKID_FORMAT:
			chunk->content.format = *(wav_format*) content;
			break;
		case WAV_CHUNKID_DATA:
			chunk->content.data = (int*) content;
			break;
		default:
			break;
	}
}

enum wav_error
wav_chunk_write (const wav_chunk *chunk, FILE *f)
{
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	else if (fwrite (&chunk->chunk_id, sizeof (chunk->chunk_id), 1, f) != 1 ||
		 fwrite (&chunk->chunk_size, sizeof (chunk->chunk_size), 1, f) != 1)
	{
		return WAV_ERROR;
	}
	
	// Write the content based on the chunkid:
	switch (chunk->chunk_id.hash)
	{
		case WAV_CHUNKID_RIFF:
			return wav_header_write (&chunk->content.header, f);
		case WAV_CHUNKID_FORMAT:
			return wav_format_write (&chunk->content.format, f);
		case WAV_CHUNKID_DATA:
			fwrite (chunk->content.data, sizeof (int), chunk->chunk_size / sizeof (int), f);
			return WAV_OK;
		default:
			return WAV_UNKNOWN_CHUNKID;
	}
}

enum wav_error
wav_chunk_read (wav_chunk *chunk, FILE *f)
{
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	else if (fread (&chunk->chunk_id, sizeof (chunk->chunk_id), 1, f) != 1 ||
		 fread (&chunk->chunk_size, sizeof (chunk->chunk_size), 1, f) != 1)
	{
		return WAV_ERROR;
	}
	
	// Read the content based on the chunkid:
	switch (chunk->chunk_id.hash)
	{
		case WAV_CHUNKID_RIFF:
			return wav_header_read (&chunk->content.header, f);
		case WAV_CHUNKID_FORMAT:
			return wav_format_read (&chunk->content.format, f);
		case WAV_CHUNKID_DATA:
			chunk->content.data = malloc (chunk->chunk_size);
			return wav_content_read (chunk, f);
		default:
			return WAV_UNKNOWN_CHUNKID;
	}
}

void
wav_chunk_print (const wav_chunk *chunk)
{
	printf ("Chunk ID:\t%.4s\n", chunk->chunk_id.str);
	printf ("Chunk Size:\t%u\n", chunk->chunk_size);
	
	switch (chunk->chunk_id.hash)
	{
		case WAV_CHUNKID_RIFF:
			wav_header_print (&chunk->content.header);
			break;
		case WAV_CHUNKID_FORMAT:
			wav_format_print (&chunk->content.format);
			break;
		default:
			break;
	}
}

// WAV_FILE

void
wav_free (wav_file *wavfile)
{
	free (wavfile->data);
}

enum wav_error
wav_write (const wav_file *wavfile, const char *filename)
{
	FILE *f = fopen (filename, "wb");
	
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	
	// Header
	wav_chunk chunk;
	
	wav_chunk_init (&chunk, WAV_CHUNKID_RIFF, 36 + (sizeof (int) * wavfile->datablocks), &wavfile->header);
	wav_chunk_write (&chunk, f);
	
	// Format
	wav_chunk_init (&chunk, WAV_CHUNKID_FORMAT, 16, &wavfile->format);
	wav_chunk_write (&chunk, f);
	
	// Data
	wav_chunk_init (&chunk, WAV_CHUNKID_DATA, sizeof (int) * wavfile->datablocks, wavfile->data);
	wav_chunk_write (&chunk, f);
	
	fclose (f);
	return WAV_OK;
}

enum wav_error
wav_read (wav_file *wavfile, const char *filename)
{
	FILE *f = fopen (filename, "rb");
	
	if (f == NULL)
	{
		return WAV_FILE_NOT_OPENED;
	}
	
	// Check if its a valid file:
	wav_chunk chunk;
	
	if (wav_chunk_read (&chunk, f) != WAV_OK ||
	    chunk.chunk_id.hash != WAV_CHUNKID_RIFF)
	{
		fclose (f);
		return WAV_INVALID_FILE;
	}
	
	wavfile->header = chunk.content.header;
	wavfile->datablocks = 0;
	wavfile->data = NULL;
	
	// Read the chunks:
	while (!feof (f))
	{
		wav_chunk_read (&chunk, f);
		
		switch (chunk.chunk_id.hash)
		{
			case WAV_CHUNKID_FORMAT:
				wavfile->format = chunk.content.format;
				break;
			case WAV_CHUNKID_DATA:
				wavfile->datablocks = chunk.chunk_size / sizeof (int);
				wavfile->data = chunk.content.data;
				fclose (f);
				return WAV_OK;
			default:
				// NOTE: Unknown chunk!
				fseek (f, chunk.chunk_size, SEEK_CUR);
				break;
		}
	}
	
	// No DATA chunk found!
	fclose (f);
	return WAV_ERROR;
}
