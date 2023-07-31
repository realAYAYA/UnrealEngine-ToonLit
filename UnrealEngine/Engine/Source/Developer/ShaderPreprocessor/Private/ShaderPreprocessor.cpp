// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderPreprocessor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "PreprocessorPrivate.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ShaderPreprocessor);

/**
 * Append defines to an MCPP command line.
 * @param OutOptions - Upon return contains MCPP command line parameters as an array of strings.
 * @param Definitions - Definitions to add.
 */
static void AddMcppDefines(TArray<TArray<ANSICHAR>>& OutOptions, const TMap<FString, FString>& Definitions)
{
	for (TMap<FString, FString>::TConstIterator It(Definitions); It; ++It)
	{
		FString Argument(FString::Printf(TEXT("-D%s=%s"), *(It.Key()), *(It.Value())));
		FTCHARToUTF8 Converter(Argument.GetCharArray().GetData());
		OutOptions.Emplace((const ANSICHAR*)Converter.Get(), Converter.Length() + 1);
	}
}

/**
 * Helper class used to load shader source files for MCPP.
 */
class FMcppFileLoader
{
public:
	/** Initialization constructor. */
	explicit FMcppFileLoader(const FShaderCompilerInput& InShaderInput, FShaderCompilerOutput& InShaderOutput)
		: ShaderInput(InShaderInput)
		, ShaderOutput(InShaderOutput)
	{
		FString InputShaderSource;
		if (LoadShaderSourceFile(*InShaderInput.VirtualSourceFilePath, InShaderInput.Target.GetPlatform(),  &InputShaderSource, nullptr, &InShaderInput.ShaderPlatformName))
		{
			InputShaderSource = FString::Printf(TEXT("%s\n#line 1\n%s"), *ShaderInput.SourceFilePrefix, *InputShaderSource);
			CachedFileContents.Add(InShaderInput.VirtualSourceFilePath, StringToArray<ANSICHAR>(*InputShaderSource, InputShaderSource.Len() + 1));
		}
	}

	/** Retrieves the MCPP file loader interface. */
	file_loader GetMcppInterface()
	{
		file_loader Loader;
		Loader.get_file_contents = GetFileContents;
		Loader.user_data = (void*)this;
		return Loader;
	}

	bool HasIncludedMandatoryHeaders() const
	{
		return CachedFileContents.Contains(TEXT("/Engine/Public/Platform.ush"));
	}

private:
	/** Holder for shader contents (string + size). */
	typedef TArray<ANSICHAR> FShaderContents;
	
	/** MCPP callback for retrieving file contents. */
	static int GetFileContents(void* InUserData, const ANSICHAR* InVirtualFilePath, const ANSICHAR** OutContents, size_t* OutContentSize)
	{
		FMcppFileLoader* This = (FMcppFileLoader*)InUserData;

		FUTF8ToTCHAR UTF8Converter(InVirtualFilePath);
		FString VirtualFilePath = UTF8Converter.Get();

		// Substitute virtual platform path here to make sure that #line directives refer to the platform-specific file.
		ReplaceVirtualFilePathForShaderPlatform(VirtualFilePath, This->ShaderInput.Target.GetPlatform());

		// Fixup autogen file
		ReplaceVirtualFilePathForShaderAutogen(VirtualFilePath, This->ShaderInput.Target.GetPlatform(), &This->ShaderInput.ShaderPlatformName);

		// Collapse any relative directories to allow #include "../MyFile.ush"
		FPaths::CollapseRelativeDirectories(VirtualFilePath);

		FShaderContents* CachedContents = This->CachedFileContents.Find(VirtualFilePath);
		if (!CachedContents)
		{
			FString FileContents;

			if (This->ShaderInput.Environment.IncludeVirtualPathToContentsMap.Contains(VirtualFilePath))
			{
				FileContents = This->ShaderInput.Environment.IncludeVirtualPathToContentsMap.FindRef(VirtualFilePath);
			}
			else if (This->ShaderInput.Environment.IncludeVirtualPathToExternalContentsMap.Contains(VirtualFilePath))
			{
				FileContents = *This->ShaderInput.Environment.IncludeVirtualPathToExternalContentsMap.FindRef(VirtualFilePath);
			}
			else
			{
				CheckShaderHashCacheInclude(VirtualFilePath, This->ShaderInput.Target.GetPlatform());

				LoadShaderSourceFile(*VirtualFilePath, This->ShaderInput.Target.GetPlatform(), &FileContents, &This->ShaderOutput.Errors, &This->ShaderInput.ShaderPlatformName);
			}

			if (FileContents.Len() > 0)
			{
				// Adds a #line 1 "<Absolute file path>" on top of every file content to have nice absolute virtual source
				// file path in error messages.
				FileContents = FString::Printf(TEXT("#line 1 \"%s\"\n%s"), *VirtualFilePath, *FileContents);

				CachedContents = &This->CachedFileContents.Add(VirtualFilePath, StringToArray<ANSICHAR>(*FileContents, FileContents.Len() + 1));
			}
		}

		if (OutContents)
		{
			*OutContents = CachedContents ? CachedContents->GetData() : NULL;
		}
		if (OutContentSize)
		{
			*OutContentSize = CachedContents ? CachedContents->Num() : 0;
		}

		return CachedContents != nullptr;
	}

	/** Shader input data. */
	const FShaderCompilerInput& ShaderInput;
	/** Shader output data. */
	FShaderCompilerOutput& ShaderOutput;
	/** File contents are cached as needed. */
	TMap<FString,FShaderContents> CachedFileContents;
};

//////////////////////////////////////////////////////////////////////////
//
// MCPP memory management callbacks
//
//    Without these, the shader compilation process ends up spending
//    most of its time in malloc/free on Windows.
//

#if PLATFORM_WINDOWS
#	define USE_UE_MALLOC_FOR_MCPP 1
#else
#	define USE_UE_MALLOC_FOR_MCPP 0
#endif

#if USE_UE_MALLOC_FOR_MCPP == 2

class FMcppAllocator
{
public:
	void* Alloc(size_t sz)
	{
		return ::malloc(sz);
	}

	void* Realloc(void* ptr, size_t sz)
	{
		return ::realloc(ptr, sz);
	}

	void Free(void* ptr)
	{
		::free(ptr);
	}
};

#elif USE_UE_MALLOC_FOR_MCPP == 1

class FMcppAllocator
{
public:
	void* Alloc(size_t sz)
	{
		return FMemory::Malloc(sz);
	}

	void* Realloc(void* ptr, size_t sz)
	{
		return FMemory::Realloc(ptr, sz);
	}

	void Free(void* ptr)
	{
		FMemory::Free(ptr);
	}
};

#endif

#if USE_UE_MALLOC_FOR_MCPP

FMcppAllocator GMcppAlloc;

#endif

static void DumpShaderDefinesAsCommentedCode(const FShaderCompilerInput& ShaderInput, FString* OutDefines)
{
	const TMap<FString, FString>& Definitions = ShaderInput.Environment.GetDefinitions();

	TArray<FString> Keys;
	Definitions.GetKeys(/* out */ Keys);
	Keys.Sort();

	FString Defines;
	for (const FString& Key : Keys)
	{
		Defines += FString::Printf(TEXT("// #define %s %s\n"), *Key, *Definitions[Key]);
	}

	*OutDefines = MakeInjectedShaderCodeBlock(TEXT("DumpShaderDefinesAsCommentedCode"), Defines);
}

//////////////////////////////////////////////////////////////////////////

/**
 * Preprocess a shader.
 * @param OutPreprocessedShader - Upon return contains the preprocessed source code.
 * @param ShaderOutput - ShaderOutput to which errors can be added.
 * @param ShaderInput - The shader compiler input.
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @param bShaderDumpDefinesAsCommentedCode - Whether to add shader definitions as comments.
 * @returns true if the shader is preprocessed without error.
 */
bool PreprocessShader(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const FShaderCompilerDefinitions& AdditionalDefines,
	EDumpShaderDefines DefinesPolicy
	)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PreprocessShader);

	// Skip the cache system and directly load the file path (used for debugging)
	if (ShaderInput.bSkipPreprocessedCache)
	{
		return FFileHelper::LoadFileToString(OutPreprocessedShader, *ShaderInput.VirtualSourceFilePath);
	}
	else
	{
		check(CheckVirtualShaderFilePath(ShaderInput.VirtualSourceFilePath));
	}

	int32 McppResult = 0;
	FString McppOutput, McppErrors;

	static FCriticalSection McppCriticalSection;

	bool bHasIncludedMandatoryHeaders = false;
	{
		FMcppFileLoader FileLoader(ShaderInput, ShaderOutput);

		TArray<TArray<ANSICHAR>> McppOptions;
		AddMcppDefines(McppOptions, ShaderInput.Environment.GetDefinitions());
		AddMcppDefines(McppOptions, AdditionalDefines.GetDefinitionMap());

		// MCPP is not threadsafe.

		FScopeLock McppLock(&McppCriticalSection);

#if USE_UE_MALLOC_FOR_MCPP
		auto spp_malloc		= [](size_t sz)				{ return GMcppAlloc.Alloc(sz); };
		auto spp_realloc	= [](void* ptr, size_t sz)	{ return GMcppAlloc.Realloc(ptr, sz); };
		auto spp_free		= [](void* ptr)				{ GMcppAlloc.Free(ptr); };

		mcpp_setmalloc(spp_malloc, spp_realloc, spp_free);
#endif

		// Convert MCPP options to array of ANSI-C strings
		TArray<const ANSICHAR*> McppOptionsANSI;
		for (const TArray<ANSICHAR>& Option : McppOptions)
		{
			McppOptionsANSI.Add(Option.GetData());
		}

		// Append additional options as C-string literal
		McppOptionsANSI.Add("-V199901L");

		ANSICHAR* McppOutAnsi = NULL;
		ANSICHAR* McppErrAnsi = NULL;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(mcpp_run);
			McppResult = mcpp_run(
				McppOptionsANSI.GetData(),
				McppOptionsANSI.Num(),
				TCHAR_TO_ANSI(*ShaderInput.VirtualSourceFilePath),
				&McppOutAnsi,
				&McppErrAnsi,
				FileLoader.GetMcppInterface()
			);
		}

		McppOutput = McppOutAnsi;
		McppErrors = McppErrAnsi;

		bHasIncludedMandatoryHeaders = FileLoader.HasIncludedMandatoryHeaders();
	}

	if (!ParseMcppErrors(ShaderOutput.Errors, ShaderOutput.PragmaDirectives, McppErrors))
	{
		return false;
	}

	// Report unhandled mcpp failure that didn't generate any errors
	if (McppResult != 0)
	{
		FShaderCompilerError* CompilerError = new(ShaderOutput.Errors) FShaderCompilerError;
		CompilerError->ErrorVirtualFilePath = ShaderInput.VirtualSourceFilePath;
		CompilerError->ErrorLineString = TEXT("0");
		CompilerError->StrippedErrorMessage = FString::Printf(TEXT("PreprocessShader mcpp_run failed with error code %d"), McppResult);
		return false;
	}

	if (!bHasIncludedMandatoryHeaders)
	{
		FShaderCompilerError Error;
		Error.ErrorVirtualFilePath = ShaderInput.VirtualSourceFilePath;
		Error.ErrorLineString = TEXT("1");
		Error.StrippedErrorMessage = TEXT("Error: Shader is required to include /Engine/Public/Platform.ush");

		ShaderOutput.Errors.Add(Error);
		return false;
	}

	// List the defines used for compilation in the preprocessed shaders, especially to know witch permutation vector this shader is.
	if (DefinesPolicy == EDumpShaderDefines::AlwaysIncludeDefines || (DefinesPolicy == EDumpShaderDefines::DontCare && ShaderInput.DumpDebugInfoPath.Len() > 0))
	{
		DumpShaderDefinesAsCommentedCode(ShaderInput, &OutPreprocessedShader);
	}

	OutPreprocessedShader += McppOutput;

	return true;
}
