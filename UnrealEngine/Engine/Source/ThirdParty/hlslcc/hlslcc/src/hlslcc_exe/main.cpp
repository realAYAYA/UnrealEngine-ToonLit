// Copyright Epic Games, Inc. All Rights Reserved.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ShaderCompilerCommon.h"
#include "hlslcc.h"
#include "LanguageSpec.h"
#include "ir.h"
#include "irDump.h"
#include "hlslcc_private.h"
#include "TestLanguage.h"

char* LoadShaderFromFile(const char* Filename);

struct SCmdOptions
{
	const char* ShaderFilename;
	EHlslShaderFrequency Frequency;
	EHlslCompileTarget Target;
	const char* Entry;
	bool bDumpAST;
	bool bNoPreprocess;
	bool bFlattenUB;
	bool bFlattenUBStructures;
	bool bUseDX11Clip;
	bool bGroupFlattenedUB;
	bool bExpandExpressions;
	bool bCSE;
	bool bSeparateShaderObjects;
	bool bPackIntoUBs;
	bool bUseFullPrecision;
	bool bUsesExternalTexture;
	bool bExpandUBMemberArrays;
	uint32 CFlags = 0;
	const char* OutFile;

	SCmdOptions() 
	{
		ShaderFilename = nullptr;
		Frequency = HSF_InvalidFrequency;
		Target = HCT_InvalidTarget;
		Entry = nullptr;
		bDumpAST = false;
		bNoPreprocess = false;
		bFlattenUB = false;
		bFlattenUBStructures = false;
		bUseDX11Clip = false;
		bGroupFlattenedUB = false;
		bExpandExpressions = false;
		bCSE = false;
		bSeparateShaderObjects = false;
		bPackIntoUBs = false;
		bUseFullPrecision = false;
		bUsesExternalTexture = false;
		bExpandUBMemberArrays = false;
		OutFile = nullptr;
	}
};

static int ParseCommandLine( int argc, char** argv, SCmdOptions& OutOptions)
{
	while (argc)
	{
		if (**argv == '-')
		{
			if (!strcmp(*argv, "-vs"))
			{
				OutOptions.Frequency = HSF_VertexShader;
			}
			else if (!strcmp(*argv, "-ps"))
			{
				OutOptions.Frequency = HSF_PixelShader;
			}
			else if (!strcmp(*argv, "-gs"))
			{
				OutOptions.Frequency = HSF_GeometryShader;
			}
			else if (!strcmp(*argv, "-ds"))
			{
				OutOptions.Frequency = HSF_DomainShader;
			}
			else if (!strcmp(*argv, "-hs"))
			{
				OutOptions.Frequency = HSF_HullShader;
			}
			else if (!strcmp(*argv, "-cs"))
			{
				OutOptions.Frequency = HSF_ComputeShader;
			}
			else if (!strcmp(*argv, "-sm4"))
			{
				OutOptions.Target = HCT_FeatureLevelSM4;
			}
			else if (!strcmp(*argv, "-sm5"))
			{
				OutOptions.Target = HCT_FeatureLevelSM5;
			}
			else if (!strcmp(*argv, "-es31"))
			{
				OutOptions.Target = HCT_FeatureLevelES3_1;
			}
			else if (!strcmp(*argv, "-es31ext"))
			{
				OutOptions.Target = HCT_FeatureLevelES3_1Ext;
			}
			else if (!strcmp(*argv, "-es2"))
			{
				OutOptions.Target = HCT_FeatureLevelES2;
			}
			else if (!strncmp(*argv, "-hlslccflags=", 13))
			{
				OutOptions.CFlags = atoi((*argv) + 13);
			}
			else if (!strncmp(*argv, "-entry=", 7))
			{
				OutOptions.Entry = (*argv) + 7;
			}
			else if (!strcmp(*argv, "-ast"))
			{
				OutOptions.bDumpAST = true;
			}
			else if (!strcmp(*argv, "-nopp"))
			{
				OutOptions.bNoPreprocess = true;
			}
			else if (!strcmp(*argv, "-flattenub"))
			{
				OutOptions.bFlattenUB = true;
			}
			else if (!strcmp(*argv, "-flattenubstruct"))
			{
				OutOptions.bFlattenUBStructures = true;
			}
			else if (!strcmp(*argv, "-dx11clip"))
			{
				OutOptions.bUseDX11Clip = true;
			}
			else if (!strcmp(*argv, "-groupflatub"))
			{
				OutOptions.bGroupFlattenedUB = true;
			}
			else if (!strcmp(*argv, "-cse"))
			{
				OutOptions.bCSE = true;
			}
			else if (!strcmp(*argv, "-xpxpr"))
			{
				OutOptions.bExpandExpressions = true;
			}
			else if (!strncmp(*argv, "-o=", 3))
			{
				OutOptions.OutFile = (*argv) + 3;
			}
			else if (!strcmp( *argv, "-separateshaders"))
			{
				OutOptions.bSeparateShaderObjects = true;
			}
			else if (!strcmp(*argv, "-packintoubs"))
			{
				OutOptions.bPackIntoUBs = true;
			}
			else if (!strcmp(*argv, "-usefullprecision"))
			{
				OutOptions.bUseFullPrecision = true;
			}
			else if (!strcmp(*argv, "-usesexternaltexture"))
			{
				OutOptions.bUsesExternalTexture = true;
			}
			else if (!strcmp(*argv, "-expandubarrays"))
			{
				OutOptions.bExpandUBMemberArrays = true;
			}
			else
			{
				dprintf("Warning: Unknown option %s\n", *argv);
			}
		}
		else
		{
			OutOptions.ShaderFilename = *argv;
		}

		argc--;
		argv++;
	}

	if (!OutOptions.ShaderFilename)
	{
		dprintf( "Provide a shader filename\n");
		return -1;
	}
	if (!OutOptions.Entry)
	{
		//default to Main
		dprintf( "No shader entrypoint specified, defaulting to 'Main'\n");
		OutOptions.Entry = "Main";
	}
	if (OutOptions.Frequency == HSF_InvalidFrequency)
	{
		//default to PixelShaders
		dprintf( "No shader frequency specified, defaulting to PS\n");
		OutOptions.Frequency = HSF_PixelShader;
	}
	if (OutOptions.Target == HCT_InvalidTarget)
	{
		dprintf("No shader model specified, defaulting to SM5\n");

		//Default to GL3 shaders
		OutOptions.Target = HCT_FeatureLevelSM5;
	}

	return 0;
}

/* 
to debug issues which only show up when multiple shaders get compiled by the same process,
such as the ShaderCompilerWorker
*/ 
#define NUMBER_OF_MAIN_RUNS 1
#if NUMBER_OF_MAIN_RUNS > 1
int actual_main( int argc, char** argv);

int main( int argc, char** argv)
{
	int result = 0;

	for(int c = 0; c < NUMBER_OF_MAIN_RUNS; ++c)
	{
		char** the_real_argv = new char*[argc];

		for(int i = 0; i < argc; ++i)
		{
			the_real_argv[i] = strdup(argv[i]);
		}
		
		int the_result = actual_main(argc, the_real_argv);
		
		for(int i = 0; i < argc; ++i)
		{
			delete the_real_argv[i];
		}

		delete[] the_real_argv;


		result += the_result;
	}
	return result;
}

int actual_main( int argc, char** argv)
#else
int main( int argc, char** argv)
#endif
{
#if ENABLE_CRT_MEM_LEAKS	
	int Flag = _CRTDBG_ALLOC_MEM_DF | /*_CRTDBG_CHECK_ALWAYS_DF |*/ /*_CRTDBG_CHECK_CRT_DF |*/ _CRTDBG_DELAY_FREE_MEM_DF;
	int OldFlag = _CrtSetDbgFlag(Flag);
	//FCRTMemLeakScope::BreakOnBlock(15828);
#endif
	char* HLSLShaderSource = nullptr;
	char* GLSLShaderSource = nullptr;
	char* ErrorLog = nullptr;

	SCmdOptions Options;
	{
		int Result = ParseCommandLine( argc-1, argv+1, Options);
		if (Result != 0)
		{
			return Result;
		}
	}
	
	HLSLShaderSource = LoadShaderFromFile(Options.ShaderFilename);
	if (!HLSLShaderSource)
	{
		dprintf( "Failed to open input shader %s\n", Options.ShaderFilename);
		return -2;
	}

	int Flags = HLSLCC_PackUniforms; // | HLSLCC_NoValidation | HLSLCC_PackUniforms;
	Flags |= Options.bNoPreprocess ? HLSLCC_NoPreprocess : 0;
	Flags |= Options.bDumpAST ? HLSLCC_PrintAST : 0;
	Flags |= Options.bUseDX11Clip ? HLSLCC_DX11ClipSpace : 0;
	Flags |= Options.bFlattenUB ? HLSLCC_FlattenUniformBuffers : 0;
	Flags |= Options.bFlattenUBStructures ? HLSLCC_FlattenUniformBufferStructures : 0;
	Flags |= Options.bGroupFlattenedUB ? HLSLCC_GroupFlattenedUniformBuffers : 0;
	Flags |= Options.bCSE ? HLSLCC_ApplyCommonSubexpressionElimination : 0;
	Flags |= Options.bExpandExpressions ? HLSLCC_ExpandSubexpressions : 0;
	Flags |= Options.bSeparateShaderObjects ? HLSLCC_SeparateShaderObjects : 0;
	Flags |= Options.bPackIntoUBs ? HLSLCC_PackUniformsIntoUniformBuffers : 0;
	Flags |= Options.bUseFullPrecision ? HLSLCC_UseFullPrecisionInPS : 0;
	Flags |= Options.bUsesExternalTexture ? HLSLCC_UsesExternalTexture : 0;
	Flags |= Options.bExpandUBMemberArrays ? HLSLCC_ExpandUBMemberArrays : 0;

	Flags |= Options.CFlags;

	FTestCodeBackend TestCodeBackend(Flags, Options.Target);
	FTestLanguageSpec TestLanguageSpec;//(Options.Target == HCT_FeatureLevelES2);

	FCodeBackend* CodeBackend = &TestCodeBackend;
	ILanguageSpec* LanguageSpec = &TestLanguageSpec;

	int Result = 0;
	{
		FCRTMemLeakScope MemLeakScopeContext(true);
		//for (int32 Index = 0; Index < 256; ++Index)
		{
			FHlslCrossCompilerContext Context(Flags, Options.Frequency, Options.Target);
			if (Context.Init(Options.ShaderFilename, LanguageSpec))
			{
				//FCRTMemLeakScope MemLeakScopeRun;
				Result = Context.Run(
					HLSLShaderSource,
					Options.Entry,
					CodeBackend,
					&GLSLShaderSource,
					&ErrorLog) ? 1 : 0;
			}

			if (GLSLShaderSource)
			{
				dprintf("GLSL Shader Source --------------------------------------------------------------\n");
				dprintf("%s",GLSLShaderSource);
				dprintf("\n-------------------------------------------------------------------------------\n\n");
			}

			if (ErrorLog)
			{
				dprintf("Error Log ----------------------------------------------------------------------\n");
				dprintf("%s",ErrorLog);
				dprintf("\n-------------------------------------------------------------------------------\n\n");
			}

			if (Options.OutFile && GLSLShaderSource)
			{
				FILE *fp = fopen( Options.OutFile, "w");

				if (fp)
				{
					fprintf( fp, "%s", GLSLShaderSource);
					fclose(fp);
				}
			}

			free(HLSLShaderSource);
			free(GLSLShaderSource);
			free(ErrorLog);

			dprintf_free();
		}
	}

#if ENABLE_CRT_MEM_LEAKS	
	Flag = _CrtSetDbgFlag(OldFlag);
#endif

	return 0;
}

char* LoadShaderFromFile(const char* Filename)
{
	char* Source = 0;
	FILE* fp = fopen(Filename, "r");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size_t FileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		Source = (char*)malloc(FileSize + 1);
		size_t NumRead = fread(Source, 1, FileSize, fp);
		Source[NumRead] = 0;
		fclose(fp);
	}
	return Source;
}
