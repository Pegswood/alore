/* main.c - The main function of the Alore interpreter "alore"

   Copyright (c) 2010-2011 Jukka Lehtosalo

   Alore is licensed under the terms of the MIT license.  See the file
   LICENSE.txt in the distribution.
*/

#include "alore.h"
#include "compile.h"
#include "debug_runtime.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>


/* Usage string shown at the start of cmd line argument help message. */
#define USAGE "Usage: alore [option] ... programfile [arg] ..."


typedef struct {
    ABool displayCode;
    ABool checksDefined;
    ABool checkFirstDefined;
    Asize_t maxHeap;
} AOptions;


static void ParseOptions(int *argcp, char ***argvp, AOptions *options);
static void InitializeOptions(AOptions *options);
static void InvalidOption(const char *opt);
static void InvalidOptionValue(const char *opt, const char *value);
static void ShowHelp(void);
static void ShowVersion(void);
static ABool ParseSize(const char *s, Asize_t *size);


/* The main function of the Alore interpreter. */
int main(int argc, char **argv)
{
    int num;
    int returnValue;
    AThread *t;
    char *file;
    char *interp;
    AOptions options;

    interp = argv[0];

    /* Skip interpreter program path (argument 0). */
    argv++;
    argc--;

    ParseOptions(&argc, &argv, &options);
        
    if (argc >= 1) {
        /* Get the name of the program to run. */
        file = argv[0];
        argv++;
        argc--;
    } else {
        /* Program name missing. Display error and exit. */
        InvalidOption(NULL);
        /* Get rid of warning (never executed). */
        file = NULL;
    }

#ifdef A_DEBUG
    /* If any periodic check related parameters are defined, also update
       DebugCheckFirst (otherwise the parameters would have no effect). */
    if (options.checksDefined && !options.checkFirstDefined) {
#ifdef A_DEBUG_COMPILER
        ADebugCheckFirst = 0;
#else
        ADebugCheckFirst = 1;
#endif
    }
#endif
    
    returnValue = 0;

    num = ALoadAloreProgram(&t, file, interp, "", FALSE, argc, argv, NULL,
                            options.maxHeap);
    if (num >= 0) {
#ifdef A_DEBUG        
        if (options.displayCode)
            ADisplayCode();
        else
#endif
        {
            AValue val;
            
#ifdef A_DEBUG_COMPILER
            ADebugInstructionCounter = 1000000;
#endif
            if (ATry(t)) {
                /* An uncaught direct exception was raised somewhere in the
                   program. We must not call AEndTry. */
                val = AError;
                goto Error;
            } else
                val = ACallValue(t, AGlobalByNum(num), 0, NULL);
            AEndTry(t);
            
          Error:
            
            ADebugVerifyMemory();

            returnValue = AEndAloreProgram(t, val);
        }
    }

    return returnValue;
}


/* Parse command line options. Update *argcp and *argvp to point to the first
   positional argument. Fully initialize and populate the options struct.

   Note that some options are only available in builds with extra debugging
   features.

   This function does not return if one of options is invalid or is completely
   processed by this function (e.g. -v). */
static void ParseOptions(int *argcp, char ***argvp, AOptions *options)
{
    int argc = *argcp;
    char **argv = *argvp;

    InitializeOptions(options);

    /* Parse options; advance argc and argv. */
    while (argc > 0 && argv[0][0] == '-') {
        switch (argv[0][1]) {
#ifdef A_DEBUG
        case 'd':
            /* Display code and exit. */
            options->displayCode = TRUE;
            break;
#endif
        case 'v':
            ShowVersion();
            break;

        case '-':
            /* Long argument with -- prefix. */
            if (strcmp(argv[0], "--version") == 0) {
                ShowVersion();
                break;
            } else if (strcmp(argv[0], "--max-heap") == 0 && argc > 1) {
                /* Set the maximum size of the heap. */
                ABool status = ParseSize(argv[1], &options->maxHeap);
                if (!status)
                    InvalidOptionValue(argv[0], argv[1]);
                argv++;
                argc--;
                break;
            } else
                InvalidOption(argv[0]);
            
#ifdef A_DEBUG
        case 'm':
            /* Memory dump (IDEA: explain) */
            ADebug_MemoryDump = 1;
            break;
            
        case 't':
            /* Memory trace (IDEA: explain) */
            ADebug_MemoryTrace = 1;
            break;
            
        case 'a': {
            /* Set up debug watch location (IDEA: explain) */
            unsigned long ptr;
            argv++;
            argc--;

            if (argc > 0 && sscanf(argv[0], "%lx", &ptr) == 1) {
                ADebug_WatchLocation = (void *)ptr;
                break;
            }
        }

        case 'D':
            /* Set periodic instruction checking options. */
            options->checksDefined = TRUE;
            switch (argv[0][2]) {
            case 'n':
                /* Debug every Nth instruction. */
                argv++;
                argc--;
                ADebugCheckEveryNth = atoi(argv[0]);
                if (ADebugCheckEveryNth < 1)
                    ADebugCheckEveryNth = 1;
                break;
            case 'f':
                /* Set first instruction to check. */
                argv++;
                argc--;
                ADebugCheckFirst = atoi(argv[0]);
                options->checkFirstDefined = TRUE;
                break;
            case 'l':
                /* Set last instruction to check. */
                argv++;
                argc--;
                ADebugCheckLast = atoi(argv[0]);
                break;
            default:
                InvalidOption(argv[0]);
            }
            break;
#endif
        default:
            InvalidOption(argv[0]);
        }
        
        argv++;
        argc--;
    }

    /* Update caller's argument information. */
    *argcp = argc;
    *argvp = argv;
}


/* Initialize options struct to default values. */
static void InitializeOptions(AOptions *options)
{
    options->checksDefined = FALSE;
    options->checkFirstDefined = FALSE;
    options->displayCode = FALSE;
    options->maxHeap = 0;
}


/* Dislay error message about invalid option. Also show general help
   message. */
static void InvalidOption(const char *opt)
{
    if (opt != NULL)
        fprintf(stderr, "alore: Invalid option %s\n\n", opt);
    ShowHelp();
}


/* Display error message about invalid value for an option. */
static void InvalidOptionValue(const char *opt, const char *value)
{
    if (opt != NULL)
        fprintf(stderr, "alore: Invalid value %s for option %s\n\n", value,
                opt);
    ShowHelp();
}


/* Show command line option help and exit with code 1. */
static void ShowHelp(void)
{
    fprintf(stderr, "%s\n", USAGE);
    fprintf(stderr, "Options and arguments:\n"
           "  -v, --version  Show version information and exit\n"
#ifdef A_DEBUG
           "  -d             DEBUG: Dump bytecode and exit\n"
           "  -m             DEBUG: Memory dump\n"
           "  -t             DEBUG: Memory trace\n"
           "  -Dn N          DEBUG: Check every Nth instruction\n"
           "  -Df N          DEBUG: Specify first checked instruction\n"
           "  -Dl N          DEBUG: Specify last checked instruction\n"
#endif
           "  args           Arguments to Main function\n");
    exit(1);
}


/* Show version number and exit with code 0. */
static void ShowVersion(void)
{
    printf("Alore %s\n"
           "Copyright (c) 2010-2011 Jukka Lehtosalo\n", AVersion);
    exit(0);
}


/* Parse sizes such as 4M or 123k and store them in *size.

   NOTE: Does not check overflow on 64-bit systems. */
static ABool ParseSize(const char *s, Asize_t *size)
{
    char *end;
    long sval = strtol(s, &end, 10);
    Asize_t val;

    if (end == s || sval < 0) {
        return FALSE;
    }

    val = sval;
    if (*end == 'k') {
        if (SIZEOF_LONG == 4 && val >= 4096 * 1024)
            return FALSE;
        val *= 1024;
        end++;
    } else if (*end == 'M') {
        if (SIZEOF_LONG == 4 && val >= 4096)
            return FALSE;
        val *= 1024 * 1024;
        end++;
    } else if (*end == 'G') {
        /* Handle 4G as a special case, as otherwise it would overflow. */
        if (SIZEOF_LONG == 4 && val == 4)
            val = 0xffffffffUL;
        else if (SIZEOF_LONG == 4 && val > 4)
            return FALSE;
        else
            val *= 1024 * 1024 * 1024;
        end++;
    }

    if (*end != '\0')
        return FALSE;

    *size = val;
    return TRUE;
}
