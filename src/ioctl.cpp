/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 */


#include "config.h"

#include "ioctl.h"
#include "apple.h"

#include <inttypes.h>

#ifdef __APPLE__

#include <sys/disk.h>
#include <sys/ioctl.h>

#endif

#include <fcntl.h>

#include "log.h"


uint64_t IoctlGetDiskSize (int fd)
#ifdef __APPLE__
// lseek doesnt tell position on block devices on Mac OS X.
// I found this for QEMU on Mac OS X. I modified it for my needs.
{
    uint64_t sectors = 0;
    uint32_t sector_size = 0;
    int ret;
    
    /* Query the number of sectors on the disk */
    ret = ioctl(fd, DKIOCGETBLOCKCOUNT, &sectors);
    if (ret == -1) 
    {
	LogPrn ("IOCTL: Unable to read the number of sectors!");
        return 0;
    }

    /* Query the size of each sector */
    ret = ioctl(fd, DKIOCGETBLOCKSIZE, &sector_size);
    if (ret == -1) 
    {
	LogPrn ("IOCTL: Unable to get sector size!");
        return 0;
    }
    return sectors * sector_size; // return device size in bytes
}
#else
{
    LogPrn ("Error: IOCTL disk size detection is not implemented for your OS. Please contact the developer to implement this feature!");
    return 0;

}
#endif
