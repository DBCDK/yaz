/*
 * Copyright (c) 1995, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: options.c,v $
 * Revision 1.2  1995-05-16 08:51:13  quinn
 * License, documentation, and memory fixes
 *
 * Revision 1.1  1995/03/27  08:35:18  quinn
 * Created util library
 * Added memory debugging module. Imported options-manager
 *
 * Revision 1.2  1994/10/04  17:47:10  adam
 * Function options now returns arg with error option.
 *
 * Revision 1.1  1994/08/16  15:57:22  adam
 * The first utility modules.
 *
 */
#include <stdlib.h>

#include <options.h>

static int arg_no = 1;
static int arg_off = 0;

int options (const char *desc, char **argv, int argc, char **arg)
{
    int ch, i = 0;
    
    if (arg_no >= argc)
        return -2;
    if (arg_off == 0)
    {
        if (argv[arg_no][0] != '-')
        {
            *arg = argv[arg_no++];
            return 0;
        }
        arg_off++;
    }
    ch = argv[arg_no][arg_off++];
    while (desc[i])
    {
        int desc_char = desc[i++];
        int type = 0;
        if (desc[i] == ':')
	{	/* string argument */
            type = desc[i++];
	}
        if (desc_char == ch)
	{ /* option with argument */
            if (type)
	    {
                if (argv[arg_no][arg_off])
		{
                    *arg = argv[arg_no]+arg_off;
                    arg_no++;
                    arg_off =  0;
		}
                else
		{
                    arg_no++;
                    arg_off = 0;
                    if (arg_no < argc)
                        *arg = argv[arg_no++];
                    else
                        *arg = "";
		}
	    }
            else /* option with no argument */
	    {
                if (argv[arg_no][arg_off])
                    arg_off++;
                else
		{
                    arg_off = 0;
                    arg_no++;
		}
	    }
            return ch;
	}		
    }
    *arg = argv[arg_no]+arg_off-1;
    arg_no = arg_no + 1;
    arg_off = 0;
    return -1;
}
