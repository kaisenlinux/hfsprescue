/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 *
 */


#include "config.h"

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>

#include "tools.h"

int SpecialPrint (const char *txt, int y);

void PrintHelp (const char *header)
{   
    const char help_txt[] = {
            "Hfsprescue scans a damaged image file or partition that is formatted with "
	    "HFS+. You can restore your files and directories, even when it's not possible "
	    "to mount it with your operating system. Your files and directories will "
	    "be stored in the directory './restored' in your current directory. The HFS+ "
	    "file or partition will not be changed. So you need enough space to copy "
	    "out the files from the HFS+ file system. Hfsprescue supports HFS+ compression (resource fork).\n"
	    "\n"
	    "You have to complete 6 steps to restore your files:\n"
	    "\n"
	    "1) Scan for your files.\n"
	    "\n"
	    "hfsprescue -s1 <device node|image file>\n"
	    "                  [-b <block size>]\n"
	    "                  [-o <offset in bytes>]\n"
	    "                  [-d <working / destination directory>]\n"
	    "                  [-f|--force]\n"
	    "\n"
	    "2) Cleanup file database.\n"
	    "\n"
	    "hfsprescue -s2 [--utf8len <value 1 to 5>] [--future-days <days>] [-d <working directory>]\n"
	    "\n"
	    "3) Restore your files.\n"
	    "\n"
	    "hfsprescue -s3 <device node|image file>\n"
	    "                  [-b <block size>]\n"
	    "                  [-o <offset in bytes>]\n"
	    "                  [-d <working directory>]\n"
	    "                  [--vh-file <file name>]\n"
	    "                  [--eof-file <file name>]\n"
	    "                  [--ignore-eof]\n"
	    "                  [-c <file number>]\n"
	    "                  [--file-list <file name>]\n"
	    "                  [--file-list-csv <file name>]\n"
	    "                  [--alternative]\n"
	    "                  [--ignore-blocks]\n"
	    "                  [--ignore-file-error]\n"
	    "\n"
	    "4) Restore your directory structure.\n"
	    "\n"
	    "hfsprescue -s4 [-d <working directory>]\n"
	    "\n"
	    "5) Move the restored files to the correct directories.\n"
	    "\n"
	    "hfsprescue -s5 [-d <working directory>]\n"
	    "\n"
	    "6) Last step, finalize and cleanup.\n"
	    "\n"
	    "hfsprescue -s6 [-k] [-d <working directory>]\n"
	    "\n"
	    "hfsprescue will guide you through every step and is telling you the command for "
	    "the next step. See the 'man hfsprescue' for more informations.\n"
	    "\n"
	    "\n"
	    "Additional features:\n"
	    "o Print help or version.\n"
	    "\n"
	    "hfsprescue [-h|--help] [--version]\n"
	    "\n"
	    "o List files that have been found.\n"
	    "\n"
	    "hfsprescue --list [--slash] [-d <working directory>]\n"
	    "hfsprescue --list [-d <working directory>] | grep myfile.txt\n"
	    "\n"
	    "o CSV export of the list of files that have been found.\n"
	    "\n"
	    "hfsprescue --csv <file name> [--slash] [-d <working directory>]\n"
	    "\n"
	    "o Recover one file instead of the whole list of files.\n"
	    "\n"
	    "hfsprescue --one-file <device node|image file> <file number>\n"
	    "                  [-b <block size>]\n"
	    "                  [-o <offset in bytes>]\n"
	    "                  [-d <working directory>]\n"
	    "                  [--vh-file <file name>]\n"
	    "                  [--eof-file <file name>]\n"
	    "                  [--ignore-eof]\n"
	    "                  [--alternative]\n"
	    "\n"
	    "o Find possible positions of the Extents Overflow File.\n"
	    "\n"
	    "hfsprescue --find-eof\n"
	    "                  [-b <block size>]\n"
	    "                  [-o <offset in bytes>]\n"
	    "                  [--vh-file <file name>]\n"
	    "\n"
	    "o Extract the Extents Overflow File.\n"
	    "\n"
	    "hfsprescue --extract-eof <device node|image file>\n"
	    "                  [ [--start-block <number>] < [--last-block <number>] | [--num-blocks <number>] > ]\n"
	    "                  [--eof-file <output file>]\n"
	    "                  [--vh-file <file name>]\n"
	    "\n"
	    "o Find HFS+ Volume Header and partition start.\n"
	    "\n"
	    "hfsprescue --find-vh\n"
	    "                  [-o <offset in bytes>]\n"
	    "                  [--first] [-f|--force]\n"
	    "                  [-v|--verbose]\n"
	    "\n"
	    "o Find HFS+ Alternate Volume Header.\n"
	    "\n"
	    "hfsprescue --find-avh [--first] [-f|--force] [-v|--verbose]\n"
	    "\n"
	    "o Extract a HFS+ Volume Header.\n"
	    "\n"
	    "hfsprescue --extract-vh <device node|image file> <LBA sector>\n"
	    "                  [--vh-file <output file>]\n"
	    "\n"
	    "o Find bytes from a file and/or an Unicode string.\n"
	    "\n"
	    "hfsprescue --find <device node|image file>\n"
	    "                  [-ff <num bytes> <file1> [file2] [...]\n"
	    "                  [-fs <string>]\n"
	    "                  [-o <offset in bytes>]\n"
	    "\n"
	    "o Remove empty directories.\n"
	    "\n"
	    "hfsprescue --remove-empty-dirs [--dir <directory>] [-f|--force]\n"
	    "\n"
	    "More details are available in the man page and online at https://www.plop.at\n"
	    "\n"
	};

    if (isatty (STDOUT_FILENO)) 
    {
	// Output is not redirected
	int y = 0;
    
	y = SpecialPrint (header, 0);
	SpecialPrint (help_txt, y);
    }
    else
    {
	// Output is redirected,  do not interrupt with a keypress
	printf ("%s%s", header, help_txt);
    }
}

int SpecialPrint (const char *txt, int y)
{
    int width  = GetScreenWidth();
    int height = GetScreenHeight();
    int length = strlen (txt);

    char line[width + 1];
    int  txt_ofs  = 0;
    int  line_ofs = 0;

    while (txt_ofs < length)
    {
	bool print = false;

	if ((txt[txt_ofs] == '\n') || (line_ofs > width - 1)) // Check for new line or end of screen.
	{
	    print = true;
	}
	else 
	{
	    line[line_ofs] = txt[txt_ofs]; // Create output string.
	}
		
	if (print) // Print string and check for bottom screen.
	{
	    line[line_ofs] = 0;
	    
	    printf ("%s", line);
	    
	    if (txt[txt_ofs] == '\n')
	    {
		printf ("\n");
		txt_ofs++;
	    }
	    
	    y++;
	    
	    if (y > height - 2)
	    {
		printf ("*** Press enter to continue ***");
		fflush (stdout);
		getchar();
		y = 0;
	    }
	    line_ofs = 0;		
	}
	else
	{
	    txt_ofs++;
	    line_ofs++;
	}
    }
    return y;
}
