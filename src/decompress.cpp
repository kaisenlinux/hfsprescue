/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 */


#define _FILE_OFFSE_BITS 64
#define _LARGEFILE64_SOURCE

#include "config.h"

#include "apple.h"
#include "freebsd.h"

#include <stdio.h>


#include <zlib.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
	 
#include "tools.h"
#include "log.h"



unsigned char decompress_cache[1024 * 200];
uint64_t      decompress_file_offset;

int decompress_cache_offset;
int fd_decompress;

extern uint32_t block_size;
extern bool ignore_file_create_error;
extern bool file_create_error_skipped;


bool FillDecompressCache()
{
    uint64_t bytes;
    
    bytes = pread64 (fd_decompress, decompress_cache, sizeof (decompress_cache), decompress_file_offset);
    if (bytes == 0) 
    {
	return false;
    }
    
    decompress_file_offset += bytes;    
    return true;
}

// Get char from cache, load new cache if end of cache reached
char GetChar()
{
    char c = decompress_cache[decompress_cache_offset];
    
    decompress_cache_offset++;
    if (decompress_cache_offset >= sizeof (decompress_cache))
    {
	decompress_cache_offset = 0;
	FillDecompressCache();    
    }
    return c;
}

bool RestoreCompressedFile (int fd, uint64_t fork_size, uint32_t fork_block, uint32_t num_blocks, uint64_t offset, char *file_name, uint32_t file_number)
{
    unsigned char buffer_in[1024 * 70];
    unsigned char buffer_out[1024 * 1024];
    
    int buffer_in_offset = 0;
    uint64_t bytes_left;
    
    FILE *pf;
    
    fd_decompress = fd;    
    
    bytes_left = fork_size;
    
    decompress_file_offset =  (uint64_t)fork_block;
    decompress_file_offset *= (uint64_t)block_size;
    decompress_file_offset += offset;
    
    
    pf = fopen (file_name, "w");
    if (!pf)
    {
        LogPrn ("(decompress) Unable to create file '%s': %s\n", file_name, strerror (errno));


        if (ignore_file_create_error)
        {
            LogPrn ("Error: _file_create_error_: Skipping file restore. File number: %" PRIu32 ".", file_number);
            file_create_error_skipped = true;
            return false;
        }
        else
        {
            LogPrn ("Error: _file_create_error_: File number: %" PRIu32 ". Use '--ignore-file-error' to skip this error.", file_number);
            exit (EXIT_FAILURE);
        }
    }

    // init cache
    decompress_cache_offset = 0;
    FillDecompressCache();    

    char c1 = 0;
    char c2 = 0;

    for (int i = 0; i < 0x100; i++) GetChar(); // skip bytes
    
    // find first block
    while (1)
    {
	c1 = c2;
	c2 = GetChar();
	
	bytes_left--;

	if (bytes_left <= 0) 
	{
	    fclose (pf);
	    return false;
	}
	
	if ((c1 == 0x78) && (c2 == 0x5e)) break;
    }

    // decompress blocks
    while (bytes_left > 0)
    {
	bool loop = true;
	buffer_in_offset = 0;

	// header
	buffer_in[buffer_in_offset++] = c1;
	buffer_in[buffer_in_offset++] = c2;

	while ((bytes_left > 0) && loop)
	{
    	    c1 = c2;
	    c2 = GetChar();
	    
	    buffer_in[buffer_in_offset++] = c2;
	    bytes_left--;

	    // uncompress if last block or header 
	    if ((bytes_left == 0) || ((c1 == 0x78) && (c2 == 0x5e)))
	    {
		unsigned long int size = sizeof (buffer_out);
		int ret = uncompress (buffer_out, &size, buffer_in, buffer_in_offset);
		if (ret == Z_OK)
		{
		    fwrite (buffer_out, 1, size, pf);
		    
		    // bail out and begin with a new buffer
		    loop = false;
		}		
	    }

	    if (buffer_in_offset > 65536) 
	    {
		bytes_left = 0; // stop loop, problem with compression
    		LogPrn ("Problem with decompression. %s", file_name);
	    }
	}
    }
    fclose (pf);
    return true;
}
