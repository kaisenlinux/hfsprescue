/*
 *
 * Released under GPL v2
 * by Elmar Hanlhofer  http://www.plop.at
 *
 * Remove empty directories
 *
 */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "rmdir.h"


const char *cmd_sh[] = {
	"/bin/sh",
	};

const char *cmd_find[] = {
	"/bin/find",
	"/sbin/find",
	"/usr/bin/find",
	"/usr/sbin/find"
	};

const char *cmd_rmdir[] = {
	"/bin/rmdir",
	"/sbin/rmdir",
	"/usr/bin/rmdir",
	"/usr/sbin/rmdir"
	};


const char *FindCommand (const char *cmd[], int num)
{
    for (int i = 0; i < num; i++)
    {
	struct stat st;
        if (stat (cmd[i], &st) == 0)
        {
    	    return cmd[i];
        }

    }
    return NULL;
}


bool RemoveEmptyDirectories (const char *dir, bool force)
{   
    const char default_directory[] = "restored";
    const char tmp_script[]        = "/tmp-rm-empty-dirs-3837362651.sh";

    char gen_directory[FILENAME_MAX + sizeof (default_directory) + 1];
    char script       [FILENAME_MAX + sizeof (default_directory) + 1 + sizeof (tmp_script)];
    const char *directory;

    const char *find;
    const char *rmdir;
    
    FILE *pf;
    
    if (dir == NULL)
    {
	if (!getcwd (gen_directory, FILENAME_MAX))
	{
	    printf ("Unable to get woking directory.\n");
	    return false;
	}
	
	strcat (gen_directory, "/");
	strcat (gen_directory, default_directory);
	directory = gen_directory;
    }
    else
    {
	directory = dir;
    }

    printf ("Base directory: %s\n\n", directory);

    if (force)
    {
	printf ("*** Force removing empty directories.\n");
    }
    else
    {
	printf ("Start removing empty directories? [N/y]: ");
	char c = getchar ();
	
	if ((c != 0x79) && (c != 0x59))
	{
	    printf ("Abort.\n");
	    return false;
	}
    }

    if (FindCommand (cmd_sh, 1) == NULL)
    {
	printf ("Error: Command '/bin/sh' not found.\n");
	return false;
    }

    find  = FindCommand (cmd_find, 4);
    if (find == NULL)
    {
	printf ("Error: Command 'find' not found.\n");
	return false;
    }

    rmdir = FindCommand (cmd_rmdir, 4);
    if (rmdir == NULL)
    {
	printf ("Error: Command 'rmdir' not found.\n");
	return false;
    }
    
    sprintf (script, "%s%s", directory, tmp_script);

    pf = fopen (script, "w");
    if (!pf)
    {
        printf ("Unable to create %s: %s\n", script, strerror (errno));
        return false;
    }
    printf ("Creating script...\n");
    
    fprintf (pf, "#!/bin/sh\n\n"
                 "# Remove empty directories, by Elmar Hanlhofer\n\n"
		 "SAVEIFS=$IFS\n"
		 "IFS=$(echo -en \"\\n\\b\")\n\n"
		 "found=$(find \"%s\" -type d | sort -nr)\n\n"
		 "echo \"Removing empty directories...\"\n\n"
		 "for dir in $found\n"
		 "do\n"
		 "	rmdir \"$dir\" 2> /dev/null\n"
		 "done\n\n"
		 "IFS=$SAVEIFS\n"
		 "exit 0\n", directory);

    fclose (pf);
    chmod (script, S_IRWXU);

    if (system (script) != 0)
    {
	printf ("Error: Unable to start the script.\n");
	return false;
    }
    
    printf ("Removing script.\n");
    unlink (script);
    
    printf ("\nDone.\n");
    return true;
}
