#ifndef __HFSPRESCUE_H_
#define __HFSPRESCUE_H_

#include <stdint.h>
#define FILE_NAME_LENGTH (256 * 4)	// Give enough space for decoded UTF-8 string
#define MAX_FILE_NAME_CHARS (4096 + 256) // Usual file name length + path
                        
struct HFSPlusExtentDescriptor
{
    uint32_t	startBlock;
    uint32_t	blockBlock;
};

struct HFSPlusForkData
{
    uint64_t	logicalSize;	// size in bytes
    uint32_t	clumpSize;	
    uint32_t	totalBlocks;	// number of all blocks
    
    HFSPlusExtentDescriptor extents[8];
};

struct HFSPlusVolumeHeader
{
    uint16_t	signature;
    uint16_t	version;
    uint32_t	attributes;
    char   	lastMountedVersion[4];  //uint32_t	lastMountedVersion;
    uint32_t	journalBlockInfo;
    
    uint32_t	createDate;
    uint32_t	modifyDate;
    uint32_t	backupDate;
    uint32_t	checkedDate;
    
    uint32_t	fileCount;
    uint32_t	folderCount;
    
    uint32_t	blockSize;
    uint32_t	totalBlocks;
    uint32_t	freeBlocks;
    
    uint32_t	nextAllocation;
    uint32_t	rsrcClumpSize;
    uint32_t	dataClumpSize;
    uint32_t	nextCataogID;
    
    uint32_t	writeCount;
    uint64_t	encodingsBitmap;
    
    uint32_t	finderInfo[8];
    
    HFSPlusForkData	allocationFile;
    HFSPlusForkData	extentsFile;
    HFSPlusForkData	catalogFile;
    HFSPlusForkData	attributesFile;
    HFSPlusForkData	startupFile;

};


struct HFSPlusBSDInfo
{
    uint32_t	a;
    uint32_t	b;
    
    uint8_t	c;
    uint8_t	d;
    
    uint16_t	e;
    
    uint32_t	f;
    uint32_t	g;
    uint32_t	h;
};



struct HFSPlusCatalogFolder
{
//    uint16_t	recordType;
    uint16_t 	flags;
    uint32_t	valence;
    uint32_t	folder_id;
    uint32_t	createDate;
    uint32_t	contentModDate;
    uint32_t	attributeModDate;
    uint32_t	accessDate;
    uint32_t	backupDate;
    HFSPlusBSDInfo	permissions;
//xx    uint32_t	userInfo;
//	    finderInfo;
    uint32_t	textEncoding;
    uint32_t	reserved;

};


struct _FILE_INFO
{
    char file_name[FILE_NAME_LENGTH];
    uint32_t parent_id;
    uint32_t catalog_id;
    uint32_t number;

    uint64_t file_size;
    uint64_t file_time;

    char date_str[30];

    bool compressed;
    
    uint64_t fork_size;
    uint32_t fork_block;
    uint32_t fork_num_blocks;
 
    uint32_t blocks[8];
    uint32_t num_blocks[8];
    uint32_t catalog_num_blocks;
        
    bool has_extents_overflow;
    int num_extents_data;
     
    bool valid;
    
    bool restored;
    char restored_file_name[4096 * 2];
};
        
#define ALLOW_EXIST true

bool RestoreUncompressedFile (_FILE_INFO &file_info, const char *file_name);

#endif
