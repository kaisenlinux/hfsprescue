/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  https://www.plop.at
 *
 * Extents Overflow File Routines
 * 
 */


#define _FILE_OFFSE_BITS 64
#define _LARGEFILE64_SOURCE

#include "config.h"

#include "apple.h"
#include "freebsd.h"

#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "eof.h"
#include "tools.h"
#include "log.h"



int eof_mode;
FILE *pf_eof;

unsigned char *eof_mem = NULL;
uint64_t eof_mem_size;
uint64_t eof_mem_pointer = 0;


bool eof_is_available = false;

const char eof_magic[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x03 };

extern HFSPlusVolumeHeader *vh;
extern bool force_block_size;
extern bool force_eof;
extern uint64_t offset;
extern char destination_directory[];

// Search possible starts of the ExtentsOverflowFile on the partition.
void FindExtentsOverflowFile (char *file)
{
    
    char buffer[sizeof (eof_magic)];

    uint64_t fpos = offset;
    int fd;
    uint64_t scanned = 0;
    int output = 0;
    int num_found = 0;
    
    if (offset > 0)
    {
        LogPrn ("Using offset %" PRIu64 " calcuation base. Starting search from the offset (= %" PRIu64 " MB).", offset, offset / 1024 / 1024);
    }
    
    
    fd = open64 (file, O_RDONLY);
    if (fd < 0)
    {
	fprintf (stderr, "Unable to open '%s': %s\n", file, strerror (errno));
	exit (EXIT_FAILURE);
    }
    
    printf ("Searching block positions of the Extents Overflow File...\n\n");

    while (pread64 (fd, buffer, sizeof (buffer), fpos))
    {
	bool found = true;
	for (int i = 0; i < sizeof (eof_magic); i++)
	{
	    if (eof_magic[i] != buffer[i])
	    {
		found = false;
		break;
	    }
	}	
	if (found)
	{
	    num_found++;
	    LogPrn ("%d. Possible block: %" PRIu64 " | File position: 0x%llx    %s", num_found, (fpos - offset) / vh->blockSize, fpos, (num_found == 2) ? "maybe ExtentsOverflowFile or CatalogFile" : "");
	}

	scanned += vh->blockSize;
	fpos += vh->blockSize;
	
	output++;
	if (output > 1000)
	{
	    printf ("%" PRIu64 " MB scanned \r", scanned / 1024 / 1024);
	    fflush (stdout);
	    output = 0;
	}
    }
    printf ("Done.                 \n");
    close (fd);
}

bool ExtractExtentsOverflowFileAutomatic (const char *file_name, bool wait_on_warning)
{
    bool invalid = false;
    bool zero = false;
    char restored_dir[MAX_FILE_NAME_CHARS];
    char default_file_name[MAX_FILE_NAME_CHARS];

    sprintf (restored_dir, "%srestored", destination_directory);
    sprintf (default_file_name, "%srestored/%s", destination_directory, EXTENTS_OVERFLOW_FILE_NAME);
    

    if (file_name == NULL)
    {
        file_name = default_file_name;

        MKDir (restored_dir , ALLOW_EXIST);
    }
    
    LogPrn ("Extracting the ExtentsOverflowFile to '%s'.\n", file_name);
    
    struct stat st;
    if (stat (file_name, &st) == 0)
    {
	LogPrn ("*** The ExtentsOverflowFile exists. Skipping extracting.");
	return true; // Return true to not interrupt user file restoring
    }

    if (force_block_size)
    {
        EOF_WarnBlockSizeForce ();
        if (wait_on_warning)
        {
    	    sleep (10);
        }
        return false;
    }

    LogNoNLPrn ("Size: %" PRIu64 " bytes", vh->extentsFile.logicalSize);
    if (vh->extentsFile.logicalSize > 1024 * 1024 * 1024) LogNoNLPrn (", %0.2f GB", (float)vh->extentsFile.logicalSize / (1024 * 1024 * 1024));
    else if (vh->extentsFile.logicalSize > 1024 * 1024)   LogNoNLPrn (", %0.2f MB", (float)vh->extentsFile.logicalSize / (1024 * 1024));
    else if (vh->extentsFile.logicalSize > 1024)          LogNoNLPrn (", %0.2f KB", (float)vh->extentsFile.logicalSize / 1024);

    LogPrn ("\nClump Size: %" PRIu32 " bytes", vh->extentsFile.clumpSize);
    LogPrn ("Total Blocks: %" PRIu32, vh->extentsFile.totalBlocks);

    for (int i = 0; i < 8; i++)
    {
	LogPrn ("Extent %d: Start %" PRIu32 ", Num %" PRIu32, i, vh->extentsFile.extents[i].startBlock, vh->extentsFile.extents[i].blockBlock);

	// Do vality checks
	if ((vh->extentsFile.extents[i].blockBlock > 0) && (vh->extentsFile.extents[i].startBlock == 0))
	{
	    invalid = true;
	}

	if ((vh->extentsFile.extents[i].startBlock > 0) && (vh->extentsFile.extents[i].blockBlock == 0))
	{
	    invalid = true;
	}
	
	if (vh->extentsFile.extents[i].startBlock == 0)
	{
	    bool zero = true;
	}
	
	if (zero && ((vh->extentsFile.extents[i].startBlock != 0) || (vh->extentsFile.extents[i].blockBlock != 0)))
	{
	    invalid = true;
	}	
    }

    if (invalid)
    {
	LogPrn ("*** Error: Invalid extents entry. Skipping extracting.");
	return false;    
    }


    _FILE_INFO file_info;
    file_info.file_size = vh->extentsFile.logicalSize;
    file_info.has_extents_overflow = false;
    
    for (int i = 0; i < 8; i++)
    {
        file_info.blocks[i]     = vh->extentsFile.extents[i].startBlock;
        file_info.num_blocks[i] = vh->extentsFile.extents[i].blockBlock;
    }

    LogPrn ("");

    if (RestoreUncompressedFile (file_info, file_name))
    {
	LogPrn ("File created.");
	return true;
    }
    else
    {
	LogPrn ("Error: File not created!");
	return false;
    }
}





bool EOF_IsAvailable()
{
    return eof_is_available;
}



bool EOF_OpenFile (const char *file_name)
{
    char header[sizeof (eof_magic)];
    char default_file_name[MAX_FILE_NAME_CHARS];

    sprintf (default_file_name, "%srestored/%s", destination_directory, EXTENTS_OVERFLOW_FILE_NAME);
    
    eof_mode = EOF_MODE_FILE;

    if (file_name == NULL)
    {
        file_name = default_file_name;
    }
    
    
    // Do simple validate checks.
    pf_eof = fopen (file_name, "r");
    if (!pf_eof)
    {
	LogPrn ("Unable to open '%s': %s", file_name, strerror (errno));
	return false;
    }
    
    if (fread (header, sizeof (header), 1, pf_eof) != 1)
    {
	LogPrn ("Invalid ExtentsOverflowFile: Unable to read header from '%s'.", file_name);
	return false;    
    }

    if (memcmp (header, eof_magic, sizeof (header)) != 0)
    {
	LogPrn ("Invalid ExtentsOverflowFile: Invalid header in '%s'.\n", file_name);
	if (force_eof)
	{
	    LogPrn ("*** Force using ExtentsOverflowFile");
	}
	else
	{
	    LogPrn ("You have the following options:");
	    LogPrn ("1) Remove the ExtentsOverflowFile '%s'.", file_name);
	    LogPrn ("2) Let the ExtentsOverflowFile extract automatically (during -s3 or --one-file).");
	    LogPrn ("3) Extract the ExtentsOverflowFile with '--extract-eof'.");
	    LogPrn ("4) Skip use of the ExtentsOverflowFile with '--ignore-eof'. Heavy fragmented files will not be restored.");
	    LogPrn ("5) Use '--force-eof' to use your invalid file.");
	
	    return false;        
	}
    }
    
    struct stat st;
    if (stat (file_name, &st) == 0)
    {
	LogPrn ("Allocating %" PRIu64 " MB for the ExtentsOverflowFile.", st.st_size / 1024 / 1024 + 1);
	eof_mem = (unsigned char *)calloc (st.st_size, 1);
	if (eof_mem)
	{
	    eof_mem_size = st.st_size;
	    fseek (pf_eof, SEEK_SET, 0);
	    if (fread (eof_mem, st.st_size, 1, pf_eof) == 1)
	    {
		eof_mode = EOF_MODE_RAM;
		fclose (pf_eof);
		pf_eof = NULL;
		LogPrn ("'%s' loaded into RAM. :)", file_name);
	    }
	    else	
	    {
		free (eof_mem);
		LogPrn ("Unable to load the ExtentsOverflowFile to the RAM. Using file operations for the ExtentsOverflowFile.");
	    }	    
	}	
	else	
	{
	    LogPrn ("Unable to allocate RAM. Using file operations for the ExtentsOverflowFile.");
	}
    }
    else
    {
	LogPrn ("Unable to get file informations of %s.", file_name);    
    }
    eof_is_available = true;

    if (eof_mode == EOF_MODE_FILE)
    {
	LogPrn ("File based working with EOF is currently not supported. You need enough RAM to load the EOF file into the RAM.");
	return false;
    }
    
    LogPrn ("");
    return true;
}


void EOF_CloseFile()
{
    if (pf_eof)
    {
	fclose (pf_eof);
	pf_eof = NULL;
    }
    
    if (eof_mem)
    {
	free (eof_mem);
	eof_mem = NULL;
    }
    
    eof_is_available = false;
}

void EOF_CreateKey (_FILE_INFO &file_info, uint32_t start_block, unsigned char *out_key)
{
    // Key length (10)
    out_key[ 0] = 0;	// Key length high
    out_key[ 1] = 0x0A; // Key length low

    // Key start
    //==========    
    // Fork type
    out_key[ 2] = 0;
    
    // Pad
    out_key[ 3] = 0;

    // File ID
    out_key[ 4] = (file_info.catalog_id >> 24) & 0xff;
    out_key[ 5] = (file_info.catalog_id >> 16) & 0xff;
    out_key[ 6] = (file_info.catalog_id >>  8) & 0xff;
    out_key[ 7] =  file_info.catalog_id        & 0xff;

    // Start block
    out_key[ 8] = (start_block >> 24) & 0xff;
    out_key[ 9] = (start_block >> 16) & 0xff;
    out_key[10] = (start_block >>  8) & 0xff;
    out_key[11] =  start_block        & 0xff;
}



// Find the data set that has the requested block number of the file (not the file system block number)
// Then calculate the file system block and remaining num_blocks
//
//   Set 1: Start block file 10, FS block 505, Num 10
//   Set 2: Start block file 20, FS block 1003, Num 5
//
// Example 1: Requested file block is 20
//   Set 2 is correct, return FS block, 1003, num_blocks 5
//
// Example 2: Requested file block is 22
//   Set 2 is correct, return FS block, 1005, num_blocks 3
//

bool EOF_GetRemainingBlocks (_FILE_INFO &file_info, uint32_t requested_file_block, uint32_t *out_block, uint32_t *out_num_blocks)
{
    uint32_t eof_block_start;
    unsigned char key[EOF_KEY_LEN];
    bool found_key = false;
    EOF_CreateKey (file_info, 0, key);


    for (eof_block_start = 0; eof_block_start < eof_mem_size; eof_block_start += EOF_BLOCK_SIZE)
    {
	if (eof_mem[eof_block_start + 9] != 0x01) continue; // Only check blocks of type 0x01

	uint64_t entry_start;	
	entry_start = 0x0E; // First entry starts at 0x0E
	
	// Scan the EOF block for the key	
	while (entry_start <= EOF_BLOCK_SIZE - EOF_BLOCK_ENTRY_LEN)
	{
	    uint64_t mem_pointer = 0;
	    mem_pointer = eof_block_start + entry_start;

	    if (memcmp (eof_mem + mem_pointer, key, EOF_SEARCH_KEY_LEN) == 0)
	    {
		found_key = true;
		
		int start_block;
		start_block = GetUInt32 (eof_mem + mem_pointer, EOF_SEARCH_KEY_LEN, 0);	    	    
	    
		// Check entries if requested block is in range
		for (int i = 0; i < EOF_DATA_ENTRIES; i++)
		{
		    int block;
		    int num_blocks;
		
		    block      = GetUInt32 (eof_mem + mem_pointer, EOF_KEY_LEN, 0 + i * EOF_DATA_LEN);
		    num_blocks = GetUInt32 (eof_mem + mem_pointer, EOF_KEY_LEN, 4 + i * EOF_DATA_LEN);
		    
		    if ((block == 0) || (num_blocks == 0)) break; // Break when no more data exists
	    
		    if ((requested_file_block == start_block) || ((requested_file_block >= start_block) && (requested_file_block < start_block + num_blocks)))
		    //if (((requested_file_block >= start_block) && (requested_file_block < start_block + num_blocks)))
		    {
			// Requested block is in range
			// Calclate the file system block and the remaining num_blocks
			uint32_t diff;
			diff = requested_file_block - start_block;
		
			*out_block      = block + diff;
			*out_num_blocks = num_blocks - diff;
		    
			LogNoNL ("ExtentsOverflow: Requested file block 0x%04x (%" PRIu32 ") found. Search key:", requested_file_block, requested_file_block);
			for (int i2 = 0; i2 < EOF_SEARCH_KEY_LEN; i2++)
			{
			    LogNoNL (" %02x", key[i2]);
			}
	
			Log (". File offset 0x%08x (%" PRIu64 "). Data set %d. Return block 0x%04x (%" PRIu32 "), num_blocks 0x%04x (%" PRIu32 ").", 
				mem_pointer, mem_pointer, 
				i,
				*out_block, *out_block,
				*out_num_blocks, *out_num_blocks				
				);		    
			return true;
		    }
		    start_block += num_blocks;
		} // for (int i = 0; i < EOF_DATA_ENTRIES; i++)
	    }		
	    entry_start += EOF_BLOCK_ENTRY_LEN;
	} // while (entry_start <= EOF_BLOCK_SIZE - EOF_BLOCK_ENTRY_LEN)
    }

    LogNoNL ("Error: ExtentsOverflow Search. Key:");
    for (int i = 0; i < EOF_SEARCH_KEY_LEN; i++)
    {
	LogNoNL (" %02x", key[i]);
    }
    LogNoNL (". Requested file block 0x%04x (%" PRIu32 "). Result: ", requested_file_block, requested_file_block);
    
    if (!found_key)
    {
	Log ("Key not found!");
    }
    else
    {
	Log ("Requested file block not found!");
    }
    
    return false;
}


void EOF_WarnBlockSizeForce ()
{
    LogPrn ("You are forcing the block size. I assume, the Volume Header is not correct.\n"
            "Automatic extracting of the ExtentsOverflowFile has been disabled! This means\n"
            "strong fragmented files will not be restored! When you want to use the\n"
            "ExtentsOverflowFile, then you have to restore it manually with '--extract-eof'.\n"
            "Use '--ignore-eof' to restore without ExtentsOverFlowFile.\n");
}


/*
--extract-eof

--eof-file
-o
-b
--first-block
--last-block
--num-blocks
--use-vh

*/

bool ExtractExtentsOverflowFile (int fd, const char *extract_file_name, uint32_t start_block, uint32_t num_blocks)
{
    uint64_t fpos;
    unsigned char buffer[vh->blockSize];
    FILE *pf;
    char restored_dir[MAX_FILE_NAME_CHARS];
    char default_file_name[MAX_FILE_NAME_CHARS];


    sprintf (restored_dir, "%srestored", destination_directory);
    sprintf (default_file_name, "%srestored/%s", destination_directory, EXTENTS_OVERFLOW_FILE_NAME);


    if (extract_file_name == NULL)
    {
        extract_file_name = default_file_name;

        MKDir (restored_dir , ALLOW_EXIST);
    }
    
    LogPrn ("Extracting the ExtentsOverflowFile to '%s'.", extract_file_name);
    
    struct stat st;
    if (stat (extract_file_name, &st) == 0)
    {
	LogPrn ("Unable to create '%s': The file already exists.", extract_file_name);
	return false;
    }
    
    pf = fopen (extract_file_name, "w");
    if (!pf)
    {
	LogPrn ("Unable to create '%s': %s", extract_file_name, strerror (errno));
	return false;
    }

    fpos = offset;
    fpos += (uint64_t)start_block * (uint64_t)vh->blockSize;

    
    LogPrn ("*** Warning. No extents of the ExtentsOverflowFile will be restored.");
    LogPrn ("Extracting blocks from %" PRIu32 " to %" PRIu32 ". %" PRIu32 " blocks.", start_block, start_block + num_blocks - 1, num_blocks);

    for (uint32_t i = 0; i < num_blocks; i++)
    {
	pread64 (fd, buffer, sizeof (buffer), fpos);
	fwrite (buffer, sizeof (buffer), 1, pf);
	fpos += (uint64_t)vh->blockSize;
    }

    fclose (pf);

    LogPrn ("File created.");
    return true;
}

