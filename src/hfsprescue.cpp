/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 * I know, the program is not perfect, but it will do its job and restore
 * the most files and directories.
 *
 */


#include "config.h"

#define _FILE_OFFSE_BITS 64
#define _LARGEFILE64_SOURCE

#include "apple.h"
#include "freebsd.h"

#include <inttypes.h>


#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif


#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <utime.h>
#include <vector>
#include <errno.h>

#ifdef __APPLE__
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#  define SHA1 CC_SHA1
#else
#  include <openssl/sha.h>
#endif



#include "hfsprescue.h"
#include "bytescan.h"
#include "eof.h"
#include "vh.h"
#include "tools.h"
#include "log.h"
#include "decompress.h"
#include "ioctl.h"
#include "help.h"
#include "rmdir.h"


#define HEADER "hfsprescue " PACKAGE_VERSION " by Elmar Hanlhofer https://www.plop.at\n\n"

#define FILESFOUND_TXTDB  "filesfound.db"
#define FILEINFO_TXTDB    "fileinfo.db"
#define FILEINFO_SHA      "fileinfo.sha"
#define FOLDERTABLE_TXTDB "foldertable.db"

#define FILE_INFO_BLOCK_NUM_LINES 50

//#define DIRECTORY_PERMISSIONS S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
#define SEPARATION_LINE "================================================================================\n"
//#define ALLOW_EXIST true



#define RUN_STEP1		1
#define RUN_STEP2		2
#define RUN_STEP3		3
#define RUN_STEP4		4
#define RUN_STEP5		5
#define RUN_STEP6		6

#define RUN_BYTE_SCAN		    	  100
#define RUN_LIST		    	  101
#define RUN_CSV_EXPORT		    	  102
#define RUN_RESTORE_ONE_FILE	    	  103
#define RUN_FIND_EXTENTSOVERFLOWFILE 	  104
#define RUN_FIND_VOLUME_HEADER	    	  105
#define RUN_FIND_ALTERNATE_VOLUME_HEADER  106
#define RUN_EXTRACT_EXTENTSOVERFLOWFILE   107
#define RUN_EXTRACT_VOLUME_HEADER         108
#define RUN_REMOVE_EMPTY_DIRECTORIES      109


#define PATH_MAX_LENGTH 1024 * 20


char recordType[4][14] = { "Folder", "File", "FolderThread", "FileThread" };

int fd;
uint64_t fofs;

bool param_alternative_file_name = false;

bool info_alternative = false;

FILE *pf_log;
FILE *pf_folder_table;
FILE *pf_file_info;

time_t time_start;
time_t time_end;

int directories = 0;
int files = 0;
float scanned = 0;
bool slash_convert = false; // Just for listing and csv export, its only a help for users.
bool invalid_skipped = false;

bool ignore_blocks             = false;
bool ignore_file_create_error  = false;
bool file_create_error_skipped = false;

int  utf8_len; // Setup later, default for non asian chars in file names is 1.
bool force_utf8_len = false;

uint32_t future_date_tolerance; // Setup later, default is one week
bool force_future_date = false;


uint64_t fs_size;
uint64_t offset;

bool ignore_eof = false;
bool force_eof  = false;
char *eof_file  = NULL;
char *file_list = NULL;
bool is_file_list_csv = false;

bool force_block_size = false;

HFSPlusVolumeHeader *vh;
uint32_t block_size;

int dots = 0;
extern char log_file_name[1024];
char destination_directory[PATH_MAX_LENGTH];
char data_directory[PATH_MAX_LENGTH];

//===========================================


void ShowTime()
{
    time (&time_end);
    LogTimePrn ("\nEnd: ", time_end);

    double diff;
    diff = difftime (time_end, time_start);

    if (diff < 60)
    {
	LogPrn ("Elapsed time: %.2lf seconds.", diff);
    }
    else if (diff < 60 * 60)
    {    
	LogPrn ("Elapsed time: %.2lf minutes.", diff / 60);
    }
    else
    {
	int hours   = (int)diff / 60 / 60;
	int minutes = (int)(diff / 60) % 60;
	LogPrn ("Elapsed time: %d hour%s, %d minute%s.", hours, (hours != 1) ? "s" : "", minutes, (minutes != 1) ? "s" : "");
    }
}


//=================================
// STEP 1
//
// Scan disk for valid files and directories
//=================================

#define OFS1_KEY_LEN		0
#define OFS1_PARENT_ID		2
#define OFS1_FILENAME_LEN	6
#define OFS1_FILENAME		8

#define START_OFS2		8
#define OFS2_TYPE			0x00
#define OFS2_CATALOG_ID			0x08
#define OFS2_TIME_CREATED		0x0C
#define OFS2_TIME_CONTENT_MODIFIED	0x10
#define OFS2_TIME_ATTRIBUTES_MODIFIED	0x14
#define OFS2_TIME_ACCESSED		0x18
#define OFS2_TIME_BACKEDUP		0x1c
#define OFS2_UID			0x20
#define OFS2_GID			0x24
#define OFS2_COMPRESSED			0x29
#define OFS2_FILE_TYPE			0x30
#define OFS2_FILE_CREATOR		0x34
#define OFS2_FILESIZE			0x58
#define OFS2_EXTENDS			0x68
#define OFS2_FORK_SIZE			0xA8
#define OFS2_FORK_BLOCK			0xB8
#define OFS2_FORK_NUM_BLOCKS		0xBC


void WriteFileInfos (char *file_name,
	    	     char *buffer,
	    	     unsigned int base,
	    	     unsigned int parent_id,
	    	     unsigned int block_entry,
	    	     unsigned int catalog_id
	    	    )
{

    uint64_t file_size;
    uint32_t file_date;
    uint32_t uid;
    uint32_t gid;
    uint32_t file_type;
    uint32_t file_creator;
    uint64_t fork_size;
    uint32_t fork_block;
    uint32_t fork_num_blocks;
    bool compressed;
    char date_str[256];

    uint32_t block;
    uint32_t num;
    uint32_t ext;
    
    int ofs;

    file_size       = GetUInt64 (buffer, base, OFS2_FILESIZE);
    file_date       = GetUInt32 (buffer, base, OFS2_TIME_CONTENT_MODIFIED);
    uid             = GetUInt32 (buffer, base, OFS2_UID);
    gid             = GetUInt32 (buffer, base, OFS2_GID);
    file_type       = GetUInt32 (buffer, base, OFS2_FILE_TYPE);
    file_creator    = GetUInt32 (buffer, base, OFS2_FILE_CREATOR);
    fork_size       = GetUInt64 (buffer, base, OFS2_FORK_SIZE);
    fork_block      = GetUInt32 (buffer, base, OFS2_FORK_BLOCK);
    fork_num_blocks = GetUInt32 (buffer, base, OFS2_FORK_NUM_BLOCKS);
    compressed      = (buffer[base + OFS2_COMPRESSED] & 0x20) ? true : false;

    LogNoNL (" | Filesize: %" PRIu64 " (ofs 0x%02x) | ", file_size, base + OFS2_FILESIZE);

    time_t rawtime = file_date - 2082844800; // linux epoch date 1970, mac 1900, substract correction seconds
    sprintf (date_str, "%s", ctime (&rawtime));
    RemoveNL (date_str);

    fprintf (pf_file_info, "%s\n",                  file_name);
    fprintf (pf_file_info, "Number: %" PRIu32 "\n",  files + 1);
    fprintf (pf_file_info, "Catalog ID: %" PRIu32 "\n",      catalog_id);
    fprintf (pf_file_info, "File size: %" PRIu64 "\n",     file_size);
    fprintf (pf_file_info, "Parent ID: %" PRIu32 "\n",       parent_id);
    fprintf (pf_file_info, "Date: %s\n",            date_str);
    fprintf (pf_file_info, "Rawtime: %llu\n", (unsigned long long)rawtime);
    fprintf (pf_file_info, "UID:     %" PRIu32 "\n",         uid);
    fprintf (pf_file_info, "GID:     %" PRIu32 "\n",         gid);
    fprintf (pf_file_info, "Type:    0x%08x\n",     file_type);
    fprintf (pf_file_info, "Creator: 0x%08x\n",     file_creator);
    fprintf (pf_file_info, "Compressed: %s\n",      compressed ? "Yes" : "No");
    fprintf (pf_file_info, "Fork size: %" PRIu64 "\n",     fork_size);
    fprintf (pf_file_info, "Fork block: %" PRIu32 "\n",      fork_block);
    fprintf (pf_file_info, "Fork num blocks: %" PRIu32 "\n", fork_num_blocks);
    fprintf (pf_file_info, "Entry: Block %u, Base offset: 0x%04x\n", block_entry, base);
    fprintf (pf_file_info, "#--------\n");

    LogNoNL ("Extend offsets: ");

    ofs = base  + OFS2_EXTENDS;
    for (ext = 0; ext < 8; ext++)
    {
	LogNoNL ("0x%04x ", ofs);
	
        block = GetUInt32 (buffer[ofs], buffer[ofs + 1], buffer[ofs + 2], buffer[ofs + 3]);
	fprintf (pf_file_info, "block (ofs: 0x%04x): 0x%08x\n", ofs, block);

	ofs += 4;
	num = GetUInt32 (buffer[ofs], buffer[ofs + 1], buffer[ofs + 2], buffer[ofs + 3]);
	fprintf (pf_file_info, "num (ofs: 0x%04x): 0x%08x\n", ofs, num);

	ofs += 4;
	fprintf (pf_file_info, "block: %" PRIu32 ", num: %" PRIu32 "\n#--------\n", block, num);
    }
    fprintf (pf_file_info, "####################\n");

    
}	      
	      

void ScanBuffer (char *buffer, unsigned int ofs, uint32_t block)
{
    uint16_t key_len;
    uint16_t type;
    uint16_t filename_len;
    char file_name[256 * 4];

    uint32_t parent_id;
    uint32_t catalog_id;
    bool found;
    unsigned int ofs2;
    unsigned int old_ofs;

    bool valid_file_name;
    
    
    while (ofs < block_size)
    {    
	found = false;
	old_ofs = ofs; // For endless loop check
	
	while ((ofs < block_size) && (!found))
	{
	    key_len      = GetUInt16 (buffer[ofs], buffer[ofs + 1]);
	    filename_len = GetUInt16 (buffer[ofs + OFS1_FILENAME_LEN], buffer[ofs + OFS1_FILENAME_LEN + 1]);

	    // check if it could be a valid entry
	    if ((key_len == filename_len * 2 + 6) && (key_len > 1) && 
	        (filename_len > 0) && (filename_len <= 256))
	    {
		ofs2 = ofs + filename_len * 2 + START_OFS2;
		type = GetUInt16 (buffer[ofs2], buffer[ofs2 + 1]);

	        if ((type > 0) && (type < 5))
	        {
	    	    found = true;
	        }		
	    }
	    
	    
	    if (!found)
	    {
		ofs++;
	    }	
	}
	
	if (found)
	{
	    uint16_t start_ofs = ofs;
	    uint64_t sector = fofs >> 9;

	    LogNoNL ("block %" PRIu32 ", sector %" PRIu64 ", ofs 0x%04x | ", block, sector, ofs);
	    
	    key_len = GetUInt16 (buffer, start_ofs, OFS1_KEY_LEN);

	    LogNoNL (" key len %" PRIu16 " | ", key_len);	    
	    
	    parent_id       = GetUInt32 (buffer, start_ofs, OFS1_PARENT_ID);
	    filename_len    = GetUInt16 (buffer, start_ofs, OFS1_FILENAME_LEN);
	    valid_file_name = ConvertFilename (buffer + start_ofs + OFS1_FILENAME, file_name, filename_len);

	    unsigned int start_ofs2;
	    	    
	    if (valid_file_name)
	    {
		start_ofs2 = start_ofs + filename_len * 2 + START_OFS2;
		
		type = GetUInt16 (buffer, start_ofs2, OFS2_TYPE);
		catalog_id = GetUInt32 (buffer, start_ofs2, OFS2_CATALOG_ID);
		
		LogNoNL ("%s | ", file_name);
		LogNoNL ("RecordType: 0x%04x, %s | Catalog ID: %" PRIu32 " | Parent ID: %" PRIu32 " ", type, recordType[type - 1], catalog_id, parent_id);

		if (type == 1)
		{		
    		    LogNoNL ("\nFolderinfo: %" PRIu32 "|%" PRIu32 "|%s", catalog_id, parent_id, file_name);

    		    fprintf (pf_folder_table, "%" PRIu32 "|%" PRIu32 "|%s\n", catalog_id, parent_id, file_name);
	    
		    directories++;
		} 
		else if (type == 2)
		{		    
		    WriteFileInfos (file_name, buffer, start_ofs2, parent_id, block, catalog_id);

		    LogNoNL ("| Buffer: ");
		    
		    for (int i = 0; i < 0x108 + filename_len * 2; i++)
		    {
			LogNoNL ("%02x ", buffer[start_ofs + i] & 0xff);
		    }
		    files++;
		} 
		Log (""); // make a new line in the log file
	    }
	    else
	    {
		// In case of invalid file name just skip 2 bytes
		start_ofs2 = start_ofs + 2;
	    }
	    ofs = start_ofs2;
	}
	PrintInfo();
	
	if (ofs == old_ofs)
	{
	    // We are in an endless loop, increase ofs
	    Log ("Warning: Endless loop, %d", ofs);
	    ofs++;
	}
    }
}



char cache_mem[1024 * 1024 * 100];	// 100MB
uint64_t cache_pointer;
uint64_t cache_size;
uint64_t cache_file_offset;


bool FillCache (int fd, uint64_t file_offset)
{
    if (! (cache_size = pread64 (fd, cache_mem, sizeof (cache_mem), file_offset)))
    {
	//printf ("\n[FillCache] Debug: File end reached\n");
	return false;
    }

    cache_file_offset = file_offset;
    return true;
}



bool GetCacheBuffer (int fd, char *dest_buffer, int i_num_bytes, uint64_t file_offset)
{
    uint64_t num_bytes = (uint64_t)i_num_bytes;

    if ((cache_file_offset + cache_size <= file_offset) ||
	(cache_file_offset > file_offset))
    {
	if (!FillCache (fd, file_offset))
	    return false;
    }

    cache_pointer = file_offset - cache_file_offset;

    if (cache_pointer + num_bytes < cache_size)
    {
	memcpy (dest_buffer, cache_mem + cache_pointer, num_bytes);
	return true;
    }
    

    while (true)
    {
	uint64_t bytes_left = cache_size - cache_pointer;

	if (bytes_left > 0)
	{
	    memcpy (dest_buffer, cache_mem + cache_pointer, bytes_left);
	    num_bytes -= bytes_left;

	    file_offset += bytes_left;	
	}
	
	if (num_bytes <= 0)
	    return true;

        if ((cache_file_offset + cache_size <= file_offset) ||
	    (cache_file_offset > file_offset))
	{
	    if (!FillCache (fd, file_offset))
		return false;
	}

	cache_pointer = file_offset - cache_file_offset;

	if (cache_pointer + num_bytes < cache_size)
	{
	    memcpy (dest_buffer, cache_mem + cache_pointer, num_bytes);
	    return true;
	}	    
    }
}


int FindFiles (bool force, HFSPlusVolumeHeader *vh, uint64_t file_size)
{

    char *buffer;
    char tmp_name[PATH_MAX_LENGTH];
    
    if (!force)
    {
	bool stop = false;

	sprintf (tmp_name, "%s%s", data_directory, FOLDERTABLE_TXTDB);
	if (FileExist (tmp_name))
	{
	    printf ("Error: File %s exists. Delete the file or use '--force' to force overwriting.\n", tmp_name);
	    stop = true;
	}    


	sprintf (tmp_name, "%s%s", data_directory, FILESFOUND_TXTDB);
	if (FileExist (tmp_name))
	{
	    printf ("Error: File %s exists. Delete the file or use '--force' to force overwriting.\n", tmp_name);
	    stop = true;
	}    
	if (stop) 
	{
	    printf ("Note: Remove the whole '%s' directory to have a clean start.\n", data_directory);
	    return EXIT_FAILURE;
	}
    }

    sprintf (tmp_name, "%s%s", data_directory, FOLDERTABLE_TXTDB);
    pf_folder_table = fopen (tmp_name, "w");
    if (!pf_folder_table)
    {
	printf ("Unable to create %s: %s\n", tmp_name, strerror (errno));
	return EXIT_FAILURE;
    }


    sprintf (tmp_name, "%s%s", data_directory, FILESFOUND_TXTDB);
    pf_file_info = fopen (tmp_name, "w");
    if (!pf_file_info)
    {
	printf ("Unable to create %s: %s\n", tmp_name, strerror (errno));
	return EXIT_FAILURE;
    }
    

    int ofs = 0;
    fofs  = offset;

    buffer = (char *)malloc (vh->blockSize * 2);


    PrintInfo();

    uint32_t block = 0;
    
    FillCache (fd, fofs);
        
    while (1)
    {
	if (!GetCacheBuffer (fd, buffer, vh->blockSize * 2, fofs))
	    break;

	//if (buffer[0] != 0xff) 
	ScanBuffer (buffer, ofs, block);

	fofs += (uint64_t)vh->blockSize;
	scanned =  (float)fofs / (float)file_size * 100;
	block++;

	PrintInfo();
    }

    scanned = 100;
    ForcePrintInfo ();

    LogPrn ("");
    ShowTime();

    LogPrn ("Done.\n");

    fclose (pf_file_info);
    fclose (pf_folder_table);
    return EXIT_SUCCESS;
}


void PrintInfoStep2 (char *argv[], int argc)
{
    char parameter[1024];
    parameter[0] = 0;
    
    
    for (int i = 1; i < argc; i++)
    {
	if (strcmp (argv[i], "-f") != 0)
	{
	    if (strcmp (argv[i], "-s1") != 0) 
	    {
		if (strcmp (argv[i], "-d") != 0)  // Skip -d parameter
		{
		    strcat (parameter, argv[i]);
		    strcat (parameter, " ");
		}
		else i++;
	    }
	}
    }
    Store ("parameter-s1", parameter);

    printf ("\n" SEPARATION_LINE	    
	    "Next step: STEP 2, cleanup file database.\n"
            "Next command: %s -s2", argv[0]);
    if (destination_directory[0] != 0)
    {
	printf (" -d '%s'", destination_directory);
    }
    printf ("\n\n");
}


//=================================
// STEP 2
//
// Cleanup file database FILESFOUND_TXTDB
//=================================

struct Digest {
    unsigned long number;
    char digest[SHA_DIGEST_LENGTH * 2 + 1];
};

// qsort(3) comparison helpers.
int CompareDigestByHash (const void *left_v, const void *right_v)
{
    const Digest *left = (const Digest*)left_v, *right = (const Digest*)right_v;
    return strncmp(left->digest, right->digest, SHA_DIGEST_LENGTH * 2);
}

int CompareDigestByNumber (const void *left_v, const void *right_v)
{
    const Digest *left = (const Digest*)left_v, *right = (const Digest*)right_v;
    return left->number - right->number;
}

void CleanupFileDatabase ()
{
    char line[1024];
    char tmp_name[PATH_MAX_LENGTH];
    FILE *pf;
    FILE *pf_tmp;

    sprintf (tmp_name, "%s%s", data_directory, FILESFOUND_TXTDB);
    pf = fopen (tmp_name, "r");
    if (!pf)
    {
	printf ("Unable to open '%s': %s\n", tmp_name, strerror (errno));
	exit (EXIT_FAILURE);
    }
    
    sprintf (tmp_name, "%s%s", data_directory, FILEINFO_SHA);
    pf_tmp = fopen (tmp_name, "w");
    if (!pf_tmp)
    {
	printf ("Unable to open '%s': %s\n", tmp_name, strerror (errno));
	exit (EXIT_FAILURE);
    }
    
    if (utf8_len != 1)
    {
	LogPrn ("*** UTF-8 max char len: %d\n", utf8_len);
    }

    LogPrn ("Cleanup file database:");

    LogPrn ("  Hashing file entries...");
    int num = 0;
    while (!feof (pf))
    {
	SHA_CTX ctx;
	SHA1_Init(&ctx);
	
	bool valid = true;
	
	for (int i = 0; i < FILE_INFO_BLOCK_NUM_LINES; i++)
	{
	    if (fgets (line, sizeof (line), pf))
	    {
		// process only static values
		if (((i < 15) && (i != 1)) ||
		   (i == 19 || i == 23 || i == 27 || i == 31 || i == 35 || i == 39 || i == 43 || i == 48))
		{
		    RemoveNL (line);
		    SHA1_Update(&ctx, line, strlen (line));
		}
	    }
	    else
	    {
		valid = false;
		break;
	    }
	}
	
	if (valid)
	{
	    unsigned char line_hash[SHA_DIGEST_LENGTH];
	    SHA1_Final(line_hash, &ctx);

	    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
	    {
		fprintf (pf_tmp, "%02x", line_hash[i]);
	    }
	    fprintf (pf_tmp, "\n");
	    num++;
	}
    }

    fclose (pf_tmp);
    
    LogPrn ("  %d database entries.", num);

    Digest *hash = (Digest *)malloc(num * sizeof(Digest));
    
    if (!hash)
    {
	LogPrn  ("Error: Cannot allocate memory. %s\n", strerror (errno));
	LogClose();
	exit (EXIT_FAILURE);    
    }

    LogPrn ("  Allocated %d MB RAM.", num * sizeof(Digest) / 1024 / 1024 + 1);

    sprintf (tmp_name, "%s%s", data_directory, FILEINFO_SHA);
    pf_tmp = fopen (tmp_name, "r");
    if (!pf_tmp)
    {
	LogPrn ("Unable to open '%s': %s", tmp_name, strerror (errno));
	LogClose();
	exit (EXIT_FAILURE);
    }

    for (int i = 0; i < num; i++)
    {
	if (fgets (line, sizeof (line), pf_tmp))
	{
	    RemoveNL (line);
	    strcpy (hash[i].digest, line);
	    hash[i].number = i;
	}
    }
    fclose (pf_tmp);

    LogPrn ("  Searching for duplicate database entries...");
    qsort(hash, num, sizeof(Digest), CompareDigestByHash);
    int removed = 0;
    for (int i = 1; i < num; i++)
    {
	if (!strcmp(hash[i].digest, hash[i - 1].digest)) 
	{
	    hash[i - 1].digest[0] = '\0';
	    removed++;
	}
    }
    LogPrn ("  Found %d duplicate entries.", removed);
    qsort(hash, num, sizeof(Digest), CompareDigestByNumber);

    fseek (pf, 0, SEEK_SET);


    sprintf (tmp_name, "%s%s", data_directory, FILEINFO_TXTDB);
    pf_tmp = fopen (tmp_name, "w");
    if (!pf_tmp)
    {
	LogPrn ("Unable to open '%s': %s", tmp_name,strerror (errno));
	LogClose();
	exit (EXIT_FAILURE);
    }


    LogPrn ("  Creating fresh database...\n");
    
    int  number = 1;
    int  num_new_entries = 0;

    bool found_encoding_error    = false;
    bool problematic_line        = false;
    bool found_future_date_error = false;
    
    char temp_file_block[FILE_INFO_BLOCK_NUM_LINES][sizeof (line)];
    
    for (int i1 = 0; i1 < num; i1++)
    {
	bool skip_file = false;
	
	// Load the file informations into a temporary buffer for further checks
	for (int i2 = 0; i2 < FILE_INFO_BLOCK_NUM_LINES; i2++)
	{
	    ClearBuffer (line, sizeof (line));
	    if (fgets (line, sizeof (line), pf))
	    {
		if (strlen (line) != StrLen2 (line, sizeof (line)))
		{
		    Log ("Error problematic line: Number %d, file block skipped", i1 * FILE_INFO_BLOCK_NUM_LINES + i2);
		    skip_file = true;
		    problematic_line = true;
		}
		else
		{
		    strcpy (temp_file_block[i2], line);
		}
	    }
	}
	
	if (hash[i1].digest[0] && !skip_file) 
	{
	    // Check if file date is in the future
	    uint32_t file_date;
	    file_date = strtoull (temp_file_block[6] + 9, NULL, 0);
	    time_t rawtime = file_date;
		
	    double diff;
	    diff = difftime (rawtime, time_start);

	    if (diff > future_date_tolerance) 
	    {
	        LogNoNL ("Error future date (line %d): %s, %s", i1 * FILE_INFO_BLOCK_NUM_LINES, strtok (ctime (&rawtime), "\n"), temp_file_block[0]);
	        found_future_date_error = true;
	        skip_file = true;		
	    }


	    if (!skip_file)
	    {
		// Additional file name check
		if (!ValidEncoding ((unsigned char *)temp_file_block[0], 
	                    	    strlen (temp_file_block[0]) - 1, 
	                    	    utf8_len))
		{
		    LogNoNL ("Error encoding (line %d): %s", i1 * FILE_INFO_BLOCK_NUM_LINES, temp_file_block[0]);
		    found_encoding_error = true;
		    skip_file = true;
		}
	    }
	    
	    if (!skip_file)
	    {
		// Write entry to database		
		for (int i2 = 0; i2 < FILE_INFO_BLOCK_NUM_LINES; i2++)
		{
		    if (i2 == 1)
		    {
			fprintf (pf_tmp, "Number: %d\n", number++);
			num_new_entries++;
		    }
		    else
		    {
			fprintf (pf_tmp, "%s", temp_file_block[i2]);
		    }
		}	    
	    }	
	}
    }

    free(hash);
    fclose (pf);
    fclose (pf_tmp);

    ShowTime();
    
    LogPrn ("Fresh database created with %d entries. %d duplicate entries removed.\n", num_new_entries, removed);

    bool show_log_file_info = false;
    
    if (problematic_line)
    {
	LogPrn ("* Info: Problematic lines have been found in the database. If you want to \n"
	        "        check the lines then search for 'Error problematic line' in the log \n"
	        "        file.\n");
	show_log_file_info = true;
    }
        
    if (found_future_date_error && !force_future_date)
    {
	LogPrn ("* Info: Entries with file date in the future have been removed! Usually, those \n"
	        "        are wrong file detections. If you want to check the file names then \n"
	        "        search for 'Error future date' in the log file. If you don't expect \n"
	        "        future file dates, then you can ignore this. The future date tolerance\n"
	        "        is 7 days. To set your own future date tolerance, run again -s2 and\n"
	        "        use '--future-days <days>'. \n");
    
	show_log_file_info = true;
    }

    if (found_encoding_error && !force_utf8_len)
    {
	LogPrn ("* Info: Entries with invalid file name encodings have been removed! Usually,\n"
	        "        those are wrong file detections. If you want to check the file names\n"
	        "        then search for 'Error encoding' in the log file. When you don't expect\n"
	        "        asian chars in your file names, then you can ignore this. To enable asian\n"
	        "        chars, run again -s2 and use '--utf8len 2'.\n");

	show_log_file_info = true;
    }


    if (show_log_file_info)
    {
	// Show log file only once, even on multiple errors.
	LogPrn ("Log file: %s\n", log_file_name);
    }

}

void PrintInfoStep3 (char *argv[], int argc)
{
    char parameter[1024];
    LoadString ("parameter-s1", parameter, sizeof (parameter));

    printf (SEPARATION_LINE
	    "Next step: STEP 3, restore files.\n"
            "Possible parameters for -s3: <device node|image file> [-b <block size>] [-o <offset in bytes>] [-c <file number>] [--alternative]\n\n"
            "Next command: %s -s3 %s", argv[0], parameter);
    if (destination_directory[0] != 0)
    {
	printf (" -d '%s'", destination_directory);
    }
    printf ("\n\n");
    
}


//=================================
// STEP 3
//
// Restore files
//=================================

char file_buffer[1024 * 1024 * 3]; // 3 MB

void WriteFileBytes (int fd, FILE *pfout, uint64_t file_offset, uint64_t bytes, uint64_t bytes_left)
{
    while (bytes > 0)
    {
	if (++dots > 39) dots = 0;
    	
    	if (dots % 10 == 0)
    	{
    	    if (bytes_left > 1024 * 1024 * 1024)
    	    {
    		float left = (float)(bytes_left / 1024 / 1024);
    		printf ("   %0.2f GB left.",  left / 1024);
    	    }
    	    else if (bytes_left > 1024 * 1024)
    	    {
    		printf ("   %" PRIu64 " MB left.", bytes_left / 1024 / 1024);
    	    }
    	    if (dots == 00)  printf ("  [|]  \r");
    	    if (dots == 10)  printf ("  [/]  \r");
    	    if (dots == 20)  printf ("  [-]  \r");
    	    if (dots == 30)  printf ("  [\\]  \r");

	    fflush (stdout);    		
	}
    	
	if (bytes >= sizeof (file_buffer))
	{
	    pread64 (fd, file_buffer, sizeof (file_buffer), file_offset);    
	    fwrite (file_buffer, 1, sizeof (file_buffer), pfout);

	    bytes       -= sizeof (file_buffer);
	    file_offset += sizeof (file_buffer);

	    bytes_left  -= sizeof (file_buffer);

	}
	else
	{
	    pread64 (fd, file_buffer, bytes, file_offset);    
	    fwrite (file_buffer, 1, bytes, pfout);	
	    bytes = 0;

	}    
    }
}

bool RestoreUncompressedFile (_FILE_INFO &file_info, const char *file_name)
{
    bool error = false;
    bool allow_extents_check = true;

    FILE *pf;
    uint64_t bytes_left;
    uint64_t file_offset;
    uint64_t bytes;

    bytes_left = file_info.file_size;

    if (file_info.has_extents_overflow && (vh->extentsFile.extents[0].startBlock < 1))
    {
	LogPrn ("Error: The file is strongly fragmented, access to the Extents Overflow File is required. The Extents Overflow File start block is not valid!");
	return false;
    }

    if (file_info.catalog_num_blocks > bytes_left / block_size + 1)
    {
	LogPrn ("Warning: Too many blocks allocated for file size, %s, blocks: %" PRIu32 ", file size: %" PRIu64 "", file_info.file_name, file_info.catalog_num_blocks, bytes_left);
	
	if (!ignore_blocks && (bytes_left > 1024 * 1024 * 1024))
	{
	    LogPrn ("Error: _too_many_blocks_skipped_: Skipping file restore. File number: %" PRIu32 ". It's bigger than 1 GB and maybe an invalid file. Use '--ignore-blocks' to restore this file.", file_info.number);
	    invalid_skipped = true;
	    return false;
	}
    }

    pf = fopen (file_name, "w");
    if (!pf)
    {
	LogPrn ("Unable to create file %s: %s\n", file_name, strerror (errno));
	
	if (ignore_file_create_error)
	{
	    LogPrn ("Error: _file_create_error_: Skipping file restore. File number: %" PRIu32 ".", file_info.number);	
	    file_create_error_skipped = true;
	    return false;
	}
	else
	{	
	    LogPrn ("Error: _file_create_error_: File number: %" PRIu32 ". Use '--ignore-file-error' to skip this error.", file_info.number);
	    exit (EXIT_FAILURE);
	}
    }
    
    if (bytes_left == 0)
    {
	fclose (pf);
	return true;
    }

    
    for (int i = 0; i < 8; i++)
    {
	if ((file_info.blocks[i] > 0) && (file_info.num_blocks[i] > 0))
	{    	    
    	    Log ("Extent %d: block  %" PRIu32 ", num blocks %" PRIu32, i, file_info.blocks[i], file_info.num_blocks[i]);

	    file_offset =  (uint64_t)file_info.blocks[i];
	    file_offset *= (uint64_t)block_size;
	    file_offset += offset;
	
	    bytes = (uint64_t)block_size * (uint64_t)file_info.num_blocks[i];
	
	    if (bytes > bytes_left) bytes = bytes_left;

	    WriteFileBytes (fd, pf, file_offset, bytes, bytes_left);

	    bytes_left -= bytes;
	}
	else
	{	    
	    allow_extents_check = false;
	    break;
	}
    }


    if (file_info.has_extents_overflow)
    {
        Log ("Info: _has_extents_overflows_: File number: %" PRIu32 ".", file_info.number);
    }

    if (!ignore_eof && file_info.has_extents_overflow && allow_extents_check && EOF_IsAvailable())
    {
	uint32_t current_blocks = file_info.catalog_num_blocks;

	uint32_t file_block = file_info.catalog_num_blocks;
	
	uint32_t eof_data_left = 0;

	while ((bytes_left > 0) && !error)
	{
	    uint32_t block;
	    uint32_t num_blocks;

	    if (!EOF_GetRemainingBlocks (file_info, file_block, &block, &num_blocks)) 
	    {
		printf ("Error: Unable to find extents in the ExtentsOverflowFile\n");
		break;
	    }

	    file_offset  = (uint64_t)block;
	    file_offset *= (uint64_t)block_size;
	    file_offset += offset;
	
	    bytes = (uint64_t)block_size * (uint64_t)num_blocks;
	
	    if (bytes > bytes_left) bytes = bytes_left;

    	    WriteFileBytes (fd, pf, file_offset, bytes, bytes_left);
    	    

	    bytes_left -= bytes;
	    
	    file_block += num_blocks;

	}
    }
    fclose (pf);

    printf ("\r                      \r"); // Clear info line
    dots = 0;

    
    // bytes left?
    if (bytes_left > 0)
    {    
	LogPrn ("Error: %s not fully recovered. %s", file_info.file_name, file_info.has_extents_overflow ? "File has Extents Overflow Data." : "");
    }
    else
    {
	file_info.restored = true;
    }
    return true;
}



void RestoreFile (_FILE_INFO &file_info, bool use_parent_directory)
{
    char dir[PATH_MAX_LENGTH];
    char file_name[PATH_MAX_LENGTH];
    char tmp_file_name[FILE_NAME_LENGTH];

    strcpy (tmp_file_name, file_info.file_name);
    FixFileName (tmp_file_name);

    // Print some file details
    LogNoNLPrn ("File: %s (%" PRIu64 " bytes", tmp_file_name, file_info.file_size);
    if (file_info.file_size > 1024 * 1024 * 1024) LogNoNLPrn (", %0.2f GB", (float)(file_info.file_size) / (1024 * 1024 * 1024));
    else if (file_info.file_size > 1024 * 1024) LogNoNLPrn (", %0.2f MB", (float)file_info.file_size / (1024 * 1024));
    else if (file_info.file_size > 1024) LogNoNLPrn (", %0.2f KB", (float)file_info.file_size / 1024);
    
    LogPrn ("%s)", file_info.compressed ? ", compressed file" : "");
    
    if (use_parent_directory)
    {
	sprintf (dir, "%srestored/%d", destination_directory, file_info.parent_id);
	MKDir (dir, ALLOW_EXIST);
    
	sprintf (file_name, "%srestored/%d/%s", destination_directory, file_info.parent_id, file_info.file_name);
    }
    else
    {
	sprintf (file_name, "%srestored/%s", destination_directory, file_info.file_name);    
    }
    
    
    if (FileExist (file_name))
    {
	if (param_alternative_file_name)
	{
	    AlternativeFileName (file_name, file_info.catalog_id, file_name);
	}
	else
	{	
	    // Keep only newest file version or overwrite it when the already restored file has 0 bytes
	    struct stat st;
	    if (stat (file_name, &st) == 0)
	    {
		if (st.st_size > 0)
		{
		    if (st.st_mtime == file_info.file_time)
		    {
			LogPrn ("Skipping file restore, file time is equal to the existing file. File number %" PRIu32 " _double_file_name_", file_info.number);
			info_alternative = true;
			return;
		    }

		    if (st.st_mtime > file_info.file_time)
		    {
			LogPrn ("Skipping file restore, file time is older than the restored. File number %" PRIu32 " _double_file_name_", file_info.number);
			info_alternative = true;
			return;
		    }
		    
		    if (file_info.file_size == 0)
		    {
			LogPrn ("Skipping file restore, new file would truncate the restored file to 0. File number %" PRIu32 " _double_file_name_", file_info.number);
			info_alternative = true;
			return;
		    }		    
		}
	    }
	}
    }

    
    strcpy (file_info.restored_file_name, file_name);    

    utimbuf timbuf;
    timbuf.actime = file_info.file_time;
    timbuf.modtime = file_info.file_time;
    
    if (file_info.compressed)
    {
	if (file_info.file_size > 0)
	{
	    file_info.compressed = false;
	    LogPrn ("Warning: Compressed file with file size! %s handled as uncompressed", file_info.file_name);
	}
	else if (file_info.fork_size == 0)
 	{
	    file_info.compressed = false;
	    LogPrn ("Warning: Compressed file with fork size 0! Unable to decompress %s", file_info.file_name);
	}
    }

    if (file_info.compressed)
    {
	file_info.restored = RestoreCompressedFile (fd, file_info.fork_size, file_info.fork_block, file_info.fork_num_blocks, offset, file_name, file_info.number);
    }
    else
    {
	RestoreUncompressedFile (file_info, file_name);
    }

    
    // Set file time
    utime (file_name, &timbuf);
}

bool LoadFileInfo (FILE *pf, _FILE_INFO  &file_info)
{
    char line[1024];
    
	fgets (line, sizeof (line), pf);
	
	if (line[0] != '#')
	{
	    file_info.valid = true;
	    file_info.restored = false;
	    file_info.has_extents_overflow = false;
//	    skip_invalid = false;

//	    file++;
//	    if (file < start_number) skip_continue = true;
//	    else skip_continue = false;
	    
	    
	    if (strlen (line) > 0)
		line[strlen (line) - 1] = 0;

	    //printf ("File name: %s\n", line);
	
	    strcpy (file_info.file_name, line);

	    if (slash_convert)
	    {
		CharReplace (file_info.file_name, ':', '/');
	    }

	    fgets (line, sizeof (line), pf); // number
	    file_info.number = atoi (line + 8);
	    
	    fgets (line, sizeof (line), pf); // catalog id	    
	    if (line[12] == '-') file_info.valid = false;
	    else file_info.catalog_id = atoi (line + 11);


	    fgets (line, sizeof (line), pf); // file size
	    if (line[11] == '-') file_info.valid = false;
	    else file_info.file_size = atoll (line + 10);
	    
	    if (file_info.file_size > fs_size)
	    {
		file_info.valid = false; // file bigger than file system
	    }
	    
	    
	    fgets (line, sizeof (line), pf); // parent id
	    if (line[11] == '-') file_info.valid = false;
	    else file_info.parent_id = atoi (line + 10);

	    fgets (line, sizeof (line), pf); // date (human readable)
	    RemoveNL (line);
	    strcpy (file_info.date_str, line + 6);
	    
	    fgets (line, sizeof (line), pf); // rawtime
	    file_info.file_time = atoll (line + 9);

	    fgets (line, sizeof (line), pf); // uid
	    fgets (line, sizeof (line), pf); // gid
	    fgets (line, sizeof (line), pf); // type
	    fgets (line, sizeof (line), pf); // creator
	    fgets (line, sizeof (line), pf); // compressed
	    file_info.compressed = (strncmp (line + 12, "Yes", 3) == 0);
	    
	    fgets (line, sizeof (line), pf); // fork size
	    file_info.fork_size = atoll (line + 11);
	    
	    fgets (line, sizeof (line), pf); // fork block
	    file_info.fork_block = atoll (line + 12);
	    
	    fgets (line, sizeof (line), pf); // fork num blocks
	    file_info.fork_num_blocks = atoll (line + 17);

	    fgets (line, sizeof (line), pf); // entry info
	    fgets (line, sizeof (line), pf); // ########################
	    
	    bool check_valid_blocks = false;
	    bool has_blocks = false;

	    for (int i = 0; i < 8; i++)
	    {
		fgets (line, sizeof (line), pf);

		int pos;
		pos = Find (line, sizeof (line), ')');
		if (pos > -1) sscanf (line + pos + 2, "%x", &file_info.blocks[i]);
		else file_info.valid = false;
		    
		fgets (line, sizeof (line), pf);
		
		pos = Find (line, sizeof (line), ')');
		if (pos > -1) sscanf (line + pos + 2, "%x", &file_info.num_blocks[i]);
		else file_info.valid = false;

		fgets (line, sizeof (line), pf); // dummy
		fgets (line, sizeof (line), pf); // dummy
		
		
		if ((file_info.blocks[i] == 0) || (file_info.num_blocks[0] == 0)) check_valid_blocks = true;

		if ((file_info.blocks[i] != 0) || (file_info.num_blocks[0] != 0)) has_blocks = true;

		// Block and num_block must be zero after first zero extent, else entry is invalid
		if (check_valid_blocks && ((file_info.blocks[i] != 0) || (file_info.num_blocks[i] != 0))) file_info.valid = false;
		
		// Check for invalid entry
		if ((file_info.blocks[i] == 0) && (file_info.num_blocks[i] != 0)) file_info.valid = false;
		if ((file_info.num_blocks[i] == 0) && (file_info.blocks[i] != 0)) file_info.valid = false;
	    }
	    
	    if (file_info.valid)
	    {
		if (file_info.compressed) // invalid entry check for compressed files
		{
		    if (has_blocks) file_info.valid = false; // compressed files have no blocks
		
		    uint64_t calc_size;
		    calc_size =  (uint64_t)file_info.fork_num_blocks;
		    calc_size *= (uint64_t)block_size;
		
		    if (file_info.fork_size > calc_size) file_info.valid = false; // fork size cant be larger than allocated blocks
		}
		else // check for extents of uncompressed file
		{
		    int all_blocks = 0;
		    for (int i = 0; i < 8; i++)
		    {
			all_blocks += file_info.num_blocks[i];
		    }
	
		    file_info.has_extents_overflow = (all_blocks * block_size < file_info.file_size);
		    file_info.catalog_num_blocks   = all_blocks;
		    
		    if (all_blocks * block_size > fs_size)
		    {
			file_info.valid = false; // file bigger than file system
		    }
		}
		
	    }
	    return true;
	}
	return false;
}



void RestoreFiles (int start_number)
{
    _FILE_INFO file_info;
    FILE *pf;
    char line[1024];
    int num_files = 0;
    int num_list_files = 0;
    int line_nr = 1;
    int num = 0;
    char tmp_name[PATH_MAX_LENGTH];
    
    std::vector <int> file_numbers;
    char restored_dir[PATH_MAX_LENGTH];

    sprintf (restored_dir, "%srestored", destination_directory);
    MKDir (restored_dir  , ALLOW_EXIST);

    sprintf (restored_dir, "%srestored/2", destination_directory);  // root directory
    MKDir (restored_dir  , ALLOW_EXIST);
        
    // Handle file list
    if (file_list != NULL)
    {
	FILE *pf_list;
	pf_list = fopen (file_list, "r");
	if (!pf_list)
	{
	    LogPrn ("Unable to open '%s': %s", file_list, strerror (errno));
	    exit (EXIT_FAILURE);
	}
	
	if (is_file_list_csv)
	{
	    // Read CSV header
	    fgets (line, sizeof (line), pf_list);
	    line_nr++;
	}
	
	LogPrn ("Loading file numbers from '%s'.%s", file_list, (is_file_list_csv ? " CSV mode." : ""));
	while (!feof (pf_list))
	{
	    if (fgets (line, sizeof (line), pf_list) == NULL) break;
	    	    
	    unsigned int val;
	    if (is_file_list_csv)
	    {
		char field1[255];
		int pos;

		// Check if only the number column is exported
		pos = Find (line, strlen (line), ';');
		if (pos != -1)
		{		
		    strncpy (field1, line, pos);
		}
		else
		{
		    strncpy (field1, line, sizeof (field1));
		}

		// Remove possible double quotes
		CharRemove (field1, '"');

		// Get value
		val = atoi (field1);
	    }
	    else
	    {
		int pos;
		pos = Find (line, strlen (line), ':');
		if (pos != -1)
		{
		    // Get value
		    sscanf (line, "%d:", &val);
		}
		else
		{
		    LogPrn ("Warning: Invalid file format at line %d. Need a ':' after the file number.", line_nr);
		}
	    }
	    
	    if (val > 0)
	    {
		file_numbers.push_back (val);
		num_list_files++;
	    }
	    
	    // read additional data until end of line
	    while (!feof (pf_list) && (strlen (line) == sizeof (line) - 1))
	    {
		fgets (line, sizeof (line), pf_list);
		if (line[strlen (line) - 1] == '\n') break;
	    }
	    line_nr++;
	}	
	fclose (pf_list);
    }


    sprintf (tmp_name, "%s%s", data_directory, FILEINFO_TXTDB);
    pf = fopen (tmp_name, "r");
    if (!pf)
    {
	LogPrn ("Unable to open '%s': %s", tmp_name, strerror (errno));
	exit (EXIT_FAILURE);
    }

    // Get number of files in the DB
    while (fgets (line, sizeof (line), pf))
    {
	if (line[0] != '#') num_files++; // ignore comments 
    }
    num_files /= 40;

    fseek (pf, 0, SEEK_SET); // Back to the file start

    if (!ignore_eof)
    {
	if (!ExtractExtentsOverflowFileAutomatic (eof_file, false)) // file name, wait on warning
	{
	    exit (EXIT_FAILURE);    
	}

	LogPrn (""); // Cosmetic
	
	if (!EOF_OpenFile (eof_file))
	{	
    	    exit (EXIT_FAILURE);    
	}
    }

    // Info text
    if (start_number > 1)
    {
	LogPrn ("Skipping files...");
    }
    
    // Start with restoring
    while (!feof (pf))    
    {
	if (LoadFileInfo (pf, file_info))
	{
	    if (file_info.number < start_number)
	    {
		LogPrn ("[%d/%d] File: Skipping %s ", file_info.number, num_files, file_info.file_name);
	    }
	    else if (!file_info.valid)
	    {
		LogPrn ("[%d/%d] File: Invalid entry for %s ", file_info.number, num_files, file_info.file_name);
	    }
	    else if (file_info.blocks[0] == 0)
	    {
		LogPrn ("[%d/%d] File: Invalid start block for %s ", file_info.number, num_files, file_info.file_name);
	    }
	    else
	    {
		if (file_list != NULL) // Restore from list
		{
		    for (int i = 0; i < file_numbers.size(); i++)
		    {
			if (file_info.number == file_numbers[i])
			{
			    num++;
			    LogNoNLPrn ("[%d/%d, Nr %d] ", num, num_list_files, file_info.number);
			    RestoreFile (file_info, true); // 2nd parameter: restore into parent directory
			    file_numbers.erase (file_numbers.begin() + i); // Remove number
			    break;
			}
		    }
		    
		    if (file_numbers.size() == 0) break; // All files from the list restored. Stop restoring.
		    
		}
		else // Standard restore
		{
		    LogNoNLPrn ("[%d/%d] ", file_info.number, num_files);
		    RestoreFile (file_info, true); // 2nd parameter: restore into parent directory
		}
	    }
	}
    }
    fclose (pf);

    EOF_CloseFile();

    
    ShowTime();
    
    LogPrn ("Done.\n");
    
    if (info_alternative && !param_alternative_file_name)
    {
	LogPrn ("Note: Old/same files have been discovered but not restored. Run this step (STEP 3) with '--alternative' to restore those files too. But rename or backup the current '%srestored' directory. Then remove the '%srestored' directory to get a fresh new directory tree. You can scan the s3 log file for '_double_file_name_' to find the files.", destination_directory);
    }
    
    if (invalid_skipped)
    {
	LogPrn ("Note: Files bigger than 1 GB with too many blocks have been skipped. Check the '%ss3.log' file and search for '_too_many_blocks_skipped_'.", data_directory);
    }

    if (file_create_error_skipped)
    {
	LogPrn ("Note: Problems with creating some files. Check the '%ss3.log' file and search for '_file_create_error_'.", data_directory);
    }

    if (file_list != NULL)
    {
	LogPrn ("Note: Use 'hfsprescue --remove-empty-dirs' after STEP 6 to easily remove empty directories.");    
    }
    
    printf ("\n");
}

void PrintInfoStep4 (char *argv[])
{
    printf (SEPARATION_LINE
	    "Next step: STEP 4, Restore your directory structure.\n"
            "Next command: %s -s4", argv[0]);
    if (destination_directory[0] != 0)
    {
	printf (" -d '%s'", destination_directory);
    }
    printf ("\n\n");
}



//===========================================
// STEP 4
// 
// Create directory structure with directory IDs
// Rename directories from ID to real name
// Move directories with no valid parent
//===========================================

struct _dir_restore
{
    uint32_t folder_id;
    uint32_t parent_id;
    unsigned int filename;
    bool dirloop;
};

int ExpandDirInfo (char *info, 
		    uint32_t *folder_id,
		    uint32_t *parent_id,
		    char *filename)
{
    unsigned int i, p, t;
    char txt[4096];
    uint32_t id;
    
    p = 0;
    t = 0;
    for (i = 0; info[i] != 0; i++)
    {
	if ((info[i] !='\r') && (info[i] != '\n'))
	{
	    if (info[i] != '|')
	    {
		txt[p] = info[i];
		p++;	
	    } 
	    else 
	    {
		txt[p] = 0;
		switch (t)
		{
		    case 0:
			id = strtoul (txt, NULL, 0);
			//id = atoll (txt);
			//if (id < 0) id = 0;
			*folder_id = id;
			break;

		    case 1:
			id = strtoul (txt, NULL, 0);
			//id = atoll (txt);
			//if (id < 0) id = 0;
			*parent_id = id;
			break;	    
		}
		t++;
		p = 0;
	    }
	}    
    }

    // catch impossible situation    
    if (*parent_id == *folder_id)
    {
	*parent_id = 0;
	*folder_id = 0;
    }
    
    txt[p] = 0;
    strcpy (filename, txt);
    return strlen (txt);
}


std::vector<_dir_restore> entries;
char *filenames;


void FindParentDirectory (uint32_t id, char *path)
{
    char path2[PATH_MAX_LENGTH];
    sprintf (path2, "%" PRIu32 "/", id);
    
    if (strlen (path2) + strlen (path) > PATH_MAX_LENGTH - 100)
    {
	printf ("Warning: Directory tree too big for directory ID %" PRIu32 ".\n", id);
	return;
    }
    
    
    // Check for looping directories. Is the directory already in the path?
    sprintf (path2, "/%" PRIu32 "/", id);
    if (strstr (path, path2) != NULL)
    {
	Log ("Skip looping folder ID: %" PRIu32, id);
	return;
    }    

    // Construct new path
    sprintf (path2, "%" PRIu32 "/%s", id, path);
    strcpy (path, path2);

    if (id == 2) // root directory has no parent
    {
	return;
    }

    for (unsigned int i = 0; i < entries.size(); i++)
    {
	if (entries[i].folder_id == id)
	{
	    FindParentDirectory (entries[i].parent_id, path);
	    return;
	}
    }
}

// Rename directory from id to real name
void RenameDirectory (const char *path, const char *d_name)
{
    char new_path[PATH_MAX_LENGTH];
    DIR *dir;
    dirent *d;

    sprintf (new_path, "%s/%s", path, d_name);
    dir = opendir (new_path);

    while ( (d = readdir (dir)) )
    {
	if ((strcmp (d->d_name, ".") != 0) &&
	    (strcmp (d->d_name, "..") != 0))
	{
	    if (d->d_type == DT_DIR)
	    	RenameDirectory (new_path, d->d_name);	
	}
    }
    closedir (dir);

    char file_name[PATH_MAX_LENGTH];

    // find and rename directory to original name
    uint32_t id = atoi (d_name);
    for (unsigned int i = 0; i < entries.size(); i++)
    {
	if (entries[i].folder_id == id)
	{
	    sprintf (new_path, "%s/%s", path, d_name); // is now current directory name
	    sprintf (file_name, "%s/%s", path, filenames + entries[i].filename); // rename to this name

	    Log ("mv %s/%s -> %s", path, d_name, file_name);

	    rename (new_path, file_name);
	    break;
	}
    }
}    

void MoveUnlinkableDirectories()
{
    DIR *dir;
    dirent *d;

    char source[PATH_MAX_LENGTH];
    char dest[PATH_MAX_LENGTH];

    sprintf (dest, "%srestored/newroot/x_directory_problem", destination_directory);
    MKDir (dest, ALLOW_EXIST);
    
    sprintf (dest, "%srestored/newroot", destination_directory);
    dir = opendir (dest);
    if (!dir)
    {
	printf ("Error creating directory %s: %s\n", dest, strerror (errno));
	exit (EXIT_FAILURE);
    }


    while ( (d = readdir (dir)) )
    {
	if ((strcmp (d->d_name, ".") != 0) &&
	    (strcmp (d->d_name, "..") != 0) &&
	    (strcmp (d->d_name, "2") != 0) &&
	    (strcmp (d->d_name, "x_directory_problem") != 0))
	{
	    sprintf (source, "%srestored/newroot/%s", destination_directory, d->d_name);
	    sprintf (dest,   "%srestored/newroot/x_directory_problem/%s", destination_directory, d->d_name);
	
	    rename (source, dest);	
	}
    }
    closedir (dir);

}

void RestoreDirectories()
{
    FILE *pf;
    char info[1024];
    int size = 0;
    _dir_restore entry;
    int utf8_len;
    char tmp_name[PATH_MAX_LENGTH];
    char tmp_name2[PATH_MAX_LENGTH];

    sprintf (tmp_name, "%s%s", data_directory, FOLDERTABLE_TXTDB);
    pf = fopen (tmp_name, "r");
    if (!pf)
    {
	fprintf (stderr, "Unable to open '%s': %s\n", tmp_name, strerror (errno));
	exit (EXIT_FAILURE);    
    }
    
    fseek (pf, 0, SEEK_END);
    size = ftell (pf);
    fseek (pf, 0, SEEK_SET);
    
    printf ("Allocating %0.2fMB\n", (float)size / 1024 / 1024);

    filenames = (char *)malloc (size);
    if (!filenames)
    {
	printf ("Error: Cannot allocate memory. %s\n", strerror (errno));
	exit (EXIT_FAILURE);    
    }

    utf8_len = LoadINT ("utf8len");

    if (utf8_len != 1)
    {
	LogPrn ("*** UTF-8 max char len: %d\n", utf8_len);
    }

    printf ("Loading directory IDs...\n");
        
    // load entries    
    int ofs = 0;    
    int duplicate_ids = 0;
    while (!feof (pf))
    {
	if (fgets (info, sizeof(info), pf))
	{
	    int len = ExpandDirInfo (info, &entry.folder_id, &entry.parent_id, filenames + ofs);
	    entry.filename = ofs;
	    if ((entry.folder_id > 1) && (entry.parent_id > 1)) 
	    {
		// Check for duplicate entries
		bool add = true;
		for (int i = 0; i < entries.size(); i++)
		{
		    if (entries[i].folder_id == entry.folder_id)
		    {
			LogNoNL ("Duplicate folder ID: %s", info);
			add = false;
			duplicate_ids++;
			break;
		    }
		}
		
		if (add)
		{
		    // Check for valid directory name
		    add = ValidEncoding ((unsigned char *)filenames + ofs, 
	                    		strlen (filenames + ofs), 
	                    		utf8_len);
	            if (!add)
	            {
	        	Log ("Invalid name: %s", filenames + ofs);
	            }
		}

		if (add)
		{
		    entries.push_back (entry);
		}
	    }
	    ofs += len + 1;
	}    
    }

    LogPrn ("%" PRIu32 " IDs loaded.", entries.size());
    
    if (duplicate_ids > 0)
    {
	LogPrn ("%d duplicate folder IDs found.", duplicate_ids);
    }

    LogPrn ("Find parent folders...");

    sprintf (tmp_name, "%srestored/mkdir.sh", destination_directory);
    pf = fopen (tmp_name, "w");
    if (!pf)
    {
	fprintf (stderr, "Unable to create %s: %s\n", tmp_name, strerror (errno));
	exit (EXIT_FAILURE);    
    }
    
    char path[PATH_MAX_LENGTH];
    fprintf (pf, "mkdir -p %srestored/newroot/2\n", destination_directory);
    fprintf (pf, "echo 2 > %srestored/newroot/2/hfsprescue_dir_id.tmp\n", destination_directory);
    
    for (unsigned int i = 0; i < entries.size(); i++)
    {
	sprintf (path, "/%" PRIu32, entries[i].folder_id);
	FindParentDirectory (entries[i].parent_id, path);
	fprintf (pf, "mkdir -p %srestored/newroot/%s\n", destination_directory, path); 
	fprintf (pf, "echo %" PRIu32 " > %srestored/newroot/%s/hfsprescue_dir_id.tmp\n", entries[i].folder_id, destination_directory, path);
    }
    fclose (pf);

    if (entries.size() == 0)
    {
	LogPrn ("No sub directories to restore.\n");
    }

    printf ("Creating new directory tree with IDs...\n");
    sprintf (tmp_name, "sh %srestored/mkdir.sh", destination_directory);
    system (tmp_name);

    // Rename directory from id to real name
    printf ("Rename directories...\n");
    sprintf (tmp_name, "%srestored", destination_directory);
    RenameDirectory (tmp_name, "newroot");

    MoveUnlinkableDirectories();
    sprintf (tmp_name, "%srestored/newroot/2", destination_directory);
    sprintf (tmp_name2, "%srestored/newroot/recovered", destination_directory);
    rename (tmp_name, tmp_name2);

    ShowTime();
    
    LogPrn ("Done.\n");
    printf ("\nNew directory structure created in %srestored/newroot/\n\n", destination_directory);

}

void PrintInfoStep5 (char *argv[])
{
    printf (SEPARATION_LINE
	    "Next step: STEP 5, move the restored files to the correct directories.\n"
            "Next command: %s -s5", argv[0]);
    if (destination_directory[0] != 0)
    {
	printf (" -d '%s'", destination_directory);
    }
    printf ("\n\n");
}


//===========================================
// STEP 5
//
// Move restored files to correct directory
//===========================================

void MoveDirectoryFiles (uint32_t id, char *new_path)
{
    char source[PATH_MAX_LENGTH];
    char dest[PATH_MAX_LENGTH];
    DIR *dir;
    dirent *d;

    sprintf (source, "%srestored/%" PRIu32, destination_directory, id);
    dir = opendir (source);
    if (!dir) 
    {
	return;
    }

    if (!FileExist (new_path))
    {
	fprintf (stderr, "Directory %s does not exist\n", new_path);
	exit (EXIT_FAILURE);    
    }
    
    while ( (d = readdir (dir)) )
    {
	if ((strcmp (d->d_name, ".") != 0) &&
	    (strcmp (d->d_name, "..") != 0))
	{
	    sprintf (source, "%srestored/%" PRIu32 "/%s", destination_directory, id, d->d_name);
	    sprintf (dest, "%s/%s", new_path, d->d_name);
	    printf ("%s -> %s\n", source, dest);

	    Log ("%s -> %s", source, dest);

	    rename (source, dest);
	}
    }
    closedir (dir);
    sprintf (source, "%srestored/%" PRIu32, destination_directory, id);
    rmdir (source);
}



void MoveFiles (const char *path, const char *d_name)
{
    char new_path[PATH_MAX_LENGTH];
    DIR *dir;
    dirent *d;

    sprintf (new_path, "%s/%s", path, d_name);
    dir = opendir (new_path);
    if (!dir) 
    {
	fprintf (stderr, "Unable open directory %s: %s\n", new_path, strerror (errno));
	exit (EXIT_FAILURE);
    }


    while ( (d = readdir (dir)) )
    {
	if ((strcmp (d->d_name, ".") != 0) &&
	    (strcmp (d->d_name, "..") != 0))
	{
	    if (d->d_type == DT_DIR) MoveFiles (new_path, d->d_name);	
	}
    }
    closedir (dir);

    uint32_t id;
    char file_name[PATH_MAX_LENGTH];
    char str_id[256];

    sprintf (file_name, "%s/hfsprescue_dir_id.tmp", new_path);

    FILE *pf;
    pf = fopen (file_name, "r"); // get id from file
    if (pf)
    {
	fgets (str_id, sizeof (str_id), pf);
	fclose (pf);
	id = atoi (str_id); 
    }
    else 
    {
	id = atoi (d_name);  // if no info file, then get id from name
    }
    MoveDirectoryFiles (id, new_path);    

}    

void PrintInfoStep6 (char *argv[])
{
    ShowTime();
    
    LogPrn ("Done.\n");

    printf ("\n" SEPARATION_LINE
	    "Next step: STEP 6, is the last step, finalize and cleanup.\n"
            "Possible parameters for -s6: [-k]\n\n"
            "Next command: %s -s6", argv[0]);
    if (destination_directory[0] != 0)
    {
	printf (" -d '%s'", destination_directory);
    }
    printf ("\n\n");
}


//===========================================
// STEP 6
//
// Move directories that maybe recovered from deleted
// Cleanup tmp files. Keep them if -k is used
// Create info file
//===========================================


void MoveUnknownDirectories()
{
    DIR *dir;
    dirent *d;

    char source[PATH_MAX_LENGTH];
    char dest[PATH_MAX_LENGTH];

    printf ("Moving directories with no parent...\n");
    sprintf (source, "%srestored/newroot/x_unknown", destination_directory);
    MKDir (source, ALLOW_EXIST);
    
    sprintf (source, "%srestored", destination_directory);
    dir = opendir (source);
    if (!dir) 
    {
	fprintf (stderr, "Unable open directory %s: %s\n", source, strerror (errno));
	exit (EXIT_FAILURE);
    }

    while ( (d = readdir (dir)) )
    {
	if ((strcmp (d->d_name, ".") != 0) &&
	    (strcmp (d->d_name, "..") != 0) &&
	    (strcmp (d->d_name, "mkdir.sh") != 0) &&
	    (strcmp (d->d_name, EXTENTS_OVERFLOW_FILE_NAME) != 0) && // Check/skip only the default file name of the extents oveflow file
	    (strcmp (d->d_name, VOLUME_HEADER_FILE_NAME) != 0) &&    // Check/skip only the default file name of the volume header
	    (strcmp (d->d_name, "newroot") != 0))
	{
	    sprintf (source, "%srestored/%s", destination_directory, d->d_name);
	    sprintf (dest,   "%srestored/newroot/x_unknown/%s", destination_directory, d->d_name);
	
	    rename (source, dest);	
	}
    }
    closedir (dir);
}

void CreateInfoFile()
{
    FILE *pf;
    char info[PATH_MAX_LENGTH];
    sprintf (info, "%srestored/newroot/INFO.TXT", destination_directory);
    
    pf = fopen (info, "w");
    if (!pf)
    {
	printf ("Unable to create %s: %s\n\n", info, strerror (errno));
	exit (EXIT_FAILURE);    
    }
    
    fprintf (pf, "Files restored with hfsprescue " PACKAGE_VERSION " https://www.plop.at\n\n");
    fprintf (pf, "Directory description:\n");
    fprintf (pf, "======================\n\n");
    fprintf (pf, "recovered           - You find your recovered files here.\n\n");
    fprintf (pf, "x_directory_problem - It was not possible to link the files and directories to their parent directory. You will find here files too.\n\n");
    fprintf (pf, "x_unknown           - No valid parent directory has been found. Maybe you will find here already deleted files. The files in this directories can be invalid.\n");
    fprintf (pf, "\n\n");
    fprintf (pf, "Additional info files:\n");
    fprintf (pf, "======================\n\n");
    fprintf (pf, "The contents of those files is just a list of the files in the corresponding directory.\nUseful when you need an overview of the restored files.\n\n");
    fprintf (pf, "recovered-files.txt, x_directory_problem-files.txt, x_unknown-files.txt\n");
    
    fclose (pf);
    
    printf ("Read the file '%s'\n\n", info);

}

void Cleanup (bool keep_tmp)
{
    char tmp_name[MAX_FILE_NAME_CHARS];
    char cwd[PATH_MAX_LENGTH];
    
    MoveUnknownDirectories();
    
    if (!keep_tmp)
    {
	printf ("Removing hfsprescue_dir_id.tmp files...\n");
	sprintf (tmp_name, "find %srestored -iname hfsprescue_dir_id.tmp -exec rm {} \\;", destination_directory);
	system (tmp_name);
	sprintf (tmp_name, "%srestored/mkdir.sh", destination_directory);
	unlink (tmp_name);    
    }

    getcwd(cwd, sizeof(cwd));

    sprintf (tmp_name, "%srestored/newroot", destination_directory);
    if (chdir (tmp_name) == 0)
    {
	printf ("Creating additional info files...\n");
	system ("find recovered > recovered-files.txt");
	system ("find x_directory_problem > x_directory_problem-files.txt");
	system ("find x_unknown > x_unknown-files.txt");
    }
    else
    {
	printf ("Error: Unable to create additional info files.\n");
    }

    
    chdir (cwd);
    printf ("\nDone.\n\n");
    CreateInfoFile();

    printf (" ******************************************************\n"
	    " * When hfsprescue helped you to bring your data back *\n"
	    " * then please donate a few dollars or euros.         *\n"
	    " *                                                    *\n"
	    " * See https://www.plop.at/en/hfsprescue.html         *\n"
	    " *                                                    *\n"
	    " ******************************************************\n"
	    "\n");
}

//===========================================

void RestoreOneFile (int file_number)
{
    _FILE_INFO file_info;    
    FILE *pf;
    char line[1024];
    char tmp_name[PATH_MAX_LENGTH];
    
    sprintf (tmp_name, "%srestored", destination_directory);
    MKDir (tmp_name, ALLOW_EXIST);
    
    sprintf (tmp_name, "%s%s", data_directory, FILEINFO_TXTDB);
    pf = fopen (tmp_name, "r");
    if (!pf)
    {
	printf ("Unable to open '%s': %s\n", tmp_name, strerror (errno));
	exit (EXIT_FAILURE);
    }

    
    if (!ignore_eof)
    {

	if (!ExtractExtentsOverflowFileAutomatic (eof_file, false)) // file name, wait on warning
	{
	    exit (EXIT_FAILURE);    
	}

	LogPrn (""); // Cosmetic
	
	if (!EOF_OpenFile (eof_file))
	{	
    	    exit (EXIT_FAILURE);    
	}
    }



    bool done = false;
    while (!feof (pf) && !done)
    {
	if (LoadFileInfo (pf, file_info))
	{
	    if (file_info.number == file_number)
	    {
		if (!file_info.valid)
		{
		    LogPrn ("Invalid entry for %s ", file_info.file_name);
		}
		else
		{
		    RestoreFile (file_info, false); // 2nd parameter: restore into parent directory
		}
		done = true;
	    }
	}
    }

    fclose (pf);
    EOF_CloseFile();
    
    if (done)
    {
	if (file_info.restored)
	{
	    LogPrn ("File %s created.", file_info.restored_file_name);
	}
	else
	{
	    LogPrn ("File %s not restored.", file_info.file_name);	
	}	
    }    
    else
    {
	sprintf (tmp_name, "%s%s", data_directory, FILEINFO_TXTDB);
        LogPrn ("File number %d not found in %s", file_number, tmp_name);
    }
}


//===========================================
void List()
{
    _FILE_INFO file_info;    
    FILE *pf;
    char line[1024];
    char tmp_name[PATH_MAX_LENGTH];
    
    
    sprintf (tmp_name, "%s%s", data_directory, FILEINFO_TXTDB);
    pf = fopen (tmp_name, "r");
    if (!pf)
    {
	fprintf (stderr, "Unable to open '%s': %s\n", tmp_name, strerror (errno));
	fprintf (stderr, "You have to complete step 1 & 2 before you can list files.\n");
	exit (EXIT_FAILURE);
    }

    while (!feof (pf))
    {
	if (LoadFileInfo (pf, file_info))
	{
	    FixFileName (file_info.file_name);
	    printf ("%" PRIu32 ": %s, %" PRIu64 " bytes, %" PRIu64 ", %s, Start block %" PRIu32 "%s%s%s\n", 
		    file_info.number,
		    file_info.file_name,
		    file_info.file_size,
		    file_info.file_time,
		    file_info.date_str,
		    file_info.blocks[0],
		    (file_info.compressed ? " _F_EOF_" : ""),
		    (file_info.has_extents_overflow ? " _F_COMPRESSED_" : ""),
		    (file_info.blocks[0] == 0 ? " _F_INVALID_START_BLOCK_" : "")
		    );
	}
    }
    fclose (pf);
}

void CSVExport (const char *csv_name)
{

    _FILE_INFO file_info;    
    FILE *pf;
    FILE *pf_csv;
    char line[1024];
    char tmp_name[PATH_MAX_LENGTH];
        
    sprintf (tmp_name, "%s%s", data_directory, FILEINFO_TXTDB);
    pf = fopen (tmp_name, "r");
    if (!pf)
    {
        fprintf (stderr, "Unable to open '%s': %s\n", tmp_name, strerror (errno));
        fprintf (stderr, "You have to complete step 1 & 2 before you can list files.\n");
        exit (EXIT_FAILURE);
    }

    pf_csv = fopen (csv_name, "w");
    if (!pf_csv)
    {
	fprintf (stderr, "Unable to create %s: %s\n", csv_name, strerror (errno));
	exit (EXIT_FAILURE);	
    }
    
    printf ("Exporting data...\n");

    fprintf (pf_csv, "\"Number\";\"File Name\";\"Parent ID\";\"Catalog ID\";\"File Size\";\"File RAW Time\";\"File Time\";\"Start block\";\"HFS+ Compressed\";\"EOF\";\"Invalid start block\"\n");

    while (!feof (pf))
    {
	if (LoadFileInfo (pf, file_info))
	{
	    FixFileName (file_info.file_name);
	    fprintf (pf_csv, "%d;\"%s\";%" PRIu32 ";%" PRIu32 ";%" PRIu64 ";%" PRIu64 ";\"%s\";%" PRIu32 ";\"%s\";\"%s\";\"%s\"\n", 
		    file_info.number,
		    file_info.file_name,
		    file_info.parent_id,
		    file_info.catalog_id,
		    file_info.file_size,
		    file_info.file_time,
		    file_info.date_str,
		    file_info.blocks[0],
		    (file_info.compressed ? "Yes" : "No"),
		    (file_info.has_extents_overflow ? "Yes" : "No"),
		    ((file_info.blocks[0] == 0) ? "Yes" : "")
		    );
	}
    }
    fclose (pf);
    fclose (pf_csv);
    
    printf ("Data exported to %s.\n", csv_name);
}



//===========================================

int main (int argc, char *argv[])
{

    int run = 0;
    int ret = 0;
        
    char file_name[4096];
    char file_name2[4096];

    char *vh_file   = NULL;
    char *directory = NULL;
    
    char log_file_name[64];

    bool keep_tmp  = false;
    bool force = false;

    char find_string[4096];
    char find_file[4096];

    int  one_file_number;
    uint64_t lba;

    int num_bytes;
    int args = 0;
    int start_number;
    
    bool force_extents = false;
    uint32_t extents_start_block;
    bool verbose = false;
    bool first = false;
    
    uint32_t start_block = 0;
    uint32_t num_blocks = 0;
    bool use_vh = false;
    bool log_append = true;


    signal (SIGINT, SigProc);	// trap CTRL-C

    offset = 0;
    destination_directory[0] = 0;
    strcpy (data_directory, "./hfsprescue-data/");


    if (HandleArgument (argv, argc, "-h") || HandleArgument (argv, argc, "--help"))
    {
	PrintHelp (HEADER);
	exit (EXIT_SUCCESS);    
    }

    printf (HEADER);

    if (argc == 1)
    {
	printf ("Use '--help' or 'man hfsprescue' for more information.\n");
	exit (EXIT_SUCCESS);    
    }


    if (HandleArgument (argv, argc, "--version"))
    {
	exit (EXIT_SUCCESS);    
    }


    if ( (ret = HandleArgument (argv, argc, "-s1", file_name)) )
    {
	if (ret == -1)
	{
	    printf ("Error: -s1 needs as second parameter a device node or file name.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_STEP1;
	strcpy (log_file_name, "s1.log");	
	log_append = false;
	args++;
    }
    
    else if (HandleArgument (argv, argc, "-s2"))
    {    
	run = RUN_STEP2;
	strcpy (log_file_name, "s2.log");
	args++;
    }

    else if ( (ret = HandleArgument (argv, argc, "-s3", file_name)) )
    {
	if (ret == -1)
	{
	    printf ("Error: -s3 needs as second parameter a device node or file name.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_STEP3;
	strcpy (log_file_name, "s3.log");
	args++;
    }
    
    else if (HandleArgument (argv, argc, "-s4"))
    {    
	run = RUN_STEP4;
	strcpy (log_file_name, "s4.log");
	args++;
    }

    else if (HandleArgument (argv, argc, "-s5"))
    {    
	run = RUN_STEP5;
	strcpy (log_file_name, "s5.log");
	args++;
    }

    else if (HandleArgument (argv, argc, "-s6"))
    {    
	run = RUN_STEP6;
	args++;
    }
    
    else if ( (ret = HandleArgument (argv, argc, "--one-file", file_name, &one_file_number)) ) // restore one file
    {
	if (ret == -1)
	{
	    printf ("Error: --one-file needs as second parameter a device node or file name and as third parameter the file number.\n"
		    "Use --list to get the file number. List example: hfsprescue --list|grep myfile.txt\n"
		    "See 'man hfsprescue' for more informations.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_RESTORE_ONE_FILE;
	strcpy (log_file_name, "onefile.log");
	args++;
    }

    else if (HandleArgument (argv, argc, "--list"))
    {
	run = RUN_LIST;
	args++;
    }

    else if ( (ret = HandleArgument (argv, argc, "--csv", file_name)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --csv needs as second parameter a file name.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_CSV_EXPORT;
	args++;
    }
    
    else if ( (ret = HandleArgument (argv, argc, "--find-eof", file_name)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --find-eof needs as second parameter a device node or file name.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_FIND_EXTENTSOVERFLOWFILE;
	strcpy (log_file_name, "find-eof.log");
	args++;
    }
    
    else if ( (ret = HandleArgument (argv, argc, "--find-vh", file_name)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --find-vh needs as second parameter a device node or file name.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_FIND_VOLUME_HEADER;
	strcpy (log_file_name, "find-vh.log");
	args++;
    }
    
    else if ( (ret = HandleArgument (argv, argc, "--find-avh", file_name)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --find-avh needs as second parameter a device node or file name.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_FIND_ALTERNATE_VOLUME_HEADER;
	strcpy (log_file_name, "find-avh.log");
	args++;
    }
    
    else if ( (ret = HandleArgument (argv, argc, "--find", file_name)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --find needs as second parameter a device node or file name.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_BYTE_SCAN;
	strcpy (log_file_name, "find.log");
	args++;
    }
        
    else if ( (ret = HandleArgument (argv, argc, "--extract-eof", file_name)) )
    {
	run = RUN_EXTRACT_EXTENTSOVERFLOWFILE;
	strcpy (log_file_name, "extract-eof.log");
	args++;
    }
    
    else if ( (ret = HandleArgument (argv, argc, "--extract-vh", file_name, &lba)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --extract-vh needs as second parameter the LBA number of the sector with the Volume Header.\n");
	    exit (EXIT_FAILURE);    
	}
	run = RUN_EXTRACT_VOLUME_HEADER;
	args++;
    }
    
    else if (HandleArgument (argv, argc, "--remove-empty-dirs"))
    {
	run = RUN_REMOVE_EMPTY_DIRECTORIES;
	args++;
    }
    
    


    // additional parameters
    
    if ( (ret = HandleArgument (argv, argc, "-o", &offset)) )
    {    
	if (ret == -1)
	{
	    printf ("Error: -o needs as second parameter a value (offset).\n");
	    exit (EXIT_FAILURE);    
	}
    }

    if ( (ret = HandleArgument (argv, argc, "-b", &block_size)) )
    {    
	if (ret == -1)
	{
	    printf ("Error: -b needs as second parameter a value (block size), usually 4096.\n");
	    exit (EXIT_FAILURE);    
	}
	force_block_size = true;
    }

    if (HandleArgument (argv, argc, "-f") || HandleArgument (argv, argc, "--force"))
    {    
	force = true;
    }

    if (HandleArgument (argv, argc, "-k"))
    {
	keep_tmp = true;
    }

    if (HandleArgument (argv, argc, "--ignore-eof"))
    {
	ignore_eof = true;
    }

    if (HandleArgument (argv, argc, "--force-eof"))
    {
	force_eof = true;
    }

    if (HandleArgument (argv, argc, "--ignore-file-error"))
    {
	ignore_file_create_error = true;
    }

    if (HandleArgument (argv, argc, "--ignore-file-error"))
    {
	ignore_blocks = true;
    }

    if (HandleArgument (argv, argc, "-v") || HandleArgument (argv, argc, "--verbose"))
    {
	verbose = true;
    }

    if (HandleArgument (argv, argc, "--first"))
    {
	first = true;
    }

    if (HandleArgument (argv, argc, "--alternative"))
    {
	param_alternative_file_name = true;
    }

    if ( (ret = HandleArgument (argv, argc, "-d", destination_directory)) )
    {
	if (ret == -1)
	{
	    printf ("Error: -d needs as second parameter the destination directory.\n");
	    exit (EXIT_FAILURE);    
	}
	if (destination_directory[strlen (destination_directory) - 1] != '/')
	{
	    strcat (destination_directory, "/");
	}
	sprintf (data_directory, "%shfsprescue-data/", destination_directory);
	MKDir (destination_directory);

    }
    

    if ( (ret = HandleArgument (argv, argc, "-c", &start_number)) )
    {    
	if (ret == -1)
	{
	    printf ("Error: -c needs as second parameter a value (number of file to continue).\n");
	    exit (EXIT_FAILURE);    
	}
    }

    if ( (ret = HandleArgument (argv, argc, "-fs", find_string)) )
    {    
	if (run != RUN_BYTE_SCAN)
	{
	    printf ("Error: -fs works only with --find\n");
	    exit (EXIT_FAILURE);    	
	}
	
	if (ret == -1)
	{
	    printf ("Error: -fs needs as second parameter a string.\n");
	    exit (EXIT_FAILURE);    
	}
    }
    
    std::vector <char *> file_names;
    if ( (ret = HandleArgument (argv, argc, "-ff", &file_names)) ) //, &num_bytes)) )
    {    
	if (run != RUN_BYTE_SCAN)
	{
	    printf ("Error: -ff works only with --find\n");
	    exit (EXIT_FAILURE);    	
	}
	
	if (file_names.size() >= 1)
	{
	    num_bytes = atoi (file_names[0]);
	}
	else
	{
	    num_bytes = 0;
	}
	
	if ((ret == -1) || (file_names.size() < 2) || (num_bytes == 0))
	{
	    printf ("Error: -ff needs at least 2 parameters.\n"
	            "  Parameter 1: Scan number of bytes.\n"
	            "  Other parameters are file names.\n"
	            "Example: -ff 4096 file1.jpg ../file2.dat\n"
	            );
	    exit (EXIT_FAILURE);    
	}	
    }


    if ( (ret = HandleArgument (argv, argc, "--slash")) )
    {    
	if ( (run != RUN_LIST) && (run != RUN_CSV_EXPORT) )
	{
	    printf ("Error: --slash works only with --list or --csv\n");
	    exit (EXIT_FAILURE);    	
	}
	slash_convert = true;	
    }

    if ( (ret = HandleArgument (argv, argc, "--dir", &directory)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --dir needs as second parameter a directory name.\n");
	    exit (EXIT_FAILURE);    
	}
    }

    if ( (ret = HandleArgument (argv, argc, "--vh-file", &vh_file)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --vh-file needs as second parameter a file name.\n");
	    exit (EXIT_FAILURE);    
	}
    }

    if ( (ret = HandleArgument (argv, argc, "--eof-file", &eof_file)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --eof-file needs as second parameter a file name.\n");
	    exit (EXIT_FAILURE);    
	}
    }

    if ( (ret = HandleArgument (argv, argc, "--file-list", &file_list)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --file-list needs as second parameter a file name.\n");
	    exit (EXIT_FAILURE);    
	}    
    }

    if ( (ret = HandleArgument (argv, argc, "--file-list-csv", &file_list)) )
    {
	if (ret == -1)
	{
	    printf ("Error: --file-list-csv needs as second parameter a file name.\n");
	    exit (EXIT_FAILURE);    
	}
	is_file_list_csv = true;
    }

    if ( (ret = HandleArgument (argv, argc, "--utf8len", &utf8_len)) )
    {
	if ((ret == -1) || (utf8_len < 1) || (utf8_len > 5))
	{
	    printf ("Error: --utf8len needs as second parameter a value between 1 and 5.\n");
	    printf ("  1 is default. It should be used when you have no files with asian file names.\n");
	    printf ("  2 should be used when your file names have asian chars.\n");
	    printf ("  3-5 should not be used.\n");
	    exit (EXIT_FAILURE);    
	}
	force_utf8_len = true;
    }
    else
    {
	utf8_len = 1; // Default value
    }

    int p_days;
    if ( (ret = HandleArgument (argv, argc, "--future-days", &p_days)) )
    {
	if ((ret == -1) || (p_days < 0))
	{
	    printf ("Error: --future-days needs as second parameter a day larger or equal 0.\n");
	    printf ("  Default is 7 days.\n");
	    exit (EXIT_FAILURE);    
	}
	force_future_date = true;
	future_date_tolerance = 60 * 60 * 24 * p_days; // Tolerance in seconds.
    }
    else
    {
	// Be gentle and allow a tolerance of one week in the future.
	future_date_tolerance = 60 * 60 * 24 * 7; // Default tolerance in seconds.
    }



    if (run == RUN_EXTRACT_EXTENTSOVERFLOWFILE)
    {
	uint32_t last_block;

	if ( (ret = HandleArgument (argv, argc, "--start-block", &start_block)) )
	{    
	    if (ret == -1)
	    {
	        printf ("Error: --start-block needs as second parameter a value.\n");
	        exit (EXIT_FAILURE);    
	    }
	}

	if ( (ret = HandleArgument (argv, argc, "--num-blocks", &num_blocks)) )
	{    
	    if (ret == -1)
	    {
	        printf ("Error: --num-blocks needs as second parameter a value.\n");
	        exit (EXIT_FAILURE);    
	    }
	}
    
	if ( (ret = HandleArgument (argv, argc, "--last-block", &last_block)) )
	{    
	    if (ret == -1)
	    {
	        printf ("Error: --last-block needs as second parameter a value.\n");
	        exit (EXIT_FAILURE);    
	    }
	}
	
	int test = 0;
	
	if (start_block == 0) test = 2; // --start-block is required
	if (num_blocks  == 0) test++;
	if (last_block  == 0) test++;
	
	if (test == 4) // No parameters set
	{
	    use_vh = true;	    
	}
	else
	{
	    if (test > 1)
	    {
		printf ("Error: You have to set a start block and num blocks or a start block and last block.\n");
		exit (EXIT_FAILURE);    	
	    }
	
	    if (last_block > 0)
	    {
		if (last_block <= start_block)
		{
		    printf ("Error: The last block must be after the start block!\n");
		    exit (EXIT_FAILURE);    		    
		}
	
		num_blocks = last_block - start_block + 1;
	    }
	}
    }


    
    if (args != 1) 
    {
	printf ("Parameter error. Run hfsprescue -h\n");
	exit (EXIT_FAILURE);
    }

    // Parameter processing end


    //=========================
    // If needed, create data directory    

    if (!FileExist (data_directory))
    {
	MKDir (data_directory);
    }
    

    //========================
    
    if (run == RUN_STEP6)
    {
	Cleanup (keep_tmp);
	exit (EXIT_SUCCESS);
    }
    
    if (run == RUN_LIST)
    {
	List();
	exit (EXIT_SUCCESS);
    }
    
    if (run == RUN_CSV_EXPORT)
    {
	CSVExport (file_name);
	exit (EXIT_SUCCESS);
    }
    
    if (run == RUN_EXTRACT_VOLUME_HEADER) 
    {
	VH_Extract (file_name, lba, vh_file);
	exit (EXIT_SUCCESS);
    }

    if (run == RUN_REMOVE_EMPTY_DIRECTORIES) 
    {
	if (RemoveEmptyDirectories (directory, force))
	{
	    exit (EXIT_SUCCESS);
	}
	else
	{
	    exit (EXIT_FAILURE);	
	}
    }

    //========================
    // step 1, 2, 3, 4 and 5
    //
    // create log file
    
    char tmp_fname[1024];
    sprintf (tmp_fname, "%s%s", data_directory, log_file_name);
    pf_log = LogFileOpen (tmp_fname, log_append ? "a" : "w");

    if (!pf_log)
    {
	printf ("Error creating file %s: %s\n", tmp_fname, strerror (errno));
	return EXIT_FAILURE;
    }
    else
    {
	fprintf (pf_log, "hfsprescue " PACKAGE_VERSION " by Elmar Hanlhofer https://www.plop.at\n\n");
    }

    LogCommand (pf_log, argv, argc);    
    time (&time_start);
    LogTimePrn ("Start: ", time_start);
    LogPrn ("");


    if (run == RUN_BYTE_SCAN) 
    {
	ByteScan (file_name, find_string, &file_names, num_bytes, offset);
	LogClose();
	exit (EXIT_SUCCESS);
    }

    if (run == RUN_STEP2)
    {   
	Store ("utf8len", utf8_len); // Backup for step 4 
	CleanupFileDatabase();
        PrintInfoStep3 (argv, argc);
        LogClose();
        exit (EXIT_SUCCESS);
    }

    if (run == RUN_STEP4)
    {    
	RestoreDirectories();
	PrintInfoStep5 (argv);
	LogClose();
	exit(EXIT_SUCCESS);
    }

    if (run == RUN_STEP5)
    {
	char restored_dir[MAX_FILE_NAME_CHARS];
        sprintf (restored_dir, "%srestored", destination_directory);
	MoveFiles (restored_dir, "newroot");
	PrintInfoStep6 (argv);
	LogClose();
	exit (EXIT_SUCCESS);
    }

    if (run == RUN_FIND_VOLUME_HEADER) 
    {
	if (first)
	{
	    LogPrn ("*** Stop searching after the first Volume Header has been found.\n");
	}
	
	VH_FindHFSVolumeHeader (file_name, verbose, first, force);
	LogClose();
	exit (EXIT_SUCCESS);
    }

    if (run == RUN_FIND_ALTERNATE_VOLUME_HEADER) 
    {
	if (first)
	{
	    LogPrn ("*** Stop searching after the first Volume Header has been found.\n");
	}
	
	VH_FindHFSAlternateVolumeHeader (file_name, verbose, first, force);
	LogClose();
	exit (EXIT_SUCCESS);
    }


    //========================
    // begin step 1 and step 3
    
    vh = (HFSPlusVolumeHeader *)malloc (sizeof (HFSPlusVolumeHeader));
    

    fd = open64 (file_name, O_RDONLY);
    if (fd < 0)
    {
	fprintf (stderr, "Can not open %s: %s\n", file_name, strerror (errno));
	LogClose();
	exit (EXIT_FAILURE);
    }

    if (vh_file == NULL)
    {
	// Read from disk/file
	pread64 (fd, vh, sizeof (HFSPlusVolumeHeader), 1024 + offset);
    }
    else
    {
	LogPrn ("*** Using external Volume Header file '%s'.", vh_file);
	
	FILE *pf;
	pf = fopen (vh_file, "r");
	if (!pf)
	{
	    fprintf (stderr, "Can not open %s: %s\n", vh_file, strerror (errno));
	    exit (EXIT_FAILURE);	
	}
	fread (vh, sizeof (HFSPlusVolumeHeader), 1, pf);	
	fclose (pf);	
    }

    // Swap bytes (MSB/LSB)
    VH_SwapBytes (vh);
    
    if (force_block_size)
    {
	vh->blockSize = block_size;
	LogPrn ("*** Force block size: %" PRIu32, block_size);
    }
    
    if (offset > 0)
    {
	LogPrn ("*** Using offset %" PRIu64 " (%" PRIu64 " MB)%s", 
		offset, 
		offset / 1024 / 1024,
		( offset % 512 != 0 ? " Warning: It's not a sector boundary!" : "") );
    }
    
    if (ignore_blocks)
    {
	LogPrn ("*** Ignore blocks");    
    }

    if (ignore_file_create_error)
    {
	LogPrn ("*** Ignore file create error");    
    }

    // Print Volume Header Data
    VH_PrintData (vh);
    
    block_size = vh->blockSize;
    fs_size = lseek64 (fd, 0, SEEK_END);

    if (fs_size == 0) // fal back to ioctl
    {
	fs_size = IoctlGetDiskSize (fd);
    }

    LogPrn ("Total size:                     %" PRIu64 " GB\n", fs_size / 1024 / 1024 / 1024);
    
    if (fs_size == 0)
    {
	LogPrn ("Error: Unable to detect device size!");
    }

    if (!ValidBlockSize (vh->blockSize) && !force_block_size)
    {
	printf ("\nWarning: Unusual block size! Use -b %u to force this block size.\n\n", vh->blockSize);
	printf ("*** A block size of 4096 is standard.\n*** Use -b 4096 for a standard block size.\n");
	LogClose();
	exit (EXIT_FAILURE);    
    }
    
    if (ignore_eof)
    {
	LogPrn ("*** Ignoring the ExtentsOverflowFile. Restoring of strong framented files is not possible!");    
    }
    

    lseek64 (fd, 0, SEEK_SET);

    if (run == RUN_STEP1) 
    {	
#ifndef __APPLE__

#endif
	//Store ("file_size", fs_size); // Backup for step 2
    	if (FindFiles (force, vh, fs_size) == EXIT_SUCCESS)
    	{
    	    PrintInfoStep2 (argv, argc);
    	}
    	else
    	{
    	    LogClose();
    	    exit (EXIT_FAILURE);
    	}
    }
    
    if (run == RUN_STEP3) 
    {
	if (start_number < 1) start_number = 1;
	RestoreFiles (start_number);
	PrintInfoStep4 (argv);
    }


    if (run == RUN_RESTORE_ONE_FILE) 
    {
	RestoreOneFile (one_file_number);
    }

    if (run == RUN_FIND_EXTENTSOVERFLOWFILE) 
    {
	FindExtentsOverflowFile (file_name);
    }

    if (run == RUN_EXTRACT_EXTENTSOVERFLOWFILE) 
    {
	bool ok = false;

	if (use_vh)
	{
	    ok = ExtractExtentsOverflowFileAutomatic (eof_file, true); // file name, wait on warning
	}
	else
	{
	    ok = ExtractExtentsOverflowFile (fd, eof_file, start_block, num_blocks);
	}

	if (!ok)
	{
	    LogClose();
	    exit (EXIT_FAILURE);
	}
    }



    LogClose();
    close (fd);

    return EXIT_SUCCESS;
}
