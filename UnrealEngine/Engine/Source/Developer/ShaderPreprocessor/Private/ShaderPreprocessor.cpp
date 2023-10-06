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
	void LogMandatoryHeaderError(const FShaderCompilerInput& Input, FShaderPreprocessOutput& Output)
	{
		FString Path = Input.VirtualSourceFilePath;
		FString Message = FString::Printf(TEXT("Error: Shader is required to include %s"), *PlatformHeader);
		Output.LogError(MoveTemp(Path), MoveTemp(Message), 1);
	}
}

static void AddStbDefine(stb_arena* MacroArena, macro_definition**& StbDefines, const TCHAR* Name, const TCHAR* Value);
static void AddStbDefines(stb_arena* MacroArena, macro_definition**& StbDefines, TMap<FString, FString> DefinitionsMap);

class FShaderPreprocessorUtilities
{
public:
	static void DumpShaderDefinesAsCommentedCode(const FShaderCompilerEnvironment& Environment, FString* OutDefines)
	{
		const TMap<FString, FString>& Definitions = Environment.Definitions.GetDefinitionMap();
		TArray<FString> Keys;
		Definitions.GetKeys(/* out */ Keys);
		Keys.Sort();

		FString Defines;
		for (const FString& Key : Keys)
		{
			Defines += FString::Printf(TEXT("// #define %s %s\n"), *Key, *Definitions[Key]);
		}

		*OutDefines += MakeInjectedShaderCodeBlock(TEXT("DumpShaderDefinesAsCommentedCode"), Defines);
	}

	static void PopulateDefines(const FShaderCompilerEnvironment& Environment, const FShaderCompilerDefinitions& AdditionalDefines, stb_arena* MacroArena, macro_definition**& OutDefines)
	{
		AddStbDefines(MacroArena, OutDefines, Environment.Definitions.GetDefinitionMap());
		AddStbDefines(MacroArena, OutDefines, AdditionalDefines.GetDefinitionMap());
	}
};

//////////////////////////////////////////////////////////////////////////
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
	const FShaderCompilerEnvironment& Environment;
	TMap<FString, TArray<ANSICHAR>> LoadedIncludesCache;
	TMap<FString, TUniquePtr<ANSICHAR[]>> SeenPathsLowerCase;

	bool HasIncludedMandatoryHeaders()
	{
		return SeenPathsLowerCase.Contains(PlatformHeaderLowerCase);
	}
};

inline bool IsEndOfLine(TCHAR C)
{
	return C == TEXT('\r') || C == TEXT('\n');
}

inline bool CommentStripNeedsHandling(TCHAR C)
{
	return IsEndOfLine(C) || C == TEXT('/') || C == 0;
}

inline int NewlineCharCount(TCHAR First, TCHAR Second)
{
	return ((First + Second) == TEXT('\r') + TEXT('\n')) ? 2 : 1;
}

// Given an FString containing the contents of a shader source file, populates the given array with contents of
// that source file with all comments stripped. This is needed since the STB preprocessor itself does not strip 
// comments.
void ConvertAndStripComments(const FString& ShaderSource, TArray<ANSICHAR>& OutStripped)
{
	// STB preprocessor does not strip comments, so we do so here before returning the loaded source
	// Doing so is barely more costly than the memcopy we require anyways so has negligible overhead.
	// Reserve worst case (i.e. assuming there are no comments at all) to avoid reallocation
	int32 BufferSize = ShaderSource.Len() + 1; // +1 to append null terminator
	OutStripped.SetNumUninitialized(BufferSize);

	ANSICHAR* CurrentOut = OutStripped.GetData();

	const TCHAR* const End = ShaderSource.GetCharArray().GetData() + ShaderSource.Len();

	// We rely on null termination to avoid the need to check Current < End in some cases
	check(*End == TEXT('\0'));
	for (const TCHAR* Current = ShaderSource.GetCharArray().GetData(); Current < End;)
	{
		// sanity check that we're not overrunning the buffer
		check(CurrentOut < (OutStripped.GetData() + BufferSize));
		// CommentStripNeedsHandling returns true when *Current == '\0';
		while (!CommentStripNeedsHandling(*Current))
		{
			// straight cast to ansichar; since this is a character in hlsl source that's not in a comment
			// we assume that it must be valid to do so. if this assumption is not valid the shader source was
			// broken/corrupt anyways.
			*CurrentOut++ = (ANSICHAR)(*Current++);
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
				while (Current < End)
				{
					if (Current[0] == '*' && Current[1] == '/')
					{
						Current += 2;
						break;
					}
					else if (IsEndOfLine(*Current))
					{
						*CurrentOut++ = '\n';
						Current += NewlineCharCount(Current[0], Current[1]);
					}
					else
					{
						++Current;
					}
				}
			}
			else
			{
				*CurrentOut++ = (ANSICHAR)(*Current++);
			}
		}
	}
	// Null terminate after comment-stripped copy
	check(CurrentOut < (OutStripped.GetData() + BufferSize));
	*CurrentOut++ = 0;

	// Set correct length after stripping but don't bother shrinking/reallocating, minor memory overhead to save time
	OutStripped.SetNum(CurrentOut - OutStripped.GetData(), /* bAllowShrinking */false);
}

const FString* FindInMemorySource(const FShaderCompilerEnvironment& Environment, const FString& FilenameConverted)
{
	const FString* InMemorySource = Environment.IncludeVirtualPathToContentsMap.Find(FilenameConverted);
	if (!InMemorySource)
	{
		const FThreadSafeSharedStringPtr* SharedPtr = Environment.IncludeVirtualPathToExternalContentsMap.Find(FilenameConverted);
		InMemorySource = SharedPtr ? SharedPtr->Get() : nullptr;
	}
	return InMemorySource;
}

static const ANSICHAR* StbLoadFile(const ANSICHAR* Filename, void* RawContext, size_t* OutLength)
{
	FStbPreprocessContext& Context = *reinterpret_cast<FStbPreprocessContext*>(RawContext);
	FString FilenameConverted = StringCast<TCHAR>(Filename).Get();
	TArray<ANSICHAR>* ContentsCached = Context.LoadedIncludesCache.Find(FilenameConverted);
	if (!ContentsCached)
	{
		ContentsCached = &Context.LoadedIncludesCache.Add(FilenameConverted);
		// Local FString used for the LoadShaderSourceFile path; we should consider retrieving source from the shader file cache as a reference
		// (avoid an extra alloc+copy)
		FString SourceCopy;

		const FString* InMemorySource = FindInMemorySource(Context.Environment, FilenameConverted);

		if (!InMemorySource)
		{
			CheckShaderHashCacheInclude(FilenameConverted, Context.ShaderInput.Target.GetPlatform(), Context.ShaderInput.ShaderFormat.ToString());
			LoadShaderSourceFile(*FilenameConverted, Context.ShaderInput.Target.GetPlatform(), &SourceCopy, nullptr);
			InMemorySource = &SourceCopy;
		}
		check(InMemorySource && !InMemorySource->IsEmpty());
		ConvertAndStripComments(*InMemorySource, *ContentsCached);
	}
	check(ContentsCached);
	*OutLength = ContentsCached->Num();
	return ContentsCached->GetData();
}

static void StbFreeFile(const ANSICHAR* Filename, const ANSICHAR* Contents, void* RawContext)
{
	// No-op; stripped/converted shader source will be freed from the cache in FStbPreprocessContext when it's destructed;
	// we want to keep it around until that point in case includes are loaded multiple times from different source locations
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

	FixupShaderFilePath(PathModified, Context.ShaderInput.Target.GetPlatform(), &Context.ShaderInput.ShaderPlatformName);

	FString PathModifiedLowerCase = PathModified.ToLower();
	const TUniquePtr<ANSICHAR[]>* SeenPath = Context.SeenPathsLowerCase.Find(PathModifiedLowerCase);
	// Keep track of previously resolved paths in a case insensitive manner so preprocessor will handle #pragma once with files included with inconsistent casing correctly
	// (we store the first correctly resolved path with original casing so we get "nice" line directives)
	if (SeenPath)
	{
		return SeenPath->Get();
	}

	bool bExists =
		Context.Environment.IncludeVirtualPathToContentsMap.Contains(PathModified) ||
		Context.Environment.IncludeVirtualPathToExternalContentsMap.Contains(PathModified) ||
		// LoadShaderSourceFile will load the file if it exists, but then cache it internally, so the next call in StbLoadFile will be cheap
		// (and hence this is not wasteful, just performs the loading earlier)
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
	FShaderPreprocessOutput& Output,
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& Environment,
	const FShaderCompilerDefinitions& AdditionalDefines
)
{
	stb_arena MacroArena = { 0 };
	macro_definition** StbDefines = nullptr;
	FShaderPreprocessorUtilities::PopulateDefines(Environment, AdditionalDefines, &MacroArena, StbDefines);

	FStbPreprocessContext Context{ Input, Environment };

	auto InFilename = StringCast<ANSICHAR>(*Input.VirtualSourceFilePath);
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
			// ignore stb warnings (for now?)
			if (Diagnostic->error_level == PP_RESULT_MODE_error)
			{
				FString Filename = Diagnostic->where->filename;
				Output.LogError(MoveTemp(Filename), MoveTemp(Message), Diagnostic->where->line_number);
			}
			else
			{
				EMessageType Type = FilterPreprocessorError(Message);
				if (Type == EMessageType::ShaderMetaData)
				{
					FString Directive;
					ExtractDirective(Directive, Message);
					Output.AddDirective(MoveTemp(Directive));
				}
			}
		}
	}

	if (!HasError)
	{
		Output.EditSource().Append(OutPreprocessedAnsi);
	}

	if (!HasError && !Context.HasIncludedMandatoryHeaders())
	{
		LogMandatoryHeaderError(Input, Output);
		HasError = true;
	}

	preprocessor_file_free(OutPreprocessedAnsi, Diagnostics);
	stbds_arrfree(StbDefines);
	stb_arena_free(&MacroArena);

	return !HasError;
}

bool PreprocessShader(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const FShaderCompilerDefinitions& AdditionalDefines,
	EDumpShaderDefines DefinesPolicy)
{
	FShaderPreprocessOutput Output;
	// when called via this overload, environment is assumed to be already merged in input struct
	const FShaderCompilerEnvironment& Environment = ShaderInput.Environment;
	bool bSucceeded = PreprocessShader(Output, ShaderInput, Environment, AdditionalDefines, DefinesPolicy);

	OutPreprocessedShader = MoveTemp(Output.EditSource());

	Output.MoveDirectives(ShaderOutput.PragmaDirectives);
	for (FShaderCompilerError& Error : Output.EditErrors())
	{
		ShaderOutput.Errors.Add(MoveTemp(Error));
	}
	return bSucceeded;
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
	FShaderPreprocessOutput& Output,
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& Environment,
	const FShaderCompilerDefinitions& AdditionalDefines,
	EDumpShaderDefines DefinesPolicy
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PreprocessShader);

	// Skip the cache system and directly load the file path (used for debugging)
	if (Input.bSkipPreprocessedCache)
	{
		return FFileHelper::LoadFileToString(Output.EditSource(), *Input.VirtualSourceFilePath);
	}

	check(CheckVirtualShaderFilePath(Input.VirtualSourceFilePath));

	Output.EditSource().Empty();

	// List the defines used for compilation in the preprocessed shaders, especially to know which permutation vector this shader is.
	if (DefinesPolicy == EDumpShaderDefines::AlwaysIncludeDefines || (DefinesPolicy == EDumpShaderDefines::DontCare && Input.DumpDebugInfoPath.Len() > 0))
	{
		FShaderPreprocessorUtilities::DumpShaderDefinesAsCommentedCode(Environment, &Output.EditSource());
	}

	return InnerPreprocessShaderStb(Output, Input, Environment, AdditionalDefines);
}
