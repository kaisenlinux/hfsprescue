/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  https://www.plop.at
 *
 * HFS+ Volume Header routines
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
    


#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "vh.h"
#include "tools.h"
#include "log.h"
#include "ioctl.h"



bool first_hfs_volume_header = true;
extern uint64_t offset;
extern char destination_directory[];

char * vh_FormatAsDate ( uint32_t secsSinceMacEpoch, bool isCreateDate );
size_t vh_HFSPlusEpochToUnixEpochDiff();

void VH_Scan (char *buffer, int size, uint64_t fpos, bool verbose, bool stop_after_first, bool force, bool show_partition_start)
{

    for (int ofs = 0; ofs < size; ofs += 512)
    {
	bool check = false;
    
	/*    HFS is not supported by hfsprescue. Maybe some time later.
	
	if (strncmp (buffer + ofs, "BD", 2) == 0)			// HFS Volume
	{
	    if (verbose)
	    {
    		LogPrn ("Info: Signature 'BD' (HFS Volume) found at offset %lld (0x%llx)", fpos + ofs, fpos + ofs);
    	    }
	    check = true;
	}
	else 
	*/
	
	if (strncmp (buffer + ofs, "H+", 2) == 0)		// HFS+ Volume
	{
	    if (verbose)
	    {
    		LogPrn ("Info: Signature 'H+' (HFS+ Volume) found at offset %" PRIu64 " (0x%llx)", fpos + ofs, fpos + ofs);
    	    }
    	    
	    if ((buffer[ofs + 2] == 0x00) && (buffer[ofs + 3] == 0x04))	// Version 4
	    {
		if (verbose)
		{
    		    LogPrn ("Info: Volume format version 4");
    		}
		check = true;
	    }
	}
	
	else if (strncmp (buffer + ofs, "HX", 2) == 0)		// HFSX Volume
	{
	    if (verbose)
	    {
	        LogPrn ("Info: Signature 'HX' (HFSX Volume) found at offset %" PRIu64 " (0x%llx)", fpos + ofs, fpos + ofs);
	    }
	    
	    if ((buffer[ofs + 2] == 0x00) && (buffer[ofs + 3] == 0x05))	// Version 5
	    {
	        if (verbose)
		{
    		    LogPrn ("Info: Volume format version 5");
    		}
		check = true;
	    }	    
	}
	
	if (check)
	{
	    bool display = true;
	    
	    
	    /* HFS is not supported by hfsprescue. Maybe some time later.
	    if (strncmp (buffer + ofs + 8, "8.10", 4) == 0) 
	    {
		if (verbose)
		{
		    LogPrn ("Info: Last mount by Mac OS 8.1 to 9.2.2.");
		}
	    }
	    else 
	    */
	    
	    if (strncmp (buffer + ofs + 8, "10.0", 4) == 0) 
	    {
		if (verbose)
		{
		    LogPrn ("Info: Last mount by Mac OS X.");
		}
	    }
	    else if (strncmp (buffer + ofs + 8, "HFSJ", 4) == 0)   // Journaled Volume 
	    {
		if (verbose)
		{
		    LogPrn ("Info: Last mount by Mac OS X.");
		}
	    }
	    else if (strncmp (buffer + ofs + 8, "H+Lx", 4) == 0) 
	    {
		if (verbose)
		{
		    LogPrn ("Info: Last mount by Linux.");
		}
	    }
    	    else
    	    {
    		if (verbose)
		{
    		    LogPrn ("Info: Last mount was not done by Mac OS X.");
    		    if (!force)
    		    {
    		        LogPrn ("Info: Skipping this Volume Header. Use --force to display this Volume Header too.");
    		    }
    		}
		display = false;	
		
		if (force)
		{
		    LogPrn ("Info: Force parameter has been used. Skip deactivated.");
		    display = true;		
		}
    	    }


	    if (display)
	    {
		LogPrn ("\n============================================");
		LogPrn ("A Volume Header has been found.\n");
		uint64_t position;
		position = fpos + ofs;
		
		if (!first_hfs_volume_header)
		{
		    Log    ("Note: If you had only one HFS+ partition on your drive, then this can be a backup of the Volume Header.");
		    printf ("Note: If you had only one HFS+ partition on your drive, then this can be a backup of the Volume Header. Check the log file 'hfsprescue-data/hfsprescue-find-vh.log' for the first entry.\n");
		}
		
		if (show_partition_start)
		{
		    LogPrn ("Partition start:     %" PRIu64 " (Byte), 0x%llx, %" PRIu64 " (LBA Sector), at %" PRIu64 " MB", position - 1024, position - 1024, (position - 1024) / 512, (position - 1024) / 1024 / 1024);
		}
		
		LogPrn ("Volume Header start: %" PRIu64 " (Byte), 0x%llx, %" PRIu64 " (LBA Sector), at %" PRIu64 " MB\n", position, position, position / 512, position / 1024 / 1024);
		
		VH_SwapBytes ((HFSPlusVolumeHeader *)(buffer + ofs));
		VH_PrintData ((HFSPlusVolumeHeader *)(buffer + ofs));
		LogPrn("");
		
		if (!show_partition_start) // --find-avh mode
		{
		    uint64_t possible;
		    HFSPlusVolumeHeader *l_vh = (HFSPlusVolumeHeader *)(buffer + ofs);

		    possible = position - ((uint64_t)l_vh->blockSize * (uint64_t)l_vh->totalBlocks - 1024);

		    LogPrn ("Possible partition start: %" PRIu64 " (Byte), 0x%llx, %" PRIu64 " (LBA Sector), at %" PRIu64 " MB", possible, possible, possible / 512, possible / 1024 / 1024);
		}
		
		
		
		first_hfs_volume_header = false;

		if (stop_after_first) // stop, we want to see only the first volume header
		{
		    return;
		}
	
	    }
	}
    }
}

void VH_FindHFSVolumeHeader (char *file, bool verbose, bool stop_after_first, bool force)
{
    FILE *pf;
    char buffer[1024 * 1024];
    int mb = 0;
    uint64_t fpos = offset;
    int fd;
    bool quit = false;
    
    if (offset > 0)
    {
        LogPrn ("*** Skipping %" PRIu64 " bytes (%" PRIu64 " MB)", offset, offset / 1024 / 1024);
    }
    
    fd = open64 (file, O_RDONLY);
    if (fd < 0)
    {
	fprintf (stderr, "Unable to open %s: %s\n", file, strerror (errno));
	exit (EXIT_FAILURE);
    }    

    while (pread64 (fd, buffer, sizeof (buffer), fpos) || quit)
    {
	VH_Scan (buffer, sizeof (buffer), fpos, verbose, stop_after_first, force, true);
	
	if (stop_after_first && !first_hfs_volume_header) // stop, we want to see only the first volume header
	{
	    quit = true;
	    break;
	}
	    
	mb++;	
	printf ("\rScanned %d MB.", mb);
	fflush (stdout);
	fpos += 1024 * 1024;
    }
    printf ("\n");
    close (fd);
}

void VH_FindHFSAlternateVolumeHeader (char *file, bool verbose, bool stop_after_first, bool force)
{
    FILE *pf;
    char buffer[1024 * 1024];
    int mb = 0;
    uint64_t fpos;
    int fd;
    bool quit = false;
    uint64_t fs_size;

    LogPrn ("Searching backwards for the Alternate Volume Header.");


    if (offset > 0)
    {
        LogPrn ("*** Skipping the last %" PRIu64 " bytes (%" PRIu64 " MB)", offset, offset / 1024 / 1024);
    }
    
    
    fd = open64 (file, O_RDONLY);
    if (fd < 0)
    {
	fprintf (stderr, "Unable to open %s: %s\n", file, strerror (errno));
	exit (EXIT_FAILURE);
    }    

    fs_size = lseek64 (fd, 0, SEEK_END);
        
    if (fs_size == 0) // fal back to ioctl
    {
        fs_size = IoctlGetDiskSize (fd);
    }
    
    if (fs_size == 0)
    {
        LogPrn ("Error: Unable to detect device size!");
	exit (EXIT_FAILURE);
    }
        


    fpos = fs_size - offset - sizeof (buffer);


    while (pread64 (fd, buffer, sizeof (buffer), fpos) || quit)
    {
	VH_Scan (buffer, sizeof (buffer), fpos, verbose, stop_after_first, force, false);
	
	if (stop_after_first && !first_hfs_volume_header) // stop, we want to see only the first volume header
	{
	    quit = true;
	    break;
	}
	    
	mb++;	
	printf ("\rScanned %d MB.", mb);
	fflush (stdout);
	fpos -= 1024 * 1024;
    }
    printf ("\n");
    close (fd);
}


void VH_SwapBytes (HFSPlusVolumeHeader *vh)
{
    Swap32 (&vh->createDate);
    Swap32 (&vh->modifyDate);
    Swap32 (&vh->backupDate);
    Swap32 (&vh->checkedDate);

    Swap32 (&vh->fileCount);
    Swap32 (&vh->folderCount);
    Swap32 (&vh->blockSize);
    Swap32 (&vh->totalBlocks);

    Swap64 (&vh->extentsFile.logicalSize);
    Swap32 (&vh->extentsFile.clumpSize);
    Swap32 (&vh->extentsFile.totalBlocks);

    for (int i = 0; i < 8; i++)
    {
        Swap32 (&vh->allocationFile.extents[i].startBlock);
        Swap32 (&vh->allocationFile.extents[i].blockBlock);

        Swap32 (&vh->extentsFile.extents[i].startBlock);
        Swap32 (&vh->extentsFile.extents[i].blockBlock);

        Swap32 (&vh->catalogFile.extents[i].startBlock);
        Swap32 (&vh->catalogFile.extents[i].blockBlock);
    }
}

void VH_PrintData (HFSPlusVolumeHeader *vh)
{
   LogPrn ("Signature:                      0x%02x, %c%c%s",vh->signature,
                                                             vh->signature & 0xff, vh->signature >> 8 & 0xff,
                                                             ( (vh->signature == 0x2b48) || (vh->signature == 0x5848) ? "" : " (Unknown)")
                                                             );

    char lmv[5];
    strncpy (lmv, vh->lastMountedVersion, 4);
    lmv[4] = 0;
    LogNoNLPrn ("LastMountedVersion:             %s, last mount ", lmv);
    if (strcmp (lmv, "10.0") == 0)
    {
        LogPrn ("by Mac OS X.");
    }
    else if (strcmp (lmv, "HFSJ") == 0)   // Journaled Volume 
    {
        LogPrn ("by Mac OS X.");
    }
    else if (strcmp (lmv, "H+Lx") == 0)
    {
        LogPrn ("by Linux.");
    }
    else
    {
        LogPrn ("was not done by Mac OS X.");
    }

    LogPrn     ("CreateDate:                     %s", vh_FormatAsDate(vh->createDate, true) );  // works as UID; is not UTC
    LogPrn     ("ModifyDate:                     %s", vh_FormatAsDate(vh->modifyDate, false) );
    LogPrn     ("BackupDate:                     %s", vh_FormatAsDate(vh->backupDate, false) );
    LogPrn     ("CheckedDate:                    %s", vh_FormatAsDate(vh->checkedDate, false) );

    LogPrn     ("FileCount:                      %" PRIu32 , vh->fileCount);
    LogPrn     ("DirCount:                       %" PRIu32 , vh->folderCount);
    LogPrn     ("BlockSize:                      %" PRIu32 , vh->blockSize);
    LogPrn     ("TotalBlocks:                    %" PRIu32 , vh->totalBlocks);
    LogPrn     ("AllocationFile StartBlock:      %" PRIu32 , vh->allocationFile.extents[0].startBlock);
    LogPrn     ("ExtentsOverflowFile StartBlock: %" PRIu32 , vh->extentsFile.extents[0].startBlock);
    LogPrn     ("CatalogFile StartBlock:         %" PRIu32 , vh->catalogFile.extents[0].startBlock);
}


/*
    --extract-vh <device> <lba sector> --vh-file <file_name>
*/

void VH_Extract (char *file, uint64_t lba, const char *extract_file_name)
{
    FILE *pf;
    char buffer[512];
    uint64_t fpos;
    int fd;
    char default_file_name[MAX_FILE_NAME_CHARS];
    char restored_dir[MAX_FILE_NAME_CHARS];

    sprintf (default_file_name, "%srestored/%s", destination_directory, VOLUME_HEADER_FILE_NAME);
    sprintf (restored_dir, "%srestored", destination_directory);
    
    fd = open64 (file, O_RDONLY);
    if (fd < 0)
    {
	fprintf (stderr, "Unable to open %s: %s\n", file, strerror (errno));
	exit (EXIT_FAILURE);
    }

    if (extract_file_name == NULL)
    {
	extract_file_name = default_file_name;
	
	MKDir (restored_dir , ALLOW_EXIST);
    }

    pf = fopen (extract_file_name, "w");    
    if (!pf)
    {
	close (fd);
	
	fprintf (stderr, "Unable to create %s: %s\n", extract_file_name, strerror (errno));
	exit (EXIT_FAILURE);    
    }

    
    fpos = lba * 512;

    printf ("Reading sector LBA %" PRIu64 "\n", lba);

    pread64 (fd, buffer, sizeof (buffer), fpos);
    fwrite (buffer, sizeof (buffer), 1, pf);
        
    fclose (pf);
    close (fd);

    printf ("Volume Header extracted to '%s'.\n", extract_file_name);
}



/* From "HFS Plus Dates":
 *     https://developer.apple.com/library/archive/technotes/tn/tn1150.html#HFSPlusDates
 *
 * <quote>
 *     HFS Plus stores dates in several data structures, including the
 *     volume header and catalog records. These dates are stored in unsigned
 *     32-bit integers (UInt32) containing the number of seconds since
 *     midnight, January 1, 1904, GMT. This is slightly different from HFS,
 *     where the value represents local time.
 *
 *     The maximum representable date is February 6, 2040 at 06:28:15 GMT.
 *
 *     The date values do not account for leap seconds. They do include a
 *     leap day in every year that is evenly divisible by four. This is
 *     sufficient given that the range of representable dates does not
 *     contain 1900 or 2100, neither of which have leap days.
 *
 *     The implementation is responsible for converting these times to the
 *     format expected by client software. For example, the Mac OS File
 *     Manager passes dates in local time; the Mac OS HFS Plus
 *     implementation converts dates between local time and GMT as
 *     appropriate.
 *
 *     Note:
 *     The creation date stored in the Volume Header is NOT stored in GMT;
 *     it is stored in local time. The reason for this is that many
 *     applications (including backup utilities) use the volume's creation
 *     date as a relatively unique identifier. If the date was stored in
 *     GMT, and automatically converted to local time by an implementation
 *     (like Mac OS), the value would appear to change when the local time
 *     zone or daylight savings time settings change (and thus cause some
 *     applications to improperly identify the volume). The use of the
 *     volume's creation date as a unique identifier outweighs its use as a
 *     date. This change was introduced late in the Mac OS 8.1 project.
 * </quote>
 *
 * Our 'isCreateDate' flag allows us to provide special-case handling for the
 * 'createDate' field of the volume header.
 *
 * Returned string uses buffer that is internal to the function, and
 * overwritten on each call.
 */
char * vh_FormatAsDate ( uint32_t secsSinceMacEpoch, bool isCreateDate )
{
    static char pbuf[128];
    memset(&pbuf, 0, sizeof(pbuf));

    size_t epochs_diff = vh_HFSPlusEpochToUnixEpochDiff();

    // Translate seconds since HFS+ epoch to seconds since Unix epoch. This
    // gives us a value that will work with the <time.h> functions.
    //
    time_t as_unix_secs = (time_t)(secsSinceMacEpoch - epochs_diff);

    struct tm components_tm;
    memset(&components_tm, 0, sizeof(struct tm));

    // XXX: This handling is a slam dunk for the UTC dates, but why it is
    //      (arguably) legit for the 'createDate' field requires an
    //      explanation.
    //
    // Here's the deal: createDate is stored in local time "as an identifier",
    // as noted in the Apple-quoted docs above. So we know that that value
    // strictly speaking is not UTC, but we also do not know which timezone
    // the date value was created in.
    //
    // We'll create a date-looking value for it in the least lossy way --
    // which is to treat the value as UTC even though we know that it is
    // not. Later, when we print the value for display to the user, we will
    // omit the timezone offset indicator, and will include the raw hex
    // presentation of the value as a "unique id". This conveys as much
    // information as possible without suggesting that we know something that
    // we do not.
    //
    gmtime_r(&as_unix_secs, &components_tm);

    if (isCreateDate)
    {
        strftime(pbuf, sizeof(pbuf), "%Y-%m-%d %H:%M:%S", &components_tm );

        char scratch[64];
        memset(&scratch, 0, sizeof(scratch));
        snprintf(scratch, sizeof(scratch), "  [UID: 0x%x]", secsSinceMacEpoch);

        strncat(pbuf, scratch,
                ((sizeof(pbuf)) - (strnlen(pbuf, sizeof(pbuf))) - 1) );
    }
    else
    {
        strftime(pbuf, sizeof(pbuf), "%Y-%m-%d %H:%M:%S%z", &components_tm);
    }

    return pbuf;
}

size_t vh_HFSPlusEpochToUnixEpochDiff()
{
/*
    // construct struct tm instances, one for the HFS+ epoch, and one for the Unix epoch
    struct tm hfsp_epoch_tm;
    struct tm unix_epoch_tm;
    memset(&hfsp_epoch_tm, 0, sizeof(struct tm));
    memset(&unix_epoch_tm, 0, sizeof(struct tm));
    strptime("1904-01-01 00:00:00+0000", "%Y-%m-%d %H:%M:%S%z", &hfsp_epoch_tm);
    strptime("1970-01-01 00:00:00+0000", "%Y-%m-%d %H:%M:%S%z", &unix_epoch_tm);

    // translate our tm structs to time_t (both represent seconds since the
    // Unix epoch); the Unix value will be zero, of course.
    time_t hfsp_epoch = timegm( &hfsp_epoch_tm );
    time_t unix_epoch = timegm( &unix_epoch_tm );

    // NOTE: epochs_diff will always be: 2082844800
    size_t epochs_diff = difftime( unix_epoch, hfsp_epoch );  // implicit: double -> size_t
*/
    size_t epochs_diff = 2082844800L;
    return epochs_diff;
}
