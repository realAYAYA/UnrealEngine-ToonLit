// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderPreprocessor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "PreprocessorPrivate.h"

#include "stb_preprocess/preprocessor.h"
#include "stb_preprocess/stb_alloc.h"
#include "stb_preprocess/stb_ds.h"

namespace
{
	const FString PlatformHeader = TEXT("/Engine/Public/Platform.ush");
	const FString PlatformHeaderLowerCase = PlatformHeader.ToLower();
	void LogMandatoryHeaderError(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output)
	{
		FShaderCompilerError Error;
		Error.ErrorVirtualFilePath = Input.VirtualSourceFilePath;
		Error.ErrorLineString = TEXT("1");
		Error.StrippedErrorMessage = FString::Printf(TEXT("Error: Shader is required to include %s"), *PlatformHeader);

		Output.Errors.Add(Error);
	}
}

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
			InputShaderSource = FString::Printf(TEXT("#line 1\n%s"), *InputShaderSource);
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
		return CachedFileContents.Contains(PlatformHeader);
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
				CheckShaderHashCacheInclude(VirtualFilePath, This->ShaderInput.Target.GetPlatform(), This->ShaderInput.ShaderFormat.ToString());

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

bool InnerPreprocessShaderMcpp(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const FShaderCompilerDefinitions& AdditionalDefines)
{
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
		LogMandatoryHeaderError(ShaderInput, ShaderOutput);
		return false;
	}

	OutPreprocessedShader += McppOutput;

	return true;
}

extern "C"
{
	// adapter functions for STB memory allocation
	void* StbMalloc(size_t Size) 
	{
		void* Alloc = FMemory::Malloc(Size);
		return Alloc; 
	}

	void* StbRealloc(void* Pointer, size_t Size) 
	{
		void* Alloc = FMemory::Realloc(Pointer, Size);
		return Alloc;
	}

	void StbFree(void* Pointer) 
	{
		return FMemory::Free(Pointer); 
	}

	ANSICHAR* StbStrDup(const ANSICHAR* InString)
	{
		if (InString)
		{
			int32 Len = FCStringAnsi::Strlen(InString) + 1;
			ANSICHAR* Result = reinterpret_cast<ANSICHAR*>(StbMalloc(Len));
			return FCStringAnsi::Strncpy(Result, InString, Len);
		}
		return nullptr;
	}
}

struct FStbPreprocessContext
{
	const FShaderCompilerInput& ShaderInput;
	TMap<FString, TArray<ANSICHAR>> LoadedIncludesCache;
	TMap<FString, TUniquePtr<ANSICHAR[]>> SeenPathsLowerCase;

	bool HasIncludedMandatoryHeaders()
	{
		return SeenPathsLowerCase.Contains(PlatformHeaderLowerCase);
	}
};

inline bool IsEndOfLine(ANSICHAR C)
{
	return C == '\r' || C == '\n';
}

inline bool CommentStripNeedsHandling(ANSICHAR C)
{
	return IsEndOfLine(C) || C == '/' || C == 0;
}

inline int NewlineCharCount(ANSICHAR First, ANSICHAR Second)
{
	return ((First + Second) == '\r' + '\n') ? 2 : 1;
}

void ConvertAndStripComments(const FString& ShaderSource, TArray<ANSICHAR>& OutStripped)
{
	auto ShaderSourceAnsiConvert = StringCast<ANSICHAR>(*ShaderSource);

	// STB preprocessor does not strip comments, so we do so here before returning the loaded source
	// Doing so is barely more costly than the memcopy we require anyways so has negligible overhead.
	// Reserve worst case (i.e. assuming there are no comments at all) to avoid reallocation
	// Note: there's a potential future optimization here if we convert and strip at the same time;
	// currently this is incurring an extra heap allocation and copy in the case where the StringCast
	// is not a straight pointer copy (one alloc for the conversion and another for the stripped char array).
	OutStripped.SetNumUninitialized(ShaderSourceAnsiConvert.Length() + 1); // +1 to append null terminator

	ANSICHAR* CurrentOut = OutStripped.GetData();

	const ANSICHAR* const End = ShaderSourceAnsiConvert.Get() + ShaderSourceAnsiConvert.Length();
	for (const ANSICHAR* Current = ShaderSourceAnsiConvert.Get(); Current < End;)
	{
		while (!CommentStripNeedsHandling(*Current))
		{
			*CurrentOut++ = *Current++;
		}

		if (IsEndOfLine(*Current))
		{
			*CurrentOut++ = '\n';
			Current += NewlineCharCount(Current[0], Current[1]);
		}
		else if (Current[0] == '/')
		{
			if (Current[1] == '/')
			{
				while (!IsEndOfLine(*Current) && Current < End)
				{
					++Current;
				}
			}
			else if (Current[1] == '*')
			{
				Current += 2;
				while (!(Current[0] == '*' && Current[1] == '/'))
				{
					if (IsEndOfLine(*Current))
					{
						*CurrentOut++ = '\n';
						Current += NewlineCharCount(Current[0], Current[1]);
					}
					else
					{
						++Current;
					}
				}
				Current += 2;
			}
			else
			{
				*CurrentOut++ = *Current++;
			}
		}
	}
	// Null terminate after comment-stripped copy
	*CurrentOut++ = 0;

	// Set correct length after stripping but don't bother shrinking/reallocating, minor memory overhead to save time
	OutStripped.SetNum(CurrentOut - OutStripped.GetData(), /* bAllowShrinking */false);
}

static const ANSICHAR* StbLoadFile(const ANSICHAR* Filename, void* RawContext, size_t* OutLength)
{
	FStbPreprocessContext& Context = *reinterpret_cast<FStbPreprocessContext*>(RawContext);
	FString FilenameConverted = StringCast<TCHAR>(Filename).Get();
	TArray<ANSICHAR>* ContentsCached = Context.LoadedIncludesCache.Find(FilenameConverted);
	if (!ContentsCached)
	{
		FString ShaderSource;

		if (Context.ShaderInput.Environment.IncludeVirtualPathToContentsMap.Contains(FilenameConverted))
		{
			ShaderSource = Context.ShaderInput.Environment.IncludeVirtualPathToContentsMap.FindRef(FilenameConverted);
		}
		else if (Context.ShaderInput.Environment.IncludeVirtualPathToExternalContentsMap.Contains(FilenameConverted))
		{
			ShaderSource = *Context.ShaderInput.Environment.IncludeVirtualPathToExternalContentsMap.FindRef(FilenameConverted);
		}
		else
		{
			CheckShaderHashCacheInclude(FilenameConverted, Context.ShaderInput.Target.GetPlatform(), Context.ShaderInput.ShaderFormat.ToString());
			LoadShaderSourceFile(*FilenameConverted, Context.ShaderInput.Target.GetPlatform(), &ShaderSource, nullptr);
		}
		check(!ShaderSource.IsEmpty());
		ContentsCached = &Context.LoadedIncludesCache.Add(FilenameConverted);
		ConvertAndStripComments(ShaderSource, *ContentsCached);
	}
	check(ContentsCached);
	*OutLength = ContentsCached->Num();
	return ContentsCached->GetData();
}

static void StbFreeFile(const ANSICHAR* Filename, const ANSICHAR* Contents, void* RawContext)
{
	FStbPreprocessContext& Context = *reinterpret_cast<FStbPreprocessContext*>(RawContext);
	FString FilenameConverted = StringCast<TCHAR>(Filename).Get();
	Context.LoadedIncludesCache.FindAndRemoveChecked(FilenameConverted);
}

static const ANSICHAR* StbResolveInclude(const ANSICHAR* PathInSource, uint32 PathLen, const ANSICHAR* ParentPathAnsi, void* RawContext)
{
	FStbPreprocessContext& Context = *reinterpret_cast<FStbPreprocessContext*>(RawContext);
	FString PathModified(PathLen, PathInSource);
	FString ParentFolder(ParentPathAnsi);
	ParentFolder = FPaths::GetPath(ParentFolder);
	if (!PathModified.StartsWith(TEXT("/"))) // if path doesn't start with / it's relative, if so append the parent's folder and collapse any relative dirs
	{
		PathModified = ParentFolder / PathModified;
		FPaths::CollapseRelativeDirectories(PathModified);
	}

	// Substitute virtual platform path here to make sure that #line directives refer to the platform-specific file.
	ReplaceVirtualFilePathForShaderPlatform(PathModified, Context.ShaderInput.Target.GetPlatform());

	// Fixup autogen file
	ReplaceVirtualFilePathForShaderAutogen(PathModified, Context.ShaderInput.Target.GetPlatform(), &Context.ShaderInput.ShaderPlatformName);

	FString PathModifiedLowerCase = PathModified.ToLower();
	const TUniquePtr<ANSICHAR[]>* SeenPath = Context.SeenPathsLowerCase.Find(PathModifiedLowerCase);
	// Keep track of previously resolved paths in a case insensitive manner so preprocessor will handle #pragma once with files included with inconsistent casing correctly
	// (we store the first correctly resolved path with original casing so we get "nice" line directives)
	if (SeenPath)
	{
		return SeenPath->Get();
	}

	bool bExists =
		Context.ShaderInput.Environment.IncludeVirtualPathToContentsMap.Contains(PathModified) ||
		Context.ShaderInput.Environment.IncludeVirtualPathToExternalContentsMap.Contains(PathModified) ||
		// LoadShaderSourceFile will load the file if it exists, but then cache it internally, so the next call in StbLoadFile will be cheap
		// (and hence this is not overly wasteful)
		LoadShaderSourceFile(*PathModified, Context.ShaderInput.Target.GetPlatform(), nullptr, nullptr);

	if (bExists)
	{
		int32 Length = FPlatformString::ConvertedLength<ANSICHAR>(*PathModified);
		TUniquePtr<ANSICHAR[]>& OutPath = Context.SeenPathsLowerCase.Add(PathModifiedLowerCase, MakeUnique<ANSICHAR[]>(Length));
		FPlatformString::Convert<TCHAR, ANSICHAR>(OutPath.Get(), Length, *PathModified);
		return OutPath.Get();
	}

	return nullptr;
}

class FShaderPreprocessorModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		init_preprocessor(&StbLoadFile, &StbFreeFile, &StbResolveInclude);
		// disable the "directive not at start of line" error; this allows a few things:
		// 1. #define'ing #pragma messages - consumed by the preprocessor (to handle UESHADERMETADATA hackery)
		// 2. #define'ing other #pragmas (those not processed explicitly by the preprocessor are copied into the preprocessed code
		// 3. handling the HLSL infinity constant (1.#INF); STB preprocessor interprets any use of # as a directive which is not the case here
		pp_set_warning_mode(PP_RESULT_directive_not_at_start_of_line, PP_RESULT_MODE_no_warning); 
	}
};
IMPLEMENT_MODULE(FShaderPreprocessorModule, ShaderPreprocessor);

static void AddStbDefine(stb_arena* MacroArena, macro_definition**& StbDefines, const TCHAR* Name, const TCHAR* Value)
{
	FString Define(FString::Printf(TEXT("%s %s"), Name, Value));
	auto ConvertedDefine = StringCast<ANSICHAR>(*Define);
	arrput(StbDefines, pp_define(MacroArena, (ANSICHAR*)ConvertedDefine.Get()));
}

static void AddStbDefines(stb_arena* MacroArena, macro_definition**& StbDefines, TMap<FString, FString> DefinitionsMap)
{
	for (TMap<FString, FString>::TConstIterator It(DefinitionsMap); It; ++It)
	{
		AddStbDefine(MacroArena, StbDefines, *It.Key(), *It.Value());
	}
}

bool InnerPreprocessShaderStb(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const FShaderCompilerDefinitions& AdditionalDefines
)
{
	stb_arena MacroArena = { 0 };
	macro_definition** StbDefines = nullptr;

	AddStbDefines(&MacroArena, StbDefines, ShaderInput.Environment.GetDefinitions());
	AddStbDefines(&MacroArena, StbDefines, AdditionalDefines.GetDefinitionMap());
	AddStbDefine(&MacroArena, StbDefines, TEXT("_STB_PREPROCESS"), TEXT("1"));

	FStbPreprocessContext Context{ ShaderInput };

	auto InFilename = StringCast<ANSICHAR>(*ShaderInput.VirtualSourceFilePath);
	int NumDiagnostics = 0;
	pp_diagnostic* Diagnostics = nullptr;
	
	char* OutPreprocessedAnsi = preprocess_file(nullptr, InFilename.Get(), &Context, StbDefines, arrlen(StbDefines), &Diagnostics, &NumDiagnostics);
	bool HasError = false;
	if (Diagnostics != nullptr)
	{
		for (int DiagIndex = 0; DiagIndex < NumDiagnostics; ++DiagIndex)
		{
			pp_diagnostic* Diagnostic = &Diagnostics[DiagIndex];
			HasError |= (Diagnostic->error_level == PP_RESULT_MODE_error);
			
			FString Message = Diagnostic->message;
			// as we do with MCPP, we are ignoring warnings (for now?)
			if (Diagnostic->error_level == PP_RESULT_MODE_error)
			{
				FShaderCompilerError* CompilerError = new(ShaderOutput.Errors) FShaderCompilerError;
				CompilerError->ErrorVirtualFilePath = Diagnostic->where->filename;
				CompilerError->ErrorLineString = FString::Printf(TEXT("%d"), Diagnostic->where->line_number);
				CompilerError->StrippedErrorMessage = Message;
			}
			else
			{
				EMessageType Type = FilterPreprocessorError(Message);
				if (Type == EMessageType::ShaderMetaData)
				{
					FString Directive;
					ExtractDirective(Directive, Message);
					ShaderOutput.PragmaDirectives.Add(Directive);
				}
			}
		}
	}

	OutPreprocessedShader = StringCast<TCHAR>(OutPreprocessedAnsi).Get();

	if (!HasError && !Context.HasIncludedMandatoryHeaders())
	{
		LogMandatoryHeaderError(ShaderInput, ShaderOutput);
		HasError = true;
	}

	preprocessor_file_free(OutPreprocessedAnsi, Diagnostics);
	stbds_arrfree(StbDefines);
	stb_arena_free(&MacroArena);

	return !HasError;
}

/**
 * Preprocess a shader.
 * @param OutPreprocessedShader - Upon return contains the preprocessed source code.
 * @param ShaderOutput - ShaderOutput to which errors can be added.
 * @param ShaderInput - The shader compiler input.
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @param DefinesPolicy - Whether to add shader definitions as comments.
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

	bool bResult = false;
	bool bLegacyPreprocess = ShaderInput.Environment.CompilerFlags.Contains(CFLAG_UseLegacyPreprocessor);
	FString PreprocessorOutput;
	if (!bLegacyPreprocess)
	{
		bResult |= InnerPreprocessShaderStb(PreprocessorOutput, ShaderOutput, ShaderInput, AdditionalDefines);
	}
	else
	{
		bResult |= InnerPreprocessShaderMcpp(PreprocessorOutput, ShaderOutput, ShaderInput, AdditionalDefines);
	}

	// List the defines used for compilation in the preprocessed shaders, especially to know witch permutation vector this shader is.
	if (DefinesPolicy == EDumpShaderDefines::AlwaysIncludeDefines || (DefinesPolicy == EDumpShaderDefines::DontCare && ShaderInput.DumpDebugInfoPath.Len() > 0))
	{
		DumpShaderDefinesAsCommentedCode(ShaderInput, &OutPreprocessedShader);
	}

	OutPreprocessedShader += PreprocessorOutput;
	return bResult;
}
