// Copyright Epic Games, Inc. All Rights Reserved.

/** MCPP command line tool for debugging preprocessed HLSL shaders. */

#if PREPROCESSED
#include "mcpp.H"
#else
#include "system.H"
#include "internal.H"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


typedef struct BufferLinkedList
{
    char* Buffer;
    struct BufferLinkedList* Next;
}
BufferLinkedList;

static BufferLinkedList* GBufferLinkedList = NULL;

// allocates a new buffer within the global linked list
static char* AllocBuffer(size_t Size)
{
    // allocate output buffer
    char* OutBuffer = (char*)malloc(Size);

    // allocate new linked list node
    BufferLinkedList* Node = (BufferLinkedList*)malloc(sizeof(BufferLinkedList));
    Node->Buffer = OutBuffer;
    Node->Next = NULL;

    // link previous node to the new node
    if (GBufferLinkedList != NULL)
    {
        GBufferLinkedList->Next = Node;
    }
    GBufferLinkedList = Node;

    return OutBuffer;
}

static void FreeBuffer(BufferLinkedList* Node)
{
    if (Node != NULL)
    {
        if (Node->Buffer != NULL)
        {
            free(Node->Buffer);
        }
        FreeBuffer(Node->Next);
        free(Node);
    }
}

// frees the global buffer linked list
static void CleanUp()
{
    FreeBuffer(GBufferLinkedList);
}

// prints the help documentation to the standard output
static void PrintHelp()
{
    puts("USAGE:");
    puts("  mcpp_cmdline OPTIONS* INPUT");
    puts("OPTIONS:");
    puts("  -o, --output FILE    Specifies an output file, otherwise result is printed to standard output");
}

// prints an error to the standard error output and returns an error code for the main entry point
static int PrintError(const char* Format, ...)
{
    fprintf(stderr, "mcpp_cmdline: error: ");
    va_list Args;
    va_start(Args, Format);
    fprintf(stderr, Format);
    va_end(Args);
    return 1;
}

// prints an error about a missing argument for the specified command line option
static int ErrMissingArgForOption(const char* CmdOption)
{
    return PrintError("missing argument for option '%s'\n", CmdOption);
}

// prints an error about too many arguments after the last considered argument
static int ErrIllegalTrailingArgs(const char* Arg0)
{
    return PrintError("illegal trailing arguments after input file, beginning with '%s'", Arg0);
}

// prints an error about the failure to open the specified file
static int ErrOpenFile(const char* Filename)
{
    return PrintError("failed to open file: %s", Filename);
}

// callback for MCPP library with interface compliant to <get_file_contents_func>
int ReadFileContentCallback(void* UserData, const char* Filename, const char** OutContent, size_t* OutContentSize)
{
    // if output buffer is not specified, only check if file exists
    FILE* File = NULL;
    if (fopen_s(&File, Filename, "rb") == 0)
    {
        if (OutContent != NULL)
        {
            // read whole file content to linear buffer (including NULL terminator)
            fseek(File, 0, SEEK_END);
            long FileSize = ftell(File);
            fseek(File, 0, SEEK_SET);
            char* Buffer = AllocBuffer(FileSize + 1);
            fread(Buffer, 1, FileSize, File);

            // append NULL terminator
            Buffer[FileSize] = '\0';
            
            // write output parameters
            *OutContent = Buffer;

            if (OutContentSize != NULL)
            {
                *OutContentSize = FileSize + 1;
            }
        }
        fclose(File);
        return 1;
    }
    return 0;
}

/*
pre-process input filename with MCPP library

@param InputFilename Input source filename (any source with C/C++ pre-processor directives)
@param OutputFilename Optional output filename. If NULL, result will be printed to the standard output
*/
static int PreProcessSourceFile(const char* InputFilename, const char* OutputFilename, const char** Options, int NumOptions)
{
    // call main function of MCPP library
    char* OutputContent = NULL;
    char* OutputErrors = NULL;
    file_loader FileLoader;
    FileLoader.get_file_contents = ReadFileContentCallback;
    FileLoader.user_data = NULL;

    int Result = mcpp_run(Options, NumOptions, InputFilename, &OutputContent, &OutputErrors, FileLoader);

    if (Result == 0)
    {
        if (OutputFilename != NULL)
        {
            // write result to output file
            FILE* OutputFile = NULL;
            if (fopen_s(&OutputFile, OutputFilename, "w") == 0)
            {
                fwrite(OutputContent, 1, strlen(OutputContent), OutputFile);
                fclose(OutputFile);
            }
            else
            {
                return ErrOpenFile(OutputFilename);
            }
        }
        else
        {
            // write result to standard output
            puts(OutputContent);
        }
        return 0;
    }
    else
    {
        fprintf(stderr, "%s", OutputErrors);
        return Result;
    }
}

static int RunCmdLine(int ArgCount, char* ArgValues[])
{
    if (ArgCount <= 1)
    {
        PrintHelp();
        return 0;
    }
    else
    {
        const char* OutputFilename = NULL;
        for (int i = 1; i < ArgCount; i++)
        {
            // check for command options like "-o" or "--output"
            const char* Val = ArgValues[i];
            if (strcmp(Val, "-o") == 0 ||
                strcmp(Val, "--output") == 0)
            {
                if (i + 1 < ArgCount)
                {
                    OutputFilename = ArgValues[++i];
                }
                else
                {
                    return ErrMissingArgForOption(Val);
                }
            }
            // otherwise, parse input filename for preprocessing
            else
            {
                // pre-process input source file
                const char* InputFilename = Val;
                int Result = PreProcessSourceFile(InputFilename, OutputFilename, NULL, 0);
                if (Result != 0)
                {
                    return Result;
                }
                if (i + 1 < ArgCount)
                {
                    return ErrIllegalTrailingArgs(ArgValues[i + 1]);
                }
            }
        }
    }
    return 0;
}


int main(int ArgCount, char* ArgValues[])
{
    int Result = RunCmdLine(ArgCount, ArgValues);
    CleanUp();
    return Result;
}


