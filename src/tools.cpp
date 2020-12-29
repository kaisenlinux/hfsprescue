/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 */


#include "config.h"

#define _FILE_OFFSE_BITS 64
#define _LARGEFILE64_SOURCE

#include "apple.h"
#include "freebsd.h"

#include "tools.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/ioctl.h> // width/height

#include <inttypes.h>
#include <vector>

#include "utf8mac.h"


extern float scanned;
extern uint64_t fofs;
extern int directories;
extern int files;

extern FILE *pf_log;
extern FILE *pf_folder_table;
extern char destination_directory[];

void Swap16 (uint16_t *val)
{
    uint16_t o;
    uint16_t n;
    
    o = *val;

    n  = (o & 0xff00) >> 8;
    n += (o & 0x00ff) << 8;

    *val = n;    
}

void Swap32 (uint32_t *val)
{
    uint32_t o;
    uint32_t n;
    
    o = *val;

    n  = (o & 0xff000000) >> 24;
    n += (o & 0x00ff0000) >> 8;
    n += (o & 0x0000ff00) << 8;
    n += (o & 0x000000ff) << 24;

    *val = n;    
}


void Swap64 (uint64_t *val)
{
    uint64_t o;
    uint64_t n;
    
    int shift[] = { 56, 40, 24, 8 };
    uint64_t mask = 0xff00000000000000;
    
    o = *val;
    n = 0;
    
    for (int i = 0; i < 4; i++)
    {
	n += (o & mask) >> shift[i];
	mask >>= 8;
    }
    
    for (int i = 3; i >= 0; i--)
    {
	n += (o & mask) << shift[i];
	mask >>= 8;
    }

    *val = n;    
}



uint16_t GetUINT16 (char c1, char c2)
{
    uint16_t ret = c2 & 0xff;
    ret += (c1 & 0xff) << 8;
    
    return ret;
}

uint16_t GetUINT16 (unsigned char c1, unsigned char c2)
{
    uint16_t ret = c2 & 0xff;
    ret += (c1 & 0xff) << 8;
    
    return ret;
}



uint16_t GetUInt16 (char c1, char c2)
{
    uint16_t ret = c2 & 0xff;
    ret += (c1 & 0xff) << 8;
    
    return ret;
}

uint32_t GetUInt32 (char c1, char c2, char c3, char c4)
{
    uint32_t ret = c4 & 0xff;
    ret += (c3 & 0xff) << 8;
    ret += (c2 & 0xff) << 16;
    ret += (c1 & 0xff) << 24;
    
    return ret;
}

uint64_t GetUInt64 (char c1, char c2, char c3, char c4,
 		    char c5, char c6, char c7, char c8)
{
    uint64_t ret = c1 & 0xff;

    ret = ret << 8;
    ret += c2 & 0xff;
    
    ret = ret << 8;
    ret += c3 & 0xff;
    
    ret = ret << 8;
    ret += c4 & 0xff;
    
    ret = ret << 8;
    ret += c5 & 0xff;
    
    ret = ret << 8;
    ret += c6 & 0xff;
    
    ret = ret << 8;
    ret += c7 & 0xff;
    
    ret = ret << 8;
    ret += c8 & 0xff;

    return ret;
}

uint64_t GetUInt64 (char *buffer, int base, int offset)
{
    int p = base + offset;
    return GetUInt64 (buffer[p + 0], buffer[p + 1], buffer[p + 2], buffer[p + 3],
		      buffer[p + 4], buffer[p + 5], buffer[p + 6], buffer[p + 7]); 
}


uint16_t GetUInt16 (char *buffer, int base, int offset)
{
    int p = base + offset;
    return GetUInt16 (buffer[p], buffer[p + 1]);
}

uint32_t GetUInt32 (char *buffer, int base, int offset)
{
    int p = base + offset;
    return GetUInt32 (buffer[p], buffer[p + 1], buffer[p + 2], buffer[p + 3]);
}

uint32_t GetUInt32 (unsigned char *buffer, int base, int offset)
{
    int p = base + offset;
    return GetUInt32 (buffer[p], buffer[p + 1], buffer[p + 2], buffer[p + 3]);
}


bool ConvertFilename (char *in, char *out, unsigned int len)
{
    char c;
    uint16_t val;
    int i_out = 0;
    char tmp_name[256 * 4];
    
    
    if (len > 255)
    {
	// Invalid filename, its too long
	return false;
    }
    
    // do quick check
    for (int i = 0;i < len; i++)
    {
        val = GetUInt16 (in[i * 2], in[i * 2 + 1]);
        if (val < 0x20) return false; // no valid file name

	if (val == '/')	// replace invalid file name char / with :
	{
	    in[i * 2 + 1] = ':';
	}
	
	// swap bytes for utf8_encodestr
	tmp_name[i * 2]     = in[i * 2 + 1];
	tmp_name[i * 2 + 1] = in[i * 2];
    }    

    size_t len_out;
    // convert to UTF-8
    int r = utf8_encodestr((u_int16_t*)(tmp_name ), len * 2,
               (u_int8_t *)out, &len_out,
               256 * 2,
               0, 0
               );

/* Disabled because of file name unicode utf-8 conversion problem
#ifndef __APPLE__
    // swap unicode bytes on non apple systems
    for (int i = 1; i < len_out; i++)
    {
    	if ((out[i] & 0xff) > 0x7f) // unicode
    	{
    	    unsigned char c1 = out[i - 1];
    	    unsigned char c2 = out[i];
    	    unsigned char c3 = out[i + 1];
    	    
    	    out[i - 1] = c2;
    	    out[i]     = c3;
    	    out[i + 1] = c1;
    	    i++;
    	}
    }
#endif
*/    
    return true;
}

// This routine is used for file name print only, on non mac systems.
void FixFileName (char *name)
{
#ifndef __APPLE__
    // swap unicode bytes on non apple systems
    int len = strlen (name);
    for (int i = 1; i < len; i++)
    {    
    	if ((name[i] & 0xff) > 0x7f) // unicode
    	{
    	    unsigned char c1 = name[i - 1];
    	    unsigned char c2 = name[i];
    	    unsigned char c3 = name[i + 1];
    	    
    	    name[i - 1] = c2;
    	    name[i]     = c3;
    	    name[i + 1] = c1;
    	    i++;
    	}
    }
#endif
}


int FileExist (const char *file_name) 
{
    struct stat st;
    
    int result = stat (file_name, &st);
    
    return result == 0;
}


void SplitFileName (const char *file_name, char *out_base, char *out_extension)
{
    int len = strlen (file_name);
    int dot_position = -1;
    
    // find last dot
    for (int i = len - 1; i > 0; i--)
    {
	if (file_name[i] == '.')
	{
	    dot_position = i;
	    break;	
	}
	
	if (file_name[i] == '/')
	{
	    // begin of directory name. file name has no extension
	    break;	
	}	
    }
    
    if (dot_position == -1)
    {
	// no dot found
	strcpy (out_base     , file_name);
	strcpy (out_extension, "");
	return;
    }
    
    // dot found;
    strcpy (out_base, file_name);
    out_base[dot_position] = 0x00;
    strcpy (out_extension, file_name + dot_position);
}

void AlternativeFileName (char *file_name, int id, char *out_file_name)
{
    char new_file_name      [strlen (file_name) + 30];
    char file_name_base     [strlen (file_name) + 30];
    char file_name_extension[strlen (file_name) + 30];

    strcpy (new_file_name, file_name);

    SplitFileName (file_name, file_name_base, file_name_extension);

    int number = 0;
    while (true)
    {
	if (!FileExist (new_file_name))
	{
	    // got new file name
	    strcpy (out_file_name, new_file_name);
	    return;	
	}
	
	if (number == 0)
	{
	    sprintf (new_file_name, "%s[ID%d]%s", file_name_base, id, file_name_extension);
	}
	else
	{
	    sprintf (new_file_name, "%s[ID%d_%d]%s", file_name_base, id, number, file_name_extension);
	}
	
	number++;
	if (number > 100)
	{
	    printf ("[AlternativeFileName] Error: More than 100 equal names. Is here something wrong with %s. \nContact the developer https://www.plop.at\n", file_name);
	    exit (EXIT_FAILURE);	
	}
    }
}


int info_force = 1;
void PrintInfo()
{
    info_force--;
    if (info_force > 0) return;

    int mb = fofs / 1024 / 1024;
    
    if (mb < 10000)
    {
	printf ("\r%0.2f%% scanned (%d MB). Found: %d directories, %d files.",
		scanned,
		mb,
		directories,
		files);    	
    }
    else
    {
	printf ("\r%0.2f%% scanned (%0.2f GB). Found: %d directories, %d files.",
		scanned,
		(float)(fofs) / 1024 / 1024 / 1024,
		directories,
		files);    
    }

    info_force = 5000;
    fflush (stdout);
}

void ForcePrintInfo()
{
    info_force = 1;
    PrintInfo();
}



void RemoveNL (char *txt)
{
    int len = strlen (txt);
    for (int i = 0; i < len; i++)
    {
	if (txt[i] == '\n')
	{
	    txt[i] = 0;
	    return;
	}
    }
}

void CharReplace (char *txt, char old_c, char new_c)
{
    int len = strlen (txt);
    for (int i = 0; i < len; i++)
    {
	if (txt[i] == old_c)
	{
	    txt[i] = new_c;
	}
    }
}

void CharRemove (char *txt, char remove)
{
    int len = strlen (txt);
    int i2 = 0;
    for (int i = 0; i < len; i++)
    {
	if (txt[i] != remove)
	{
	    txt[i2] = txt[i];
	    i2++;
	}
    }
    txt[i2] = 0;
}


void SigProc (int sig)
{
    signal (SIGINT, SigProc);
    
    if (pf_log)
    {
	fclose (pf_log);
    }

    if (pf_folder_table)
    {
	fclose (pf_folder_table);
    }

    exit (EXIT_FAILURE);
}


void MaskFileName (char *in, char *out)
{
    int p = 0;
    for (int i = 0; i < 1024; i++)
    {
	if (in[i] == 0)
	{
	    out[p] = 0;
	    return;
	}
	
	if ((in[i] == '$') || (in[i] == '"') || (in[i] == '\\')) 
	{
	    out[p] = '\\';
	    p++;	
	}
	
	out[p] = in[i];
	p++;    
    }

    out[1023] = 0;
}

int Find (char *buffer, int len, char c)
{
    for (int i = 0; i < len; i++)
	if (buffer[i] == c)
	    return i;
    
    return -1;
}


int HandleArgument (char *argv[], int argc, const char *parameter)
{
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    return 1;
	}    
    }
    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, char *ret_string)
{
    ret_string[0] = 0;
    
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 1 < argc)
	    {
		strcpy (ret_string, argv[i + 1]);
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }
    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, uint64_t *ret)
{
    *ret = 0;
    
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 1 < argc)
	    {
		*ret = strtoull (argv[i + 1], NULL, 10);
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }
    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, int *ret)
{
    *ret = 0;
    
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 1 < argc)
	    {
		*ret = atoi (argv[i + 1]);
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }
    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, uint32_t *ret)
{
    *ret = 0;
    
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 1 < argc)
	    {
		unsigned long ui;
		ui = strtoull (argv[i + 1], NULL, 10);
		*ret = (uint32_t)ui;
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }
    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, char *ret_string, int *ret_int)
{
    ret_string[0] = 0;
    *ret_int      = 0;
    
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 2 < argc)
	    {
		strcpy (ret_string, argv[i + 1]);
		*ret_int = atoi (argv[i + 2]);
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }
    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, char *ret_string, uint64_t *ret_int)
{
    ret_string[0] = 0;
    *ret_int      = 0;
    
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 2 < argc)
	    {
		strcpy (ret_string, argv[i + 1]);
		*ret_int = atoll (argv[i + 2]);
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }
    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, char **ret_string)
{
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 1 < argc)
	    {
		//strcpy (ret_string, argv[i + 1]);
		*ret_string = argv[i + 1];
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }

    return 0;
}

int HandleArgument (char *argv[], int argc, const char *parameter, std::vector <char *> *file_names)
{
    file_names->clear();
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], parameter) == 0)
	{
	    if (i + 1 < argc)
	    {
		for (int i2 = i + 1; i2 < argc; i2++)
		{
		    if (argv[i2][0] == '-')
		    {
			return 1;
		    }
		    
		    int len = strlen (argv[i2]);
		    char *p = NULL;
		    
		    p = (char *)malloc (len + 1);
		    if (!p)
		    {
    			printf ("Error: Allocating memory for file names!\n");
    			exit (EXIT_FAILURE);			
		    }
		    strcpy (p, argv[i2]);
		    file_names->push_back (p);		
		}
		return 1;
	    }
	    else
	    {
		return -1;
	    }	
	}    
    }

    return 0;
}



void LogCommand (FILE *pf, char *argv[], int argc)
{
    fprintf (pf, "Command: ");
    for (int i = 0; i < argc; i++)
    {
	fprintf (pf, "%s ", argv[i]);
    }
    fprintf (pf, "\n\n");
}


bool ValidBlockSize (int bs)
{
    int valid[] = { 512, 1024, 2048, 4096, 8192};

    for (int i = 0; i < 5; i++)
	if (bs == valid[i]) return true;
	
    return false;
}

#define DIRECTORY_PERMISSIONS S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
void MKDir (const char *name)
{
    MKDir (name, false);
}

void MKDir (const char *name, bool allow_exist)
{
    if (FileExist (name))
    {
	return;
    }

    if (mkdir (name, DIRECTORY_PERMISSIONS))
    {
        printf ("Error creating directory %s: %s\n", name, strerror (errno));
        exit (EXIT_FAILURE);
    }
}

int GetScreenHeight()
{
    struct winsize w;

    ioctl (0, TIOCGWINSZ, &w);

    return w.ws_row;
}

int GetScreenWidth()
{
    struct winsize w;

    ioctl (0, TIOCGWINSZ, &w);
    
    return w.ws_col;
}

// Additional file name check. ASCII and UTF-8
bool ValidEncoding (unsigned char *buf, int nbytes, int utf8_max_follow)
{
    // See ftp://ftp.astron.com/pub/file/, encoding.cpp

    // Check for ASCII char
    bool only_ascii = true;
    for (int i = 0; i < nbytes; i++)
    {
	if ((buf[i] < 0x20) || (buf[i] >= 0x7F)) only_ascii = false;
    }

    if (only_ascii) return true;

    // Check for UTF-8 chars
    for (int i = 0; i < nbytes; i++)
    {
	if (buf[i] > 0x7f) // Non ASCII char
	{
	    if ((buf[i] & 0xC0) == 0xC0) // possible UTF-8 char, 11xxxxxx begins UTF-8
	    {
		int following;
    		if ((buf[i] & 0x20) == 0) // 110xxxxx
    		{		
		    following = 1;
		} 
		else if ((buf[i] & 0x10) == 0) // 1110xxxx 
		{	
		    following = 2;
		} 
		else if ((buf[i] & 0x08) == 0) // 11110xxx
		{	
		    following = 3;
		} 
		else if ((buf[i] & 0x04) == 0) // 111110xx
		{	
		    following = 4;
		} 
		else if ((buf[i] & 0x02) == 0) // 1111110x
		{	
		    following = 5;
		} 
		else
		{
		    return false;
		}

		if (following > utf8_max_follow)
		{
		    return false;
		}
			
		i += following; // goto next char
	    }
	    else
	    {
		return false; // wrong encoding
	    }
	}
	else if (buf[i] < 0x20)
	{
	    return false;
	}
    }
    return true; // Test passed, only ASCII or UTF-8 chars
}

void ClearBuffer (char *buffer, int size)
{
    for (int i = 0; i < size; i++)
    {
	buffer[i] = 0;
    }
}

// The buffer must be cleared, before it is filled by a string and then checked reverse by the function.
// Its used to get length of a string, even when it has a 0x00 char inside.
int StrLen2 (char *buffer, int size)
{
    for (int i = size - 1; i >= 0; i--)
    {
	if (buffer[i] != 0x00) 
	{
	    return i + 1;    
	}
    }
    return 0;
}

void Store (const char *file_name, uint64_t value)
{
    FILE *pf;
    char full_name[1024];
    
    sprintf (full_name, "%shfsprescue-data/%s", destination_directory, file_name);
    pf = fopen (full_name, "w");
    if (!pf)
    {
	printf ("Unable to write to '%s': %s\n", full_name, strerror (errno));
        exit (EXIT_FAILURE);
    }
    fprintf (pf, "%" PRIu64, value);
    fclose (pf);
}

void Store (const char *file_name, int value)
{
    FILE *pf;
    char full_name[1024];
    
    sprintf (full_name, "%shfsprescue-data/%s", destination_directory, file_name);
    pf = fopen (full_name, "w");
    if (!pf)
    {
	printf ("Unable to write to '%s': %s\n", full_name, strerror (errno));
        exit (EXIT_FAILURE);
    }
    fprintf (pf, "%d", value);
    fclose (pf);
}

void Store (const char *file_name, const char *string)
{
    FILE *pf;
    char full_name[1024];
    
    sprintf (full_name, "%shfsprescue-data/%s", destination_directory, file_name);
    pf = fopen (full_name, "w");
    if (!pf)
    {
	printf ("Unable to write to '%s': %s\n", full_name, strerror (errno));
        exit (EXIT_FAILURE);
    }
    fprintf (pf, "%s", string);
    fclose (pf);
}

uint64_t LoadUINT64 (const char *file_name)
{
    FILE *pf;
    char full_name[1024];
    char buffer[40];
    
    sprintf (full_name, "%shfsprescue-data/%s", destination_directory, file_name);
    pf = fopen (full_name, "r");
    if (!pf)
    {
	printf ("Unable to open '%s': %s\n", full_name, strerror (errno));
        exit (EXIT_FAILURE);
    }
    fgets (buffer, sizeof (buffer), pf);    
    fclose (pf);

    return strtoull (buffer, NULL, 0);
}

int LoadINT (const char *file_name)
{
    FILE *pf;
    char full_name[1024];
    char buffer[40];
    
    sprintf (full_name, "%shfsprescue-data/%s", destination_directory, file_name);
    pf = fopen (full_name, "r");
    if (!pf)
    {
	printf ("Unable to open '%s': %s\n", full_name, strerror (errno));
        exit (EXIT_FAILURE);
    }
    fgets (buffer, sizeof (buffer), pf);    
    fclose (pf);

    return atoi (buffer);
}

void LoadString (const char *file_name, char *to, int len)
{
    FILE *pf;
    char full_name[1024];
    
    sprintf (full_name, "%shfsprescue-data/%s", destination_directory, file_name);
    pf = fopen (full_name, "r");
    if (!pf)
    {
	printf ("Unable to open '%s': %s\n", full_name, strerror (errno));
        exit (EXIT_FAILURE);
    }
    fgets (to, len, pf);    
    fclose (pf);
}



