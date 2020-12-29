/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  https://www.plop.at
 *
 */

#ifndef _TOOLS_H_
#define _TOOLS_H_


#include "config.h"


#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <vector>

void Swap16 (uint16_t *val);
void Swap32 (uint32_t *val);
void Swap64 (uint64_t *val);

uint16_t GetUINT16 (char c1, char c2);
uint16_t GetUINT16 (unsigned char c1, unsigned char c2);
uint16_t GetUInt16 (char c1, char c2);
uint32_t GetUInt32 (char c1, char c2, char c3, char c4);

uint64_t GetUInt64 (char c1, char c2, char c3, char c4, char c5, char c6, char c7, char c8);
uint64_t GetUInt64 (char *buffer, int base, int offset);


uint16_t GetUInt16 (char *buffer, int base, int offset);

uint32_t GetUInt32 (char *buffer, int base, int offset);
uint32_t GetUInt32 (unsigned char *buffer, int base, int offset);


bool ConvertFilename (char *in, char *out, unsigned int len);
int FileExist (const char *file_name);
void SplitFileName (const char *file_name, char *out_base, char *out_extension);
void AlternativeFileName (char *file_name, int id, char *out_file_name);


void PrintInfo();
void ForcePrintInfo();

void RemoveNL    (char *txt);
void CharReplace (char *txt, char old_c, char new_c);
void CharRemove  (char *txt, char remove);

void SigProc (int sig);

void MaskFileName (char *in, char *out);

int Find (char *buffer, int len, char c);

int HandleArgument (char *argv[], int argc, const char *parameter);
int HandleArgument (char *argv[], int argc, const char *parameter, char *ret_string);
int HandleArgument (char *argv[], int argc, const char *parameter, uint64_t *ret);
int HandleArgument (char *argv[], int argc, const char *parameter, int *ret);
int HandleArgument (char *argv[], int argc, const char *parameter, uint32_t *ret);
int HandleArgument (char *argv[], int argc, const char *parameter, char *ret_string, int *ret_int);
int HandleArgument (char *argv[], int argc, const char *parameter, char *ret_string, uint64_t *ret_int);
int HandleArgument (char *argv[], int argc, const char *parameter, char **ret_string);
int HandleArgument (char *argv[], int argc, const char *parameter, std::vector <char *> *file_names);



void LogCommand (FILE *pf, char *argv[], int argc);

bool ValidBlockSize (int bs);

void MKDir (const char *name);
void MKDir (const char *name, bool allow_exist);

int GetScreenHeight();
int GetScreenWidth();

void FixFileName (char *name);

bool ValidEncoding (unsigned char *buf, int nbytes, int utf8_max_follow);

void ClearBuffer (char *buffer, int size);
int  StrLen2 (char *buffer, int size);

void Store (const char *file_name, uint64_t value);
void Store (const char *file_name, int value);
void Store (const char *file_name, const char *string);
uint64_t LoadUINT64 (const char *file_name);
int  LoadINT (const char *file_name);
void LoadString (const char *file_name, char *to, int len);

#endif
