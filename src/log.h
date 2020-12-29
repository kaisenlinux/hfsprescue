/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  https://www.plop.at
 *
 */

#ifndef _LOG_H_
#define _LOG_H_


#include "config.h"


#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdarg.h>
#include <ctime>

FILE *LogFileOpen (const char *file_name, const char *mode);
void Log        (const char* format, ...);
void LogNoNL    (const char* format, ...);
void LogPrn     (const char* format, ...);
void LogNoNLPrn (const char* format, ...);
void LogTimePrn (const char* txt, time_t time);
void LogNL      ();
void LogClose   ();
#endif
