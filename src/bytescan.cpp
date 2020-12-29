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
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <vector>
    


#include <stdlib.h>
#include <string.h>

#include "bytescan.h"
#include "tools.h"
#include "log.h"
#include "utf8mac.h"



char unicode[1024];
size_t unicode_len;


bool ScanForUnicode (char *buffer, int size, char *str, uint64_t fpos)
{
    bool ret = false;
    for (int i = 0; i < size - unicode_len; i++)
    {
	if (memcmp (buffer + i, unicode, unicode_len) == 0)
	{
    	    LogPrn ("String \"%s\" found at offset %" PRIu64 " + %d = %" PRIu64 " (0x%llx + 0x%x = 0x%llx)", str, fpos, i, fpos + i, fpos, i, fpos + i);
    	    ret = true;
	}
    }
    
    return ret;
}

bool ScanForBytes (char *file_name, char *buffer, int size, char *buffer2, int size2, uint64_t fpos)
{
    bool ret = false;
    for (int i = 0; i < size - size2; i++)
    {
	if (memcmp (buffer + i, buffer2, size2) == 0)
	{
    	    LogPrn ("File %s: Bytes found at offset %" PRIu64 " + %d = %" PRIu64 " (0x%llx + 0x%x = 0x%llx)", file_name, fpos, i, fpos + i, fpos, i, fpos + i);
    	    ret = true;
	}    
    }
    
    return ret;
}

#define OUTPUT_LIMIT 4
void ByteScan (char *file, char *find_string, std::vector <char *> *file_names, int num_bytes, uint64_t offset)
{
    if (num_bytes > 1024 * 1024)
    {
	fprintf (stderr, "Error: max size is %d (1MB)\n", 1024 * 1024);
	exit (EXIT_FAILURE);
    }

    FILE *pf;
    char buffer[1024 * 1024];
    std::vector <char *> buffer_file;
    
    int mb = 0;
    int out = OUTPUT_LIMIT;
    uint64_t fpos = offset;
    int fd;
    
    size_t consumed;

    if ((file_names->size() < 2) && !find_string[0])
    {
	fprintf (stderr, "Error: For --find you also have to use -ff (find file) and/or -fs (find string).\n");
	exit (EXIT_FAILURE);
    }

    
    if (file_names->size() > 1)
    {
	char *p = NULL;
	buffer_file.push_back (p); // First entry is scan size from parameters. So set a dummy buffer to have the same index later.
	for (int i = 1; i < file_names->size(); i++)
	{
	    p = (char *)malloc (num_bytes);
	    if (!p)
	    {
		fprintf (stderr, "Unable to allocate file scan buffer\n");
		exit (EXIT_FAILURE);	    
	    }
	    buffer_file.push_back (p);
	    
	    pf = fopen (file_names->at(i), "r");
	    if (!pf)
	    {
		fprintf (stderr, "Unable to open '%s': %s\n", file_names->at(i), strerror (errno));
		exit (EXIT_FAILURE);
	    }    
	    LogPrn ("Reading %d bytes from '%s'.", num_bytes, file_names->at(i));
	    fread (buffer_file[i], num_bytes, 1, pf);    
	    fclose (pf);
	}
	LogPrn ("");
    }
    
    if (offset > 0)
    {
        LogPrn ("*** Skipping %" PRIu64 " bytes (%" PRIu64 " MB)", offset, offset / 1024 / 1024);
    }
    
    
    if (find_string[0])
    {
	int ret;
	ret = utf8_decodestr ((u_int8_t*)find_string, strlen (find_string),
            		      (u_int16_t *)unicode, &unicode_len, sizeof (unicode),
            		      0, 0,
            		      &consumed);
            		      
        if (ret == ENAMETOOLONG)
        {
    	    LogPrn ("Name didn't fit; only %d chars were decoded.", unicode_len);
    	}
    	
    	if (ret == EINVAL)
    	{
	    LogPrn ("Illegal UTF-8 sequence found.");
    	    exit (EXIT_FAILURE);
    	}

	// swap unicode bytes  (not implemented in utf8_decodestr)
	for (int i = 0; i < unicode_len; i += 2)
	{
	    char c = unicode[i];
	    unicode[i] = unicode[i + 1];
	    unicode[i + 1] = c;
	}
    }


    fd = open64 (file, O_RDONLY);
    if (fd < 0)
    {
	fprintf (stderr, "Unable to open %s: %s\n", file, strerror (errno));
	exit (EXIT_FAILURE);
    }    

    while (pread64 (fd, buffer, sizeof (buffer), fpos))
    {
	if (find_string[0]) 
	{
	    if (ScanForUnicode (buffer, sizeof (buffer), find_string, fpos))
	    {
		out = OUTPUT_LIMIT; // Force print to screen.
	    }
	}
	
	if (file_names->size() > 1)
	{
	    for (int i = 1; i < file_names->size(); i++)
	    {
		if (ScanForBytes   (file_names->at(i), buffer, sizeof (buffer), buffer_file[i], num_bytes, fpos))
		{
		    out = OUTPUT_LIMIT; // Force print to screen.
		}
	    }
	}
	    
	mb++;	
	out++;
	if (out >= OUTPUT_LIMIT)
	{
	    printf ("%d MB scanned \r", mb);
	    fflush (stdout);
	    out = 0;
	}
	fpos += 1024 * 1024;
    }
    printf ("\n");
    close (fd);
    
    for (int i = 1; i < buffer_file.size(); i++)
    {
	free (buffer_file[i]);
    }
    buffer_file.clear();
    
    LogPrn ("\nDone.");
}
