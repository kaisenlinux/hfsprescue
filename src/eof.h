#ifndef _EXTENTSOVERLFOWFILE_H_
#define _EXTENTSOVERLFOWFILE_H_

#define EXTENTS_OVERFLOW_FILE_NAME "ExtentsOverflowFile"


#define EOF_BLOCK_SIZE	4096	// 0x1000 = 4KB in OSX,  Note MacOS has only a block size of 1KB

#define EOF_MODE_FILE 1
#define EOF_MODE_RAM  2 

#define EOF_KEY_LEN         (2 + 10)
#define EOF_SEARCH_KEY_LEN  8 // We dont use the Start Block for searching, so the key len is only 8.


#define EOF_DATA_ENTRIES    8
#define EOF_DATA_LEN        8

#define EOF_BLOCK_ENTRY_LEN (EOF_KEY_LEN + (EOF_DATA_LEN * EOF_DATA_ENTRIES))

#include "hfsprescue.h"

void FindExtentsOverflowFile (char *file);

bool ExtractExtentsOverflowFileAutomatic (const char *file_name, bool wait_on_warning);

bool ExtractExtentsOverflowFile (int fd, const char *extract_file_name, uint32_t start_block, uint32_t num_blocks);


bool EOF_IsAvailable();
bool EOF_OpenFile (const char *file_name);
void EOF_CloseFile();

void EOF_CreateKey (_FILE_INFO &file_info, uint32_t start_block, unsigned char *out_key);

bool EOF_GetRemainingBlocks (_FILE_INFO &file_info, uint32_t requested_file_block, uint32_t *out_block, uint32_t *out_num_blocks);


void EOF_WarnBlockSizeForce();

#endif