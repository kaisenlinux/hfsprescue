#ifndef _VH_H_
#define _VH_H_

#include "hfsprescue.h"

#define VOLUME_HEADER_FILE_NAME "VolumeHeader"


void VH_FindHFSVolumeHeader (char *file, bool verbose, bool stop_after_first, bool force);
void VH_FindHFSAlternateVolumeHeader (char *file, bool verbose, bool stop_after_first, bool force);

void VH_SwapBytes (HFSPlusVolumeHeader *vh);
void VH_PrintData (HFSPlusVolumeHeader *vh);

void VH_Extract (char *file, uint64_t lba, const char *extract_file_name);


#endif