/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 */


#include "config.h"

#include "log.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

extern FILE *pf_log;

char log_file_name[1024];

FILE *LogFileOpen (const char *file_name, const char *mode)
{
    strcpy (log_file_name, file_name);
    return fopen (file_name, mode);
}

void Log (const char* format, ...)
{
    va_list args;

    va_start (args, format);
    vfprintf (pf_log, format, args);
    va_end (args);
    
    fprintf (pf_log, "\n");
}

void LogNoNL (const char* format, ...)
{
    va_list args;

    va_start (args, format);
    vfprintf (pf_log, format, args);
    va_end (args);
}

void LogPrn (const char* format, ...)
{
    va_list args;

    va_start (args, format);
    vfprintf (pf_log, format, args);
    va_end (args);

    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    
    fprintf (pf_log, "\n");
    printf ("\n");
    
}

void LogNoNLPrn (const char* format, ...)
{
    va_list args;

    va_start (args, format);
    vfprintf (pf_log, format, args);
    va_end (args);

    va_start (args, format);
    vprintf (format, args);
    va_end (args);
}

void LogTimePrn (const char* txt, time_t t)
{
    //time_t t = time (0);   // Get time now
    struct tm *now = localtime (&t);
    
    fprintf (pf_log, "%s%4d/%02d/%02d %02d:%02d:%02d\n", txt,
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
		now->tm_hour, now->tm_min, now->tm_sec);

    printf ("%s%4d/%02d/%02d %02d:%02d:%02d\n", txt,
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
		now->tm_hour, now->tm_min, now->tm_sec);    
}

void LogNL()
{
    fprintf (pf_log, "\n");
}

void LogClose()
{
    if (pf_log)
    {
        LogNL();
        fclose (pf_log);
    }
}