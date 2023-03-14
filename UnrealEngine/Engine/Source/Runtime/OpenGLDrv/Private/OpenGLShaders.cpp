// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLShaders.cpp: OpenGL shader RHI implementation.
=============================================================================*/

#include "OpenGLShaders.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "OpenGLDrvPrivate.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneUtils.h"
#include "PsoLruCache.h"
#include "RHICoreShader.h"
#include "OpenGLProgramBinaryFileCache.h"

#define CHECK_FOR_GL_SHADERS_TO_REPLACE 0

#if PLATFORM_WINDOWS
#include <mmintrin.h>
#endif
#include "SceneUtils.h"

static TAutoConsoleVariable<int32> CVarEnableLRU(
	TEXT("r.OpenGL.EnableProgramLRUCache"),
	0,
	TEXT("OpenGL program LRU cache.\n")
	TEXT("For use only when driver only supports a limited number of active GL programs.\n")
	TEXT("0: disable LRU. (default)\n")
	TEXT("1: When the LRU cache limits are reached, the least recently used GL program(s) will be deleted to make space for new/more recent programs. Expect hitching if requested shader is not in LRU cache."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarLRUMaxProgramCount(
	TEXT("r.OpenGL.ProgramLRUCount"),
	700,
	TEXT("OpenGL LRU maximum occupancy.\n")
	TEXT("Limit the maximum number of active shader programs at any one time.\n")
	TEXT("0: disable LRU.\n")
	TEXT("Non-Zero: Maximum number of active shader programs, if reached least, recently used shader programs will deleted. "),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLRUMaxProgramBinarySize(
	TEXT("r.OpenGL.ProgramLRUBinarySize"),
	35*1024*1024,
	TEXT("OpenGL LRU maximum binary shader size.\n")
	TEXT("Limit the maximum number of active shader programs at any one time.\n")
	TEXT("0: disable LRU. (default)\n")
	TEXT("Non-Zero: Maximum number of bytes active shader programs may use. If reached, least recently used shader programs will deleted."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLRUKeepProgramBinaryResident(
	TEXT("r.OpenGL.ProgramLRUKeepBinaryResident"),
	0,
	TEXT("OpenGL LRU should keep program binary in memory.\n")
	TEXT("Do not discard the program binary after creation of the GL program.\n")
	TEXT("0: Program binary is discarded after GL program creation and recreated on program eviction. (default)\n")
	TEXT("1: Program binary is retained, this improves eviction and re-creation performance but uses more memory."),
	ECVF_ReadOnly |ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarIgnoreLinkFailure(
	TEXT("r.OpenGL.IgnoreLinkFailure"),
	0,
	TEXT("Ignore OpenGL program link failures.\n")
	TEXT("0: Program link failure generates a fatal error when encountered. (default)\n")
	TEXT("1: Ignore link failures. this may allow a program to continue but could lead to undefined rendering behaviour."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarIgnoreShaderCompileFailure(
	TEXT("r.OpenGL.IgnoreShaderCompileFailure"),
	0,
	TEXT("Ignore OpenGL shader compile failures.\n")
	TEXT("0: Shader compile failure return an error when encountered. (default)\n")
	TEXT("1: Ignore Shader compile failures."),
	ECVF_RenderThreadSafe);

#if PLATFORM_ANDROID
bool GOpenGLShaderHackLastCompileSuccess = false;
#endif

#define VERIFY_GL_SHADER_LINK 1
#define VERIFY_GL_SHADER_COMPILE 1

static bool ReportShaderCompileFailures()
{
	bool bReportCompileFailures = true;
#if PLATFORM_ANDROID
	const FString * ConfigRulesReportGLShaderCompileFailures = FAndroidMisc::GetConfigRulesVariable(TEXT("ReportGLShaderCompileFailures"));
	bReportCompileFailures = ConfigRulesReportGLShaderCompileFailures == nullptr || ConfigRulesReportGLShaderCompileFailures->Equals("true", ESearchCase::IgnoreCase);
#endif

#if VERIFY_GL_SHADER_COMPILE
	return bReportCompileFailures;
#else
	return false;
#endif
}

static bool ReportProgramLinkFailures()
{
	bool bReportLinkFailures = true;
#if PLATFORM_ANDROID
	const FString* ConfigRulesReportGLProgramLinkFailures = FAndroidMisc::GetConfigRulesVariable(TEXT("ReportGLProgramLinkFailures"));
	bReportLinkFailures = ConfigRulesReportGLProgramLinkFailures == nullptr || ConfigRulesReportGLProgramLinkFailures->Equals("true", ESearchCase::IgnoreCase);
#endif

#if VERIFY_GL_SHADER_LINK
	return bReportLinkFailures;
#else
	return false;
#endif
}


static uint32 GCurrentDriverProgramBinaryAllocation = 0;
static uint32 GNumPrograms = 0;

static void PrintProgramStats()
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT(" --- Programs Num: %d, Size: %d \n"), GNumPrograms, GCurrentDriverProgramBinaryAllocation);
}

static FAutoConsoleCommand ConsoleCommandPrintProgramStats(
								TEXT("r.OpenGL.PrintProgramStats"),
								TEXT("Print to log current program binary stats"),
								FConsoleCommandDelegate::CreateStatic(PrintProgramStats)
);

static void SetNewProgramStats(GLuint Program)
{
	VERIFY_GL_SCOPE();

#if STATS | VERIFY_GL_SHADER_LINK
	GLint BinaryLength = 0;
	glGetProgramiv(Program, GL_PROGRAM_BINARY_LENGTH, &BinaryLength);
#endif

#if STATS
	INC_MEMORY_STAT_BY(STAT_OpenGLProgramBinaryMemory, BinaryLength);
	INC_DWORD_STAT(STAT_OpenGLProgramCount);
#endif
	
	GNumPrograms++;
#if VERIFY_GL_SHADER_LINK
	GCurrentDriverProgramBinaryAllocation += BinaryLength;
#endif
}

static void SetDeletedProgramStats(GLuint Program)
{
	VERIFY_GL_SCOPE();
#if STATS | VERIFY_GL_SHADER_LINK
	GLint BinaryLength = 0;
	glGetProgramiv(Program, GL_PROGRAM_BINARY_LENGTH, &BinaryLength);
#endif

#if STATS
	DEC_MEMORY_STAT_BY(STAT_OpenGLProgramBinaryMemory, BinaryLength);
	DEC_DWORD_STAT(STAT_OpenGLProgramCount);
#endif

#if VERIFY_GL_SHADER_LINK
	GCurrentDriverProgramBinaryAllocation -= BinaryLength;
#endif
	GNumPrograms--;
}

// Create any resources that are required by internal ogl rhi functions.
void FOpenGLDynamicRHI::SetupRecursiveResources()
{
	NULLPixelShaderRHI = GetNULLPixelShader();
}

FRHIPixelShader* FOpenGLDynamicRHI::GetNULLPixelShader() const
{
	if (NULLPixelShaderRHI)
	{
		return NULLPixelShaderRHI;
	}
	else
	{
		auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FNULLPS> PixelShader(ShaderMap);
		return PixelShader.GetPixelShader();
	}
}

const uint32 SizeOfFloat4 = 16;
const uint32 NumFloatsInFloat4 = 4;

FORCEINLINE void FOpenGLShaderParameterCache::FRange::MarkDirtyRange(uint32 NewStartVector, uint32 NewNumVectors)
{
	if (NumVectors > 0)
	{
		uint32 High = StartVector + NumVectors;
		uint32 NewHigh = NewStartVector + NewNumVectors;
		
		uint32 MaxVector = FMath::Max(High, NewHigh);
		uint32 MinVector = FMath::Min(StartVector, NewStartVector);
		
		StartVector = MinVector;
		NumVectors = (MaxVector - MinVector) + 1;
	}
	else
	{
		StartVector = NewStartVector;
		NumVectors = NewNumVectors;
	}
}

/**
 * Verify that an OpenGL program has linked successfully.
 */
static bool VerifyLinkedProgram(GLuint Program)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLinkVerifyTime);
	VERIFY_GL_SCOPE();

	GLint LinkStatus = 0;
	glGetProgramiv(Program, GL_LINK_STATUS, &LinkStatus);
	if (LinkStatus != GL_TRUE)
	{
		if (ReportProgramLinkFailures())
		{
			GLenum LastGLError = glGetError();
			GLint LogLength;
			ANSICHAR DefaultLog[] = "No log";
			ANSICHAR *CompileLog = DefaultLog;
			glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
			if (LogLength > 1)
			{
				CompileLog = (ANSICHAR *)FMemory::Malloc(LogLength);
				glGetProgramInfoLog(Program, LogLength, NULL, CompileLog);
			}
			UE_LOG(LogRHI, Error, TEXT("Failed to link program. Current total programs: %d program binary bytes, last gl error 0x%X, drvalloc %d\n  log:\n%s"),
				GNumPrograms,
				LastGLError,
				GCurrentDriverProgramBinaryAllocation,
				ANSI_TO_TCHAR(CompileLog));

			if (LogLength > 1)
			{
				FMemory::Free(CompileLog);
			}
		}
		else
		{
			UE_LOG(LogRHI, Error, TEXT("Failed to link program. Current total programs:%d"), GNumPrograms);
		}
		// if we're required to ignore link failure then we return true here.
		return CVarIgnoreLinkFailure.GetValueOnAnyThread() == 1;
	}
	return true;
}

// ============================================================================================================================

class FOpenGLCompiledShaderValue
{
	const FName CompressionMethod = NAME_Oodle;

public:
	FOpenGLCompiledShaderValue(GLuint ResourceIN, const TArray<ANSICHAR>& GlslCodeIN)
		: Resource(ResourceIN)
	{
		CompressShader(GlslCodeIN);
	}
	~FOpenGLCompiledShaderValue()
	{
		StatTotalStoredSize -= GlslCode.Num();
		StatTotalUncompressedSize -= UncompressedSize == -1 ? GlslCode.Num() : UncompressedSize;
	}

	GLuint Resource = 0;

	TArray<ANSICHAR>  GetUncompressedShader() const
	{
		TArray<ANSICHAR> OutGlslCode;

		QUICK_SCOPE_CYCLE_COUNTER(STAT_glUncompressShader);

		if (UncompressedSize != -1)
		{
			OutGlslCode.Empty(UncompressedSize);
			OutGlslCode.SetNum(UncompressedSize);

			bool bResult = FCompression::UncompressMemory(
				CompressionMethod,
				(void*)OutGlslCode.GetData(),
				UncompressedSize,
				(void*)GlslCode.GetData(),
				GlslCode.Num());

			check(bResult);
		}
		else
		{
			OutGlslCode = GlslCode;
		}
		return OutGlslCode;
	}

	static TAtomic<uint32> StatTotalStoredSize;
	static TAtomic<uint32> StatTotalUncompressedSize;
	bool bHasCompiled = false;
private:
	TArray<ANSICHAR> GlslCode;
	int32 UncompressedSize = -1;

	void CompressShader(const TArray<ANSICHAR>& InGlslCode)
	{
		static_assert(sizeof(InGlslCode[0]) == sizeof(uint8), "expecting shader code type to be byte.");
		check(GlslCode.IsEmpty());

		UncompressedSize = InGlslCode.Num();
		int32 CompressedSize = FCompression::CompressMemoryBound(CompressionMethod, UncompressedSize);

		GlslCode.Empty(CompressedSize);
		GlslCode.SetNumUninitialized(CompressedSize);

		bool bCompressed = FCompression::CompressMemory(
			CompressionMethod,
			(void*)GlslCode.GetData(),
			CompressedSize,
			(void*)InGlslCode.GetData(),
			UncompressedSize,
			COMPRESS_BiasMemory);

		if (bCompressed)
		{
			// shrink buffer
			GlslCode.SetNum(CompressedSize, true);
		}
		else
		{
			GlslCode = InGlslCode;
			UncompressedSize = -1;
		}

		StatTotalStoredSize += GlslCode.Num();
		StatTotalUncompressedSize += UncompressedSize == -1 ? GlslCode.Num() : UncompressedSize;

 		//UE_LOG(LogRHI, Warning, TEXT("Shader sizes: %d %d"), StatTotalStoredSize.Load(EMemoryOrder::Relaxed), StatTotalUncompressedSize.Load(EMemoryOrder::Relaxed));
	}

};

TAtomic<uint32> FOpenGLCompiledShaderValue::StatTotalStoredSize = 0;
TAtomic<uint32> FOpenGLCompiledShaderValue::StatTotalUncompressedSize = 0;

typedef TMap<FOpenGLCompiledShaderKey, TSharedPtr<FOpenGLCompiledShaderValue> > FOpenGLCompiledShaderCache;

static FCriticalSection GCompiledShaderCacheCS;

static FOpenGLCompiledShaderCache& GetOpenGLCompiledShaderCache()
{
	static FOpenGLCompiledShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}

struct FLibraryShaderCacheValue
{
	FOpenGLCodeHeader* Header;
	uint32 ShaderCrc;
	GLuint GLShader;
	TArray<FUniformBufferStaticSlot> StaticSlots;

#if DEBUG_GL_SHADERS
	TArray<ANSICHAR> GlslCode;
	const ANSICHAR*  GlslCodeString; // make it easier in VS to see shader code in debug mode; points to begin of GlslCode
#endif
};

typedef TMap<FSHAHash, FLibraryShaderCacheValue> FOpenGLCompiledLibraryShaderCache;

static FOpenGLCompiledLibraryShaderCache& GetOpenGLCompiledLibraryShaderCache()
{
	static FOpenGLCompiledLibraryShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}

// ============================================================================================================================


static const TCHAR* ShaderNameFromShaderType(GLenum ShaderType)
{
	switch(ShaderType)
	{
		case GL_VERTEX_SHADER: return TEXT("vertex");
		case GL_FRAGMENT_SHADER: return TEXT("fragment");
		case GL_GEOMETRY_SHADER: return TEXT("geometry");
		case GL_COMPUTE_SHADER: return TEXT("compute");
		default: return NULL;
	}
}

// ============================================================================================================================

namespace
{
	inline void AppendCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source)
	{
		if (Dest.Num() > 0)
		{
			Dest.Insert(Source, FCStringAnsi::Strlen(Source), Dest.Num() - 1);;
		}
		else
		{
			Dest.Append(Source, FCStringAnsi::Strlen(Source) + 1);
		}
	}

	inline void ReplaceCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source, const ANSICHAR * Replacement)
	{
		int32 SourceLen = FCStringAnsi::Strlen(Source);
		int32 ReplacementLen = FCStringAnsi::Strlen(Replacement);
		int32 FoundIndex = 0;
		for (const ANSICHAR * FoundPointer = FCStringAnsi::Strstr(Dest.GetData(), Source);
			nullptr != FoundPointer;
			FoundPointer = FCStringAnsi::Strstr(Dest.GetData()+FoundIndex, Source))
		{
			FoundIndex = FoundPointer - Dest.GetData();
			Dest.RemoveAt(FoundIndex, SourceLen);
			Dest.Insert(Replacement, ReplacementLen, FoundIndex);
		}
	}

	inline const ANSICHAR * CStringEndOfLine(const ANSICHAR * Text)
	{
		const ANSICHAR * LineEnd = FCStringAnsi::Strchr(Text, '\n');
		if (nullptr == LineEnd)
		{
			LineEnd = Text + FCStringAnsi::Strlen(Text);
		}
		return LineEnd;
	}

	inline bool CStringIsBlankLine(const ANSICHAR * Text)
	{
		while (!FCharAnsi::IsLinebreak(*Text))
		{
			if (!FCharAnsi::IsWhitespace(*Text))
			{
				return false;
			}
			++Text;
		}
		return true;
	}

	inline int CStringCountOccurances(TArray<ANSICHAR> & Source, const ANSICHAR * TargetString)
	{
		int32 TargetLen = FCStringAnsi::Strlen(TargetString);
		int Count = 0;
		int32 FoundIndex = 0;
		for (const ANSICHAR * FoundPointer = FCStringAnsi::Strstr(Source.GetData(), TargetString);
			nullptr != FoundPointer;
			FoundPointer = FCStringAnsi::Strstr(Source.GetData() + FoundIndex, TargetString))
		{
			FoundIndex = FoundPointer - Source.GetData();
			FoundIndex += TargetLen;
			Count++;
		}
		return Count;
	}

	inline bool MoveHashLines(TArray<ANSICHAR> & Dest, TArray<ANSICHAR> & Source)
	{
		// Walk through the lines to find the first non-# line...
		const ANSICHAR * LineStart = Source.GetData();
		for (bool FoundNonHashLine = false; !FoundNonHashLine;)
		{
			const ANSICHAR * LineEnd = CStringEndOfLine(LineStart);
			if (LineStart[0] != '#' && !CStringIsBlankLine(LineStart))
			{
				FoundNonHashLine = true;
			}
			else if (LineEnd[0] == '\n')
			{
				LineStart = LineEnd + 1;
			}
			else
			{
				LineStart = LineEnd;
			}
		}
		// Copy the hash lines over, if we found any. And delete from
		// the source.
		if (LineStart > Source.GetData())
		{
			int32 LineLength = LineStart - Source.GetData();
			if (Dest.Num() > 0)
			{
				Dest.Insert(Source.GetData(), LineLength, Dest.Num() - 1);
			}
			else
			{
				Dest.Append(Source.GetData(), LineLength);
				Dest.Append("", 1);
			}
			if (Dest.Last(1) != '\n')
			{
				Dest.Insert("\n", 1, Dest.Num() - 1);
			}
			Source.RemoveAt(0, LineStart - Source.GetData());
			return true;
		}
		return false;
	}
}

// make some anon ns functions available to platform extensions
void PE_AppendCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source)
{
	AppendCString(Dest, Source);
}

void PE_ReplaceCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source, const ANSICHAR * Replacement)
{
	ReplaceCString(Dest, Source, Replacement);
}

inline uint32 GetTypeHash(FAnsiCharArray const& CharArray)
{
	return FCrc::MemCrc32(CharArray.GetData(), CharArray.Num() * sizeof(ANSICHAR));
}

// Helper to compile a shader 
// returns true if shader was compiled without any errors or errors should be ignored
static bool CompileCurrentShader(const GLuint Resource, const FAnsiCharArray& GlslCode)
{
	VERIFY_GL_SCOPE();
	const ANSICHAR * GlslCodeString = GlslCode.GetData();
	int32 GlslCodeLength = GlslCode.Num() - 1;

	glShaderSource(Resource, 1, (const GLchar**)&GlslCodeString, &GlslCodeLength);
	glCompileShader(Resource);

	// Verify that an OpenGL shader has compiled successfully.
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCompileVerifyTime);
	{
		GLint CompileStatus;
		glGetShaderiv(Resource, GL_COMPILE_STATUS, &CompileStatus);
		if (CompileStatus != GL_TRUE)
		{
			if (ReportShaderCompileFailures())
			{
				GLint LogLength;
				ANSICHAR DefaultLog[] = "No log";
				ANSICHAR *CompileLog = DefaultLog;
				glGetShaderiv(Resource, GL_INFO_LOG_LENGTH, &LogLength);
#if PLATFORM_ANDROID
				if ( LogLength == 0 )
				{
					// make it big anyway
					// there was a bug in android 2.2 where glGetShaderiv would return 0 even though there was a error message
					// https://code.google.com/p/android/issues/detail?id=9953
					LogLength = 4096;
				}
#endif
				if (LogLength > 1)
				{
					CompileLog = (ANSICHAR *)FMemory::Malloc(LogLength);
					glGetShaderInfoLog(Resource, LogLength, NULL, CompileLog);
				}

#if DEBUG_GL_SHADERS
				if (GlslCodeString)
				{
					UE_LOG(LogRHI,Error,TEXT("Shader:\n%s"),ANSI_TO_TCHAR(GlslCodeString));
				}
#endif
				UE_LOG(LogRHI,Error,TEXT("Failed to compile shader. Compile log:\n%s"), ANSI_TO_TCHAR(CompileLog));
				if (LogLength > 1)
				{
					FMemory::Free(CompileLog);
				}
			}
			// if we're required to ignore compile failure then we return true here, it will end with link failure.
			return CVarIgnoreShaderCompileFailure.GetValueOnAnyThread() == 1;
		}
	}
	return true;
}

static const FOpenGLShaderDeviceCapabilities& GetOpenGLShaderDeviceCapabilities()
{
	static bool bInitialized = false;

	static FOpenGLShaderDeviceCapabilities Capabilities;
	if( !bInitialized )
	{
		GetCurrentOpenGLShaderDeviceCapabilities(Capabilities);
		bInitialized = true;
	}
	return Capabilities;
}

static void GLSLToPlatform(const FOpenGLCodeHeader& Header, GLenum TypeEnum, FAnsiCharArray& GlslCodeOriginal, FAnsiCharArray& GlslPlatformCodeOUT)
{
	const FOpenGLShaderDeviceCapabilities& Capabilities = GetOpenGLShaderDeviceCapabilities();

	// get a modified version of the shader based on device capabilities to compile (destructive to GlslCodeOriginal copy)
	GLSLToDeviceCompatibleGLSL(GlslCodeOriginal, Header.ShaderName, TypeEnum, Capabilities, GlslPlatformCodeOUT);
}

/**
 * Compiles an OpenGL shader using the given GLSL microcode.
 * @returns the compiled shader upon success.
 */
template <typename ShaderType, typename TRHIType>
ShaderType* CompileOpenGLShader(const FOpenGLCodeHeader& Header, const FOpenGLCompiledShaderKey& SourceKey, const FSHAHash& LibraryHash, TRHIType* RHIShader)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCompileTime);
	VERIFY_GL_SCOPE();

	ShaderType* Shader = nullptr;
	{
		FLibraryShaderCacheValue *Val = GetOpenGLCompiledLibraryShaderCache().Find(LibraryHash);
		if (Val)
		{
			Shader = new ShaderType();
			Shader->Resource = Val->GLShader;
			Shader->Bindings = Val->Header->Bindings;
			Shader->UniformBuffersCopyInfo = Val->Header->UniformBuffersCopyInfo;
			Shader->StaticSlots = Val->StaticSlots;
#if DEBUG_GL_SHADERS
			Shader->GlslCode = Val->GlslCode;
			Shader->GlslCodeString = (ANSICHAR*)Shader->GlslCode.GetData();
#endif
			return Shader;
		}
	}
	// Find the existing compiled shader in the cache.
	FScopeLock Lock(&GCompiledShaderCacheCS);
	TSharedPtr<FOpenGLCompiledShaderValue> FoundShader = GetOpenGLCompiledShaderCache().FindRef(SourceKey);
	check(FoundShader);
	GLuint Resource = FoundShader->Resource;

	if (Resource == 0)
	{
		const GLenum TypeEnum = ShaderType::TypeEnum;
		Resource = FOpenGL::CreateShader(TypeEnum);
		const FOpenGLShaderDeviceCapabilities& Capabilities = GetOpenGLShaderDeviceCapabilities();

		// Save the code and defer compilation if our device supports program binaries and we're not checking for shader compatibility.
		const bool bDeferredCompilation = FOpenGLProgramBinaryCache::IsEnabled();
		if (bDeferredCompilation)
		{
			// do nothing...
		}
		else
		{
			const FAnsiCharArray GlslPlatformCode = FoundShader->GetUncompressedShader();
			const bool bSuccessfullyCompiled = CompileCurrentShader(Resource, GlslPlatformCode);
			ensure(bSuccessfullyCompiled);
		}

		FoundShader->Resource = Resource;
	}

	Shader = new ShaderType();
	Shader->Resource = Resource;
	Shader->Bindings = Header.Bindings;
	Shader->UniformBuffersCopyInfo = Header.UniformBuffersCopyInfo;

	UE::RHICore::InitStaticUniformBufferSlots(Shader->StaticSlots, Shader->Bindings.ShaderResourceTable);

#if DEBUG_GL_SHADERS
	{
		const FAnsiCharArray glslPlatformCode = FoundShader->GetUncompressedShader();
		Shader->GlslCode = glslPlatformCode;
		Shader->GlslCodeString = (ANSICHAR*)Shader->GlslCode.GetData();
	}
#endif
	if (LibraryHash != FSHAHash() && !GetOpenGLCompiledLibraryShaderCache().Contains(LibraryHash))
	{
		FLibraryShaderCacheValue Val;
		Val.GLShader = Resource;
		Val.Header = new FOpenGLCodeHeader;
		*Val.Header = Header;
		Val.ShaderCrc = SourceKey.GetCodeCRC();
		Val.StaticSlots = Shader->StaticSlots;
#if DEBUG_GL_SHADERS
		Val.GlslCode = FoundShader->GetUncompressedShader();
		Val.GlslCodeString = (ANSICHAR*)Shader->GlslCode.GetData();
#endif
		GetOpenGLCompiledLibraryShaderCache().Add(LibraryHash, Val);
	}

	return Shader;
}

void OPENGLDRV_API GetCurrentOpenGLShaderDeviceCapabilities(FOpenGLShaderDeviceCapabilities& Capabilities)
{
	FMemory::Memzero(Capabilities);

#if PLATFORM_DESKTOP
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Desktop;
	if (FOpenGL::IsAndroidGLESCompatibilityModeEnabled())
	{
		Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Android;
		Capabilities.bSupportsShaderFramebufferFetch = FOpenGL::SupportsShaderFramebufferFetch();
		Capabilities.bRequiresARMShaderFramebufferFetchDepthStencilUndef = false;
		Capabilities.MaxVaryingVectors = FOpenGL::GetMaxVaryingVectors();
	}

#elif PLATFORM_ANDROID
		Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Android;
		Capabilities.bSupportsShaderFramebufferFetch = FOpenGL::SupportsShaderFramebufferFetch();
		Capabilities.bRequiresARMShaderFramebufferFetchDepthStencilUndef = FOpenGL::RequiresARMShaderFramebufferFetchDepthStencilUndef();
		Capabilities.MaxVaryingVectors = FOpenGL::GetMaxVaryingVectors();
		Capabilities.bRequiresDisabledEarlyFragmentTests = FOpenGL::RequiresDisabledEarlyFragmentTests();
#elif PLATFORM_IOS
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_iOS;
#else
	FOpenGL::PE_GetCurrentOpenGLShaderDeviceCapabilities(Capabilities); // platform extension
#endif
	Capabilities.MaxRHIShaderPlatform = GMaxRHIShaderPlatform;
}

void OPENGLDRV_API GLSLToDeviceCompatibleGLSL(FAnsiCharArray& GlslCodeOriginal, const FString& ShaderName, GLenum TypeEnum, const FOpenGLShaderDeviceCapabilities& Capabilities, FAnsiCharArray& GlslCode)
{
	if (FOpenGL::PE_GLSLToDeviceCompatibleGLSL(GlslCodeOriginal, ShaderName, TypeEnum, Capabilities, GlslCode))
	{
		return; // platform extension overrides
	}

	// Whether we need to emit mobile multi-view code or not.
	const bool bEmitMobileMultiView = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), "gl_ViewID_OVR") != nullptr);

	// Whether we need to emit texture external code or not.
	const bool bEmitTextureExternal = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), "samplerExternalOES") != nullptr);

	FAnsiCharArray GlslCodeAfterExtensions;
	const ANSICHAR* GlslPlaceHolderAfterExtensions = "// end extensions";
	bool bGlslCodeHasExtensions = CStringCountOccurances(GlslCodeOriginal, GlslPlaceHolderAfterExtensions) == 1;
	
	if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_Android)
	{
		const ANSICHAR* ESVersion = "#version 320 es";

		bool FoundVersion = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), ESVersion)) != nullptr;

		if (!FoundVersion)
		{
			ESVersion = "#version 310 es";
		}
		
		AppendCString(GlslCode, ESVersion);
		AppendCString(GlslCode, "\n");
		ReplaceCString(GlslCodeOriginal, ESVersion, "");
	}

	if (TypeEnum == GL_FRAGMENT_SHADER && Capabilities.bRequiresDisabledEarlyFragmentTests)
	{
		ReplaceCString(GlslCodeOriginal, "layout(early_fragment_tests) in;", "");
	}

	// The incoming glsl may have preprocessor code that is dependent on defines introduced via the engine.
	// This is the place to insert such engine preprocessor defines, immediately after the glsl version declaration.
	if (TypeEnum == GL_FRAGMENT_SHADER)
	{
		if (FOpenGL::SupportsPixelLocalStorage() && FOpenGL::SupportsShaderDepthStencilFetch())
		{
			AppendCString(GlslCode, "#define UE_MRT_PLS 1\n");
		}
		else if(FOpenGL::SupportsShaderMRTFramebufferFetch())
		{
			AppendCString(GlslCode, "#define UE_MRT_FRAMEBUFFER_FETCH 1\n");
		}
	}

	if (bEmitTextureExternal)
	{
		// remove comment so MoveHashLines works as intended
		ReplaceCString(GlslCodeOriginal, "// Uses samplerExternalOES", "");

		MoveHashLines(GlslCode, GlslCodeOriginal);

		if (GSupportsImageExternal)
		{
			AppendCString(GlslCode, "\n\n");

#if PLATFORM_ANDROID
			FOpenGL::EImageExternalType ImageExternalType = FOpenGL::GetImageExternalType();
			switch (ImageExternalType)
			{
				case FOpenGL::EImageExternalType::ImageExternal100:
					AppendCString(GlslCode, "#extension GL_OES_EGL_image_external : require\n");
					break;

				case FOpenGL::EImageExternalType::ImageExternal300:
					AppendCString(GlslCode, "#extension GL_OES_EGL_image_external : require\n");
					break;

				case FOpenGL::EImageExternalType::ImageExternalESSL300:
					// GL_OES_EGL_image_external_essl3 is only compatible with ES 3.x
					AppendCString(GlslCode, "#extension GL_OES_EGL_image_external_essl3 : require\n");
					break;
			}
#else
			AppendCString(GlslCode, "#extension GL_OES_EGL_image_external : require\n");
#endif
			AppendCString(GlslCode, "\n\n");
		}
		else
		{
			// Strip out texture external for devices that don't support it.
			AppendCString(GlslCode, "#define samplerExternalOES sampler2D\n");
		}
	}

	if (bEmitMobileMultiView)
	{
		MoveHashLines(GlslCode, GlslCodeOriginal);

		if (GSupportsMobileMultiView)
		{
			AppendCString(GlslCode, "\n\n");
			AppendCString(GlslCode, "#extension GL_OVR_multiview2 : enable\n");
			AppendCString(GlslCode, "\n\n");
		}
		else
		{
			// Strip out multi-view for devices that don't support it.
			AppendCString(GlslCode, "#define gl_ViewID_OVR 0\n");
		}
	}

	// Move version tag & extensions before beginning all other operations
	MoveHashLines(GlslCode, GlslCodeOriginal);

	// OpenGL SM5 shader platforms require location declarations for the layout, but don't necessarily use SSOs
	if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_Desktop)
	{
		AppendCString(GlslCode, "#define INTERFACE_BLOCK(Pos, Interp, Modifiers, Semantic, PreType, PostType) layout(location=Pos) Interp Modifiers struct { PreType PostType; }\n");
	}
	else
	{
		AppendCString(GlslCode, "#define INTERFACE_BLOCK(Pos, Interp, Modifiers, Semantic, PreType, PostType) layout(location=Pos) Modifiers Semantic { PreType PostType; }\n");
	}

	if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_Desktop)
	{
		// If we're running <= featurelevel es3.1 shaders then enable this extension which adds support for uintBitsToFloat etc.
		if ((FCStringAnsi::Strstr(GlslCode.GetData(), "#version 150") != nullptr))
		{
			AppendCString(GlslCode, "\n\n");
			AppendCString(GlslCode, "#extension GL_ARB_gpu_shader5 : enable\n");
			AppendCString(GlslCode, "\n\n");
		}
	}

#if	DEBUG_GL_SHADERS
	if (ShaderName.IsEmpty() == false)
	{
		AppendCString(GlslCode, "// ");
		AppendCString(GlslCode, TCHAR_TO_ANSI(ShaderName.GetCharArray().GetData()));
		AppendCString(GlslCode, "\n");
	}
#endif

	if (bEmitMobileMultiView && GSupportsMobileMultiView && TypeEnum == GL_VERTEX_SHADER)
	{
		AppendCString(GlslCode, "\n\n");
		AppendCString(GlslCode, "layout(num_views = 2) in;\n");
		AppendCString(GlslCode, "\n\n");
	}

	if (TypeEnum != GL_COMPUTE_SHADER)
	{
		if (FOpenGL::SupportsClipControl())
		{
			AppendCString(GlslCode, "#define HLSLCC_DX11ClipSpace 0 \n");
		}
		else
		{
			AppendCString(GlslCode, "#define HLSLCC_DX11ClipSpace 1 \n");
		}
	}

	// Append the possibly edited shader to the one we will compile.
	// This is to make it easier to debug as we can see the whole
	// shader source.
	AppendCString(GlslCode, "\n\n");
	AppendCString(GlslCode, GlslCodeOriginal.GetData());

	if (bGlslCodeHasExtensions && GlslCodeAfterExtensions.Num() > 0)
	{
		// the initial code has an #extension chunk. replace the placeholder line
		ReplaceCString(GlslCode, GlslPlaceHolderAfterExtensions, GlslCodeAfterExtensions.GetData());
	}
}

/**
 * Helper for constructing strings of the form XXXXX##.
 * @param Str - The string to build.
 * @param Offset - Offset into the string at which to set the number.
 * @param Index - Number to set. Must be in the range [0,100).
 */
static ANSICHAR* SetIndex(ANSICHAR* Str, int32 Offset, int32 Index)
{
	check(Index >= 0 && Index < 100);

	Str += Offset;
	if (Index >= 10)
	{
		*Str++ = '0' + (ANSICHAR)(Index / 10);
	}
	*Str++ = '0' + (ANSICHAR)(Index % 10);
	*Str = '\0';
	return Str;
}

template<typename RHIType>
RHIType* CreateProxyShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	typedef typename TOpenGLResourceTraits<RHIType>::TConcreteType TOGLProxyType;

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FShaderCodeReader ShaderCode(Code);

	const GLenum TypeEnum = TOGLProxyType::ContainedGLType::TypeEnum;
	FMemoryReaderView Ar(Code, true);

	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	FOpenGLCodeHeader Header = { 0 };
	Ar << Header;
	// Suppress static code analysis warning about a potential comparison of two constants
	CA_SUPPRESS(6326);
	if (Header.GlslMarker != 0x474c534c
		|| (TypeEnum == GL_VERTEX_SHADER && Header.FrequencyMarker != 0x5653)
		|| (TypeEnum == GL_FRAGMENT_SHADER && Header.FrequencyMarker != 0x5053)
		|| (TypeEnum == GL_GEOMETRY_SHADER && Header.FrequencyMarker != 0x4753)
		|| (TypeEnum == GL_COMPUTE_SHADER && Header.FrequencyMarker != 0x4353)
		)
	{
		UE_LOG(LogRHI, Fatal,
			TEXT("Corrupt shader bytecode. GlslMarker=0x%08x FrequencyMarker=0x%04x"),
			Header.GlslMarker,
			Header.FrequencyMarker
		);
		return nullptr;
	}

	int32 CodeOffset = Ar.Tell();

	// The code as given to us.

	// put back the 'original code crc' in to cache key
	// pull back out the modified glsl.

 	FAnsiCharArray GlslCodeOriginal;
 	AppendCString(GlslCodeOriginal, (ANSICHAR*)Code.GetData() + CodeOffset);
	uint32 CodeCRC = FCrc::MemCrc32(GlslCodeOriginal.GetData(), GlslCodeOriginal.Num());
	FOpenGLCompiledShaderKey ShaderCodeKey(TypeEnum, GlslCodeOriginal.Num(), CodeCRC);

#if CHECK_FOR_GL_SHADERS_TO_REPLACE
	{
		// 1. Check for specific file
		FString PotentialShaderFileName = FString::Printf(TEXT("%s-%d-0x%x.txt"), ShaderNameFromShaderType(TypeEnum), glslPlatformCode.Num(), CodeCRC);
		FString PotentialShaderFile = FPaths::ProfilingDir();
		PotentialShaderFile *= PotentialShaderFileName;

		UE_LOG(LogRHI, Log, TEXT("Looking for shader file '%s' for potential replacement."), *PotentialShaderFileName);

		int64 FileSize = IFileManager::Get().FileSize(*PotentialShaderFile);
		if (FileSize > 0)
		{
			FArchive* Ar = IFileManager::Get().CreateFileReader(*PotentialShaderFile);
			if (Ar != NULL)
			{
				UE_LOG(LogRHI, Log, TEXT("Replacing %s shader with length %d and CRC 0x%x with the one from a file."), (TypeEnum == GL_VERTEX_SHADER) ? TEXT("vertex") : ((TypeEnum == GL_FRAGMENT_SHADER) ? TEXT("fragment") : TEXT("geometry")), GlslCodeOriginal.Num(), GlslCodeOriginalCRC);

				// read in the file
				GlslCodeOriginal.Empty();
				GlslCodeOriginal.AddUninitialized(FileSize + 1);
				Ar->Serialize(GlslCodeOriginal.GetData(), FileSize);
				delete Ar;
				GlslCodeOriginal[FileSize] = 0;
			}
		}
	}
#endif

	{
		FScopeLock Lock(&GCompiledShaderCacheCS);

		TSharedPtr<FOpenGLCompiledShaderValue> FoundShader = GetOpenGLCompiledShaderCache().FindRef(ShaderCodeKey);
		if (!FoundShader)
		{
			FAnsiCharArray Finalglsl;
			GLSLToPlatform(Header, TypeEnum, GlslCodeOriginal, Finalglsl);
			GetOpenGLCompiledShaderCache().Add(ShaderCodeKey, MakeShareable(new FOpenGLCompiledShaderValue(0, Finalglsl)));
		}
		// With debug shaders we insert a shader name into the source and that can make it unique failing CRC check
#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT) && !DEBUG_GL_SHADERS 
		else
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_GLCheckShaderCodeCRC);
			FAnsiCharArray Finalglsl;
			GLSLToPlatform(Header, TypeEnum, GlslCodeOriginal, Finalglsl);
			TArray<ANSICHAR> FoundShaderCode = FoundShader->GetUncompressedShader();
			if (FoundShaderCode.Num() != Finalglsl.Num()
				|| FMemory::Memcmp(FoundShaderCode.GetData(), Finalglsl.GetData(), FoundShaderCode.Num())
				)
			{
				UE_LOG(LogRHI, Fatal, TEXT("SHADER CRC CLASH!"));
			}
		}
#endif
	}

	RHIType* RHIShader = nullptr;
	if (ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
	{
		RHIShader = new TOGLProxyType(ShaderCodeKey, [&](RHIType* OwnerRHI)
		{
			return CompileOpenGLShader<typename TOGLProxyType::ContainedGLType>(Header, ShaderCodeKey, Hash, OwnerRHI);
		});
	}
	else
	{
		// take a copy of the code for RHIT version.
		RHIShader = new TOGLProxyType(ShaderCodeKey, [Header = Header, ShaderCodeKey= ShaderCodeKey, Hash](RHIType* OwnerRHI)
		{
			return CompileOpenGLShader<typename TOGLProxyType::ContainedGLType>(Header, ShaderCodeKey, Hash, OwnerRHI);
		});
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (RHIShader)
	{
		RHIShader->ShaderName = ShaderCode.FindOptionalData(FShaderCodeName::Key);
	}
#endif

	return RHIShader;
}

FVertexShaderRHIRef FOpenGLDynamicRHI::RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateProxyShader<FRHIVertexShader>(Code, Hash);
}

FPixelShaderRHIRef FOpenGLDynamicRHI::RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateProxyShader<FRHIPixelShader>(Code, Hash);
}

FGeometryShaderRHIRef FOpenGLDynamicRHI::RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateProxyShader<FRHIGeometryShader>(Code, Hash);
}

static void MarkShaderParameterCachesDirty(FOpenGLShaderParameterCache* ShaderParameters, bool UpdateCompute)
{
	VERIFY_GL_SCOPE();
	const int32 StageStart = UpdateCompute  ? CrossCompiler::SHADER_STAGE_COMPUTE : CrossCompiler::SHADER_STAGE_VERTEX;
	const int32 StageEnd = UpdateCompute ? CrossCompiler::NUM_SHADER_STAGES : CrossCompiler::NUM_NON_COMPUTE_SHADER_STAGES;
	for (int32 Stage = StageStart; Stage < StageEnd; ++Stage)
	{
		ShaderParameters[Stage].MarkAllDirty();
	}
}

void FOpenGLDynamicRHI::BindUniformBufferBase(FOpenGLContextState& ContextState, int32 NumUniformBuffers, FUniformBufferRHIRef* BoundUniformBuffers, uint32 FirstUniformBuffer, bool ForceUpdate)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLUniformBindTime);
	VERIFY_GL_SCOPE();
	checkSlow(IsInRenderingThread() || IsInRHIThread());

	for (int32 BufferIndex = 0; BufferIndex < NumUniformBuffers; ++BufferIndex)
	{
		GLuint Buffer = 0;
		uint32 Offset = 0;
		uint32 Size = ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE;
		int32 BindIndex = FirstUniformBuffer + BufferIndex;

		if (IsValidRef(BoundUniformBuffers[BufferIndex]))
		{
			FRHIUniformBuffer* UB = BoundUniformBuffers[BufferIndex].GetReference();
			FOpenGLUniformBuffer* GLUB = ((FOpenGLUniformBuffer*)UB);
			Buffer = GLUB->Resource;

			if (GLUB->bIsEmulatedUniformBuffer)
			{
				continue;
			}

			Size = GLUB->GetSize();
#if SUBALLOCATED_CONSTANT_BUFFER
			Offset = GLUB->Offset;
#endif
		}
		else
		{
			if (PendingState.ZeroFilledDummyUniformBuffer == 0)
			{
				void* ZeroBuffer = FMemory::Malloc(ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
				FMemory::Memzero(ZeroBuffer,ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
				FOpenGL::GenBuffers(1, &PendingState.ZeroFilledDummyUniformBuffer);
				check(PendingState.ZeroFilledDummyUniformBuffer != 0);
				CachedBindUniformBuffer(ContextState,PendingState.ZeroFilledDummyUniformBuffer);
				glBufferData(GL_UNIFORM_BUFFER, ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE, ZeroBuffer, GL_STATIC_DRAW);
				FMemory::Free(ZeroBuffer);
				IncrementBufferMemory(GL_UNIFORM_BUFFER, ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
			}

			Buffer = PendingState.ZeroFilledDummyUniformBuffer;
		}

		if (ForceUpdate || (Buffer != 0 && ContextState.UniformBuffers[BindIndex] != Buffer)|| ContextState.UniformBufferOffsets[BindIndex] != Offset)
		{
			FOpenGL::BindBufferRange(GL_UNIFORM_BUFFER, BindIndex, Buffer, Offset, Size);
			ContextState.UniformBuffers[BindIndex] = Buffer;
			ContextState.UniformBufferOffsets[BindIndex] = Offset;
			ContextState.UniformBufferBound = Buffer;	// yes, calling glBindBufferRange also changes uniform buffer binding.
		}
	}
}

// ============================================================================================================================

struct FOpenGLUniformName
{
	FOpenGLUniformName()
	{
		FMemory::Memzero(Buffer);
	}
	
	ANSICHAR Buffer[10];
	
	friend bool operator ==(const FOpenGLUniformName& A,const FOpenGLUniformName& B)
	{
		return FMemory::Memcmp(A.Buffer, B.Buffer, sizeof(A.Buffer)) == 0;
	}
	
	friend uint32 GetTypeHash(const FOpenGLUniformName &Key)
	{
		return FCrc::MemCrc32(Key.Buffer, sizeof(Key.Buffer));
	}
};

static TMap<GLuint, TMap<FOpenGLUniformName, int64>>& GetOpenGLUniformBlockLocations()
{
	static TMap<GLuint, TMap<FOpenGLUniformName, int64>> UniformBlockLocations;
	return UniformBlockLocations;
}

static TMap<GLuint, TMap<int64, int64>>& GetOpenGLUniformBlockBindings()
{
	static TMap<GLuint, TMap<int64, int64>> UniformBlockBindings;
	return UniformBlockBindings;
}

static GLuint GetOpenGLProgramUniformBlockIndex(GLuint Program, const FOpenGLUniformName& UniformBlockName)
{
	TMap<FOpenGLUniformName, int64>& Locations = GetOpenGLUniformBlockLocations().FindOrAdd(Program);
	int64* Location = Locations.Find(UniformBlockName);
	if(Location)
	{
		return *Location;
	}
	else
	{
		int64& Loc = Locations.Emplace(UniformBlockName);
		Loc = (int64)FOpenGL::GetUniformBlockIndex(Program, UniformBlockName.Buffer);
		return Loc;
	}
}

static void GetOpenGLProgramUniformBlockBinding(GLuint Program, GLuint UniformBlockIndex, GLuint UniformBlockBinding)
{
	TMap<int64, int64>& Bindings = GetOpenGLUniformBlockBindings().FindOrAdd(Program);
	int64* Bind = static_cast<int64 *>(Bindings.Find(UniformBlockIndex));
	if(!Bind)
	{
		Bind = &(Bindings.Emplace(UniformBlockIndex));
		check(Bind);
		*Bind = -1;
	}
	check(Bind);
	if(*Bind != static_cast<int64>(UniformBlockBinding))
	{
		*Bind = static_cast<int64>(UniformBlockBinding);
		FOpenGL::UniformBlockBinding(Program, UniformBlockIndex, UniformBlockBinding);
	}
}

// ============================================================================================================================

int32 GEvictOnBSSDestructLatency = 0;
static FAutoConsoleVariableRef CVarEvictOnBssDestructLatency(
	TEXT("r.OpenGL.EvictOnBSSDestruct.Latency"),
	GEvictOnBSSDestructLatency,
	TEXT(""),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

class FOpenGLLinkedProgram
{
public:
	FOpenGLLinkedProgramConfiguration Config;

	struct FPackedUniformInfo
	{
		GLint	Location;
		uint8	ArrayType;	// OGL_PACKED_ARRAYINDEX_TYPE
		uint8	Index;		// OGL_PACKED_INDEX_TYPE
	};

	// Holds information needed per stage regarding packed uniform globals and uniform buffers
	struct FStagePackedUniformInfo
	{
		// Packed Uniform Arrays (regular globals); array elements per precision/type
		TArray<FPackedUniformInfo>			PackedUniformInfos;

		// Packed Uniform Buffers; outer array is per Uniform Buffer; inner array is per precision/type
		TArray<TArray<FPackedUniformInfo>>	PackedUniformBufferInfos;

		// Holds the unique ID of the last uniform buffer uploaded to the program; since we don't reuse uniform buffers
		// (can't modify existing ones), we use this as a check for dirty/need to mem copy on Mobile
		TArray<uint32>						LastEmulatedUniformBufferSet;
	};
	FStagePackedUniformInfo	StagePackedUniformInfo[CrossCompiler::NUM_SHADER_STAGES];

	GLuint		Program;
	bool		bDrawn;
	bool		bConfigIsInitalized;

	int32		MaxTextureStage;
	TBitArray<>	TextureStageNeeds;
	int32		MaxUAVUnitUsed;
	TBitArray<>	UAVStageNeeds;

	TArray<FOpenGLBindlessSamplerInfo> Samplers;

	// TODO: This should be stored within the lru.
	class FLRUInfo
	{
	public:
		FLRUInfo() : EvictBucket(-2) {}
		// ID to LRU (if used) allows quick access when updating LRU status.
		FSetElementId LRUNode;
		// cached binary used to create this program.
		TArray<uint8> CachedProgramBinary;

		// < 0 if not pending eviction. Bucket index if pending eviction.
		int32 EvictBucket;
	} LRUInfo;

private:
	FOpenGLLinkedProgram()
	: Program(0), bDrawn(false), bConfigIsInitalized(false), MaxTextureStage(-1), MaxUAVUnitUsed(-1)
	{
		TextureStageNeeds.Init( false, FOpenGL::GetMaxCombinedTextureImageUnits() );
		UAVStageNeeds.Init( false, FOpenGL::GetMaxCombinedUAVUnits() );
	}

public:

	FOpenGLLinkedProgram(const FOpenGLProgramKey& ProgramKeyIn)
		: FOpenGLLinkedProgram()
	{
		Config.ProgramKey = ProgramKeyIn;
	}

	FOpenGLLinkedProgram(const FOpenGLProgramKey& ProgramKeyIn, GLuint ProgramIn)
		: FOpenGLLinkedProgram()
	{
		// Add a program without a valid config. (partially initialized)
		Program = ProgramIn;

		// The key is required as the program could be evicted before being bound (set bss).
		Config.ProgramKey = ProgramKeyIn;
	}

	FOpenGLLinkedProgram(const FOpenGLLinkedProgramConfiguration& ConfigIn, GLuint ProgramIn)
		: FOpenGLLinkedProgram()
	{
		SetConfig(ConfigIn);
		Program = ProgramIn;
	}

	~FOpenGLLinkedProgram()
	{
		DeleteGLResources();
	}

	void DeleteGLResources()
	{
		VERIFY_GL_SCOPE();
		SetDeletedProgramStats(Program);
		FOpenGL::DeleteProgramPipelines(1, &Program);

		GetOpenGLUniformBlockLocations().Remove(Program);
		GetOpenGLUniformBlockBindings().Remove(Program);

		Program = 0;

		for (int Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			StagePackedUniformInfo[Stage].PackedUniformInfos.Empty();
			StagePackedUniformInfo[Stage].PackedUniformBufferInfos.Empty();
			StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet.Empty();
		}
	}

	void ConfigureShaderStage( int Stage, uint32 FirstUniformBuffer );

	// Make sure GlobalArrays (created from shader reflection) matches our info (from the cross compiler)
	static inline void SortPackedUniformInfos(const TArray<FPackedUniformInfo>& ReflectedUniformInfos, const TArray<CrossCompiler::FPackedArrayInfo>& PackedGlobalArrays, TArray<FPackedUniformInfo>& OutPackedUniformInfos)
	{
		check(OutPackedUniformInfos.Num() == 0);
		OutPackedUniformInfos.Empty(PackedGlobalArrays.Num());
		for (int32 Index = 0; Index < PackedGlobalArrays.Num(); ++Index)
		{
			auto& PackedArray = PackedGlobalArrays[Index];
			FPackedUniformInfo OutInfo = {-1, PackedArray.TypeName, CrossCompiler::PACKED_TYPEINDEX_MAX};

			// Find this Global Array in the reflection list
			for (int32 FindIndex = 0; FindIndex < ReflectedUniformInfos.Num(); ++FindIndex)
			{
				auto& ReflectedInfo = ReflectedUniformInfos[FindIndex];
				if (ReflectedInfo.ArrayType == PackedArray.TypeName)
				{
					OutInfo = ReflectedInfo;
					break;
				}
			}

			OutPackedUniformInfos.Add(OutInfo);
		}
	}

	void SetConfig(const FOpenGLLinkedProgramConfiguration& ConfigIn)
	{
		Config = ConfigIn;
		bConfigIsInitalized = true;
	}
};

static bool bMeasureEviction = false;
class FDelayedEvictionContainer
{
public:

	FDelayedEvictionContainer()
	{
		Init();
	}

	void Add(FOpenGLLinkedProgram* LinkedProgram);

	FORCEINLINE_DEBUGGABLE static void OnProgramTouched(FOpenGLLinkedProgram* LinkedProgram)
	{
		if(LinkedProgram->LRUInfo.EvictBucket >=0 )
		{
			FDelayedEvictionContainer::Get().Remove(LinkedProgram);
			INC_DWORD_STAT(STAT_OpenGLShaderLRUEvictionDelaySavedCount);
		}
	}

	void Tick();

	void Init();

	void Remove(FOpenGLLinkedProgram* RemoveMe);

	FORCEINLINE_DEBUGGABLE static FDelayedEvictionContainer & Get()
	{
		static FDelayedEvictionContainer DelayedEvictionContainer;
		return DelayedEvictionContainer;
	}
private:
	class FDelayEvictBucket
	{
	public:
		FDelayEvictBucket() : NumToFreePerTick(0) {}
		int32 NumToFreePerTick;
		TSet<FOpenGLLinkedProgram*> ProgramsToEvict;
	};

	TArray<FDelayEvictBucket> Buckets;

	int32 TotalBuckets;
	int32 TimePerBucket;
	int32 CurrentBucketTickCount;
	int32 NewProgramBucket;
	int32 EvictBucketIndex;
};

static void ConfigureStageStates(FOpenGLLinkedProgram* LinkedProgram)
{
	const FOpenGLLinkedProgramConfiguration &Config = LinkedProgram->Config;

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_VERTEX,
			OGL_FIRST_UNIFORM_BUFFER
		);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_VERTEX].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_PIXEL,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers
		);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_PIXEL].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_GEOMETRY,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.NumUniformBuffers
		);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_GEOMETRY].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_COMPUTE,
			OGL_FIRST_UNIFORM_BUFFER
		);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_COMPUTE].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Bindings.PackedGlobalArrays.Num());
	}
}

namespace UE
{
	namespace OpenGL
	{
        static bool CreateGLProgramFromUncompressedBinary(GLuint& ProgramOUT, const TArray<uint8>& ProgramBinary)
        {
	        VERIFY_GL_SCOPE();
	        GLuint GLProgramName = 0;
	        FOpenGL::GenProgramPipelines(1, &GLProgramName);
	        int32 BinarySize = ProgramBinary.Num();
	        //UE_LOG(LogRHI, Log, TEXT("CreateGLProgramFromBinary : gen program %x, size: %d"), GLProgramName, BinarySize);
        
	        check(BinarySize);
        
	        const uint8* ProgramBinaryPtr = ProgramBinary.GetData();
        
	        // BinaryFormat is stored at the start of ProgramBinary array
	        FOpenGL::ProgramBinary(GLProgramName, ((GLenum*)ProgramBinaryPtr)[0], ProgramBinaryPtr + sizeof(GLenum), BinarySize - sizeof(GLenum));
	        //	UE_LOG(LogRHI, Warning, TEXT("LRU: CreateFromBinary %d, binary format: %x, BinSize: %d"), GLProgramName, ((GLenum*)ProgramBinaryPtr)[0], BinarySize - sizeof(GLenum));
        
	        ProgramOUT = GLProgramName;
	        return VerifyLinkedProgram(GLProgramName);
        }

        static bool CreateGLProgramFromCompressedBinary(GLuint& ProgramOUT, const TArray<uint8>& CompressedProgramBinary)
        {
	        TArray<uint8> UncompressedProgramBinary;
        
	        bool bDecompressSuccess;
        
	        {
		        QUICK_SCOPE_CYCLE_COUNTER(STAT_DecompressProgramBinary);
		        bDecompressSuccess = UE::OpenGL::UncompressCompressedBinaryProgram(CompressedProgramBinary, UncompressedProgramBinary);
	        }
        
	        if(bDecompressSuccess)
	        {
		        QUICK_SCOPE_CYCLE_COUNTER(STAT_CreateProgramFromBinary);
		        return CreateGLProgramFromUncompressedBinary(ProgramOUT, UncompressedProgramBinary);
	        }
	        return false;
        }

		bool CreateGLProgramFromBinary(GLuint& ProgramOUT, const TArray<uint8>& ProgramBinary)
		{
			SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateProgramFromBinaryTime)
			bool bSuccess;
			if (UE::OpenGL::IsStoringCompressedBinaryPrograms())
			{
				bSuccess = CreateGLProgramFromCompressedBinary(ProgramOUT, ProgramBinary);
			}
			else
			{
				bSuccess = CreateGLProgramFromUncompressedBinary(ProgramOUT, ProgramBinary);
			}

			if( bSuccess )
			{
				SetNewProgramStats(ProgramOUT);
			}

			return bSuccess;
		}

        static void CompilePendingShaders(const FOpenGLLinkedProgramConfiguration& Config)
        {
	        VERIFY_GL_SCOPE();
        
	        // Find the existing compiled shader in the cache.
	        FScopeLock Lock(&GCompiledShaderCacheCS);
	        for (int32 StageIdx = 0; StageIdx < UE_ARRAY_COUNT(Config.Shaders); ++StageIdx)
	        {
		        TSharedPtr<FOpenGLCompiledShaderValue> FoundShader = GetOpenGLCompiledShaderCache().FindRef(Config.Shaders[StageIdx].ShaderKey);
		        if (FoundShader && FoundShader->bHasCompiled == false)
		        {
			        TArray<ANSICHAR> GlslCode = FoundShader->GetUncompressedShader();
			        CompileCurrentShader(Config.Shaders[StageIdx].Resource, GlslCode);
			        FoundShader->bHasCompiled = true;
		        }
	        }
        }

	}
}

static int32 GetProgramBinarySize(GLuint Program)
{
	GLint BinaryLength = -1;
	glGetProgramiv(Program, GL_PROGRAM_BINARY_LENGTH, &BinaryLength);
	check(BinaryLength > 0);
	return BinaryLength;
}

void ConfigureGLProgramStageStates(FOpenGLLinkedProgram* LinkedProgram)
{
	ensure(VerifyLinkedProgram(LinkedProgram->Program));
	FOpenGL::BindProgramPipeline(LinkedProgram->Program);
	ConfigureStageStates(LinkedProgram);
}

class FGLProgramCacheLRU
{
	class FEvictedGLProgram
	{
		FOpenGLLinkedProgram* LinkedProgram;
 
		FORCEINLINE_DEBUGGABLE TArray<uint8>& GetProgramBinary()
 		{
			return LinkedProgram->LRUInfo.CachedProgramBinary;
 		}

	public:

		// Create an evicted program with the program binary provided.
		FEvictedGLProgram(const FOpenGLProgramKey& ProgramKey, TArray<uint8>&& ProgramBinaryIn)
		{
			LinkedProgram = new FOpenGLLinkedProgram(ProgramKey);

			GetProgramBinary() = MoveTemp(ProgramBinaryIn);

			INC_MEMORY_STAT_BY(STAT_OpenGLShaderLRUProgramMemory, GetProgramBinary().Num());
		}

		FEvictedGLProgram(FOpenGLLinkedProgram* InLinkedProgram)
			: LinkedProgram(InLinkedProgram)
		{
			bool bCreateProgramBinary = CVarLRUKeepProgramBinaryResident.GetValueOnAnyThread() == 0 || LinkedProgram->LRUInfo.CachedProgramBinary.Num() == 0;

			if( bCreateProgramBinary )
			{
			// build offline binary:
				UE::OpenGL::GetProgramBinaryFromGLProgram(LinkedProgram->Program, GetProgramBinary());
				INC_MEMORY_STAT_BY(STAT_OpenGLShaderLRUProgramMemory, GetProgramBinary().Num());
			}

			if(bMeasureEviction)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_EvictFromLRU_DeleteGLResource);
				// Remove existing GL program and associated data.
				LinkedProgram->DeleteGLResources();
			}
			else
			{
				LinkedProgram->DeleteGLResources();
			}
		}

		void RestoreGLProgramFromBinary()
		{
			check(LinkedProgram->Program == 0);
			bool bSuccess = UE::OpenGL::CreateGLProgramFromBinary(LinkedProgram->Program, GetProgramBinary());
			if(bSuccess)
			{
				if(CVarLRUKeepProgramBinaryResident.GetValueOnAnyThread() == 0)
				{
					DEC_MEMORY_STAT_BY(STAT_OpenGLShaderLRUProgramMemory, GetProgramBinary().Num());
					GetProgramBinary().Empty();
				}
			}
			else
			{
				uint32 ProgramCRC = FCrc::MemCrc32(GetProgramBinary().GetData(), GetProgramBinary().Num());
				UE_LOG(LogRHI, Log, TEXT("[%s, %d, %d, crc 0x%X]"), *LinkedProgram->Config.ProgramKey.ToString(), LinkedProgram->Program, GetProgramBinary().Num(), ProgramCRC );
				// dump first 32 bytes..
				if (GetProgramBinary().Num() >= 32)
				{
					const uint32* MemPtr = (const uint32*)GetProgramBinary().GetData();
					for (int32 Dump = 0; Dump < 8; Dump++)
					{
						UE_LOG(LogRHI, Log, TEXT("[%d :  0x%08X]"), Dump, *MemPtr++);
					}
				}
				RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramCreate"));
				UE_LOG(LogRHI, Fatal, TEXT("RestoreGLProgramFromBinary : Failed to restore GL program from binary data! [%s]"), *LinkedProgram->Config.ProgramKey.ToString());
			}
		}

		FOpenGLLinkedProgram* GetLinkedProgram()
		{
			return LinkedProgram;
		}
	};
	typedef TMap<FOpenGLProgramKey, FEvictedGLProgram> FOpenGLEvictedProgramsMap;
	typedef TPsoLruCache<FOpenGLProgramKey, FOpenGLLinkedProgram* > FOpenGLProgramLRUCache;
	const int LRUCapacity = 2048;
	int32 LRUBinaryMemoryUse;

	// Find linked program within the evicted container.
	// no attempt to promote to LRU or create the GL object is made.
	FOpenGLLinkedProgram* FindEvicted(const FOpenGLProgramKey& ProgramKey)
	{
		FEvictedGLProgram* FoundEvicted = EvictedPrograms.Find(ProgramKey);
		if (FoundEvicted)
		{
			FOpenGLLinkedProgram* LinkedProgram = FoundEvicted->GetLinkedProgram();
			return LinkedProgram;
		}
		return nullptr;
	}

	FOpenGLLinkedProgram* FindEvictedAndUpdateLRU(const FOpenGLProgramKey& ProgramKey)
	{
		// Missed LRU cache, check evicted cache and add back to LRU
		FEvictedGLProgram* FoundEvicted = EvictedPrograms.Find(ProgramKey);
		if (FoundEvicted)
		{
			SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLRUMissTime);
			INC_DWORD_STAT(STAT_OpenGLShaderLRUMissCount);

			// UE_LOG(LogRHI, Warning, TEXT("LRU: found and recovered EVICTED program %s"), *ProgramKey.ToString());
			FoundEvicted->RestoreGLProgramFromBinary();
			FOpenGLLinkedProgram* LinkedProgram = FoundEvicted->GetLinkedProgram();

			// Remove from the evicted program map.
			EvictedPrograms.Remove(ProgramKey);

			// Add this back to the LRU
			Add(ProgramKey, LinkedProgram);

			DEC_DWORD_STAT(STAT_OpenGLShaderLRUEvictedProgramCount);

			// reconfigure the new program:
			ConfigureGLProgramStageStates(LinkedProgram);

			SetNewProgramStats(LinkedProgram->Program);

			return LinkedProgram;
		}

		// nope.
		return nullptr;
	}

	void EvictFromLRU(FOpenGLLinkedProgram* LinkedProgram)
	{
		SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLRUEvictTime);
		LinkedProgram->LRUInfo.LRUNode = FSetElementId();

		if (LinkedProgram->LRUInfo.EvictBucket >= 0)
		{
			// remove it from the delayed eviction container since we're evicting now.
			FDelayedEvictionContainer::Get().Remove(LinkedProgram);
		}

		DEC_DWORD_STAT(STAT_OpenGLShaderLRUProgramCount);

		if (bMeasureEviction)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT__EvictFromLRU_GetBinarySize);
			LRUBinaryMemoryUse -= GetProgramBinarySize(LinkedProgram->Program);
		}
		else
		{
			LRUBinaryMemoryUse -= GetProgramBinarySize(LinkedProgram->Program);
		}

		checkf(!EvictedPrograms.Contains(LinkedProgram->Config.ProgramKey), TEXT("Program is already in the evicted program list: %s"), *LinkedProgram->Config.ProgramKey.ToString());
		//UE_LOG(LogRHI, Warning, TEXT("LRU: Evicting program %d"), LinkedProgram->Program);
		FEvictedGLProgram& test = EvictedPrograms.Emplace(LinkedProgram->Config.ProgramKey, FEvictedGLProgram(LinkedProgram));
		INC_DWORD_STAT(STAT_OpenGLShaderLRUEvictedProgramCount);
	}

public:

	bool IsEvicted(const FOpenGLProgramKey& ProgramKey)
	{
		return FindEvicted(ProgramKey) != nullptr;
	}

	void EvictLeastRecentFromLRU()
	{
		EvictFromLRU(LRU.RemoveLeastRecent());
	}

	void EvictMostRecentFromLRU()
	{
		EvictFromLRU(LRU.RemoveMostRecent());
	}

	void EvictProgramFromLRU(const FOpenGLProgramKey& ProgramKey)
	{
		FOpenGLLinkedProgram* RemovedLinkedProgram = nullptr;
		if(LRU.Remove(ProgramKey, RemovedLinkedProgram))
		{
			INC_DWORD_STAT(STAT_OpenGLShaderLRUScopeEvictedProgramCount);
			EvictFromLRU(RemovedLinkedProgram);
		}
	}

	FGLProgramCacheLRU() : LRUBinaryMemoryUse(0), LRU(LRUCapacity)
	{
	}

	bool IsLRUAtCapacity() const
	{
		return LRU.Num() == CVarLRUMaxProgramCount.GetValueOnAnyThread() || LRU.Num() == LRU.Max() || LRUBinaryMemoryUse > CVarLRUMaxProgramBinarySize.GetValueOnAnyThread();
	}

	void Add(const FOpenGLProgramKey& ProgramKey, FOpenGLLinkedProgram* LinkedProgram)
	{
		// Remove least recently used programs until we reach our limit.
		// note that a single large shader could evict multiple smaller shaders.
		checkf(!LRU.Contains(ProgramKey), TEXT("Program is already in the LRU program list: %s"), *ProgramKey.ToString());
		checkf(!IsEvicted(ProgramKey), TEXT("Program is already in the evicted program list: %s"), *ProgramKey.ToString());

		// UE_LOG(LogRHI, Warning, TEXT("LRU: adding program %s (%d)"), *ProgramKey.ToString(), LinkedProgram->Program);

		while (IsLRUAtCapacity())
		{
			EvictLeastRecentFromLRU();
		}

		LinkedProgram->LRUInfo.LRUNode = LRU.Add(ProgramKey, LinkedProgram);
		FDelayedEvictionContainer::OnProgramTouched(LinkedProgram);
		LRUBinaryMemoryUse += GetProgramBinarySize(LinkedProgram->Program);
		INC_DWORD_STAT(STAT_OpenGLShaderLRUProgramCount);
	}

	void AddAsEvicted(const FOpenGLProgramKey& ProgramKey, TArray<uint8>&& ProgramBinary)
	{
		checkf(!LRU.Contains(ProgramKey), TEXT("Program is already in the LRU program list: %s"), *ProgramKey.ToString());
		checkf(!IsEvicted(ProgramKey), TEXT("Program is already in the evicted program list: %s"), *ProgramKey.ToString());

		FEvictedGLProgram& test = EvictedPrograms.Emplace(ProgramKey, FEvictedGLProgram(ProgramKey, MoveTemp(ProgramBinary)));

		// UE_LOG(LogRHI, Warning, TEXT("LRU: adding EVICTED program %s"), *ProgramKey.ToString());

		INC_DWORD_STAT(STAT_OpenGLShaderLRUEvictedProgramCount);
	}

	FOpenGLLinkedProgram* Find(const FOpenGLProgramKey& ProgramKey, bool bFindAndCreateEvictedProgram)
	{
		// if it's in LRU pop to top.
		FOpenGLLinkedProgram *const * Found = LRU.FindAndTouch(ProgramKey);
		if (Found)
		{
			check((*Found)->LRUInfo.LRUNode.IsValidId());
			//UE_LOG(LogRHI, Warning, TEXT("LRU: ::Find program %d exists in LRU!"), (*Found)->Program);
			return *Found;
		}

		if( bFindAndCreateEvictedProgram )
		{
			return FindEvictedAndUpdateLRU(ProgramKey);
		}
		else
		{
			return FindEvicted(ProgramKey);
		}
	}

	FORCEINLINE_DEBUGGABLE void Touch(FOpenGLLinkedProgram* LinkedProgram)
	{
		if(LinkedProgram->LRUInfo.LRUNode.IsValidId())
		{
			LRU.MarkAsRecent(LinkedProgram->LRUInfo.LRUNode);
		}
		else
		{
			// This must find the program.
			ensure(FindEvictedAndUpdateLRU(LinkedProgram->Config.ProgramKey));
		}
		FDelayedEvictionContainer::OnProgramTouched(LinkedProgram);
	}

	void Empty()
	{
		// delete all FOpenGLLinkedPrograms from evicted container
		for(FOpenGLEvictedProgramsMap::TIterator It(EvictedPrograms); It; ++It )
		{
			FOpenGLLinkedProgram* LinkedProgram = It.Value().GetLinkedProgram();
			delete LinkedProgram;
		}
		EvictedPrograms.Empty();

		// delete all FOpenGLLinkedPrograms from LRU
		for (FOpenGLProgramLRUCache::TIterator It(LRU); It; ++It)
		{
			delete It.Value();
		}
		LRU.Empty(LRUCapacity);
	}

	void EnumerateLinkedPrograms(TFunction<void(FOpenGLLinkedProgram*)> EnumFunc)
	{
		// delete all FOpenGLLinkedPrograms from evicted container
		for (FOpenGLEvictedProgramsMap::TIterator It(EvictedPrograms); It; ++It)
		{
			EnumFunc( It.Value().GetLinkedProgram());
		}
		// delete all FOpenGLLinkedPrograms from LRU
		for (FOpenGLProgramLRUCache::TIterator It(LRU); It; ++It)
		{
			EnumFunc(It.Value());
		}
	}

	FOpenGLProgramLRUCache LRU;
	FOpenGLEvictedProgramsMap EvictedPrograms;
};

typedef TMap<FOpenGLProgramKey, FOpenGLLinkedProgram*> FOpenGLProgramsMap;

// FGLProgramCache is a K/V store that holds on to all FOpenGLLinkedProgram created.
// It is implemented by either a TMap or an LRU cache that will limit the number of active GL programs at any one time.
// (LRU is used only to work around the mali driver's maximum shader heap size.)
class FGLProgramCache
{
	FGLProgramCacheLRU ProgramCacheLRU;
	FOpenGLProgramsMap ProgramCache;
	bool bUseLRUCache;
public:
	FGLProgramCache()
	{
		if(CVarEnableLRU.GetValueOnAnyThread() && !FOpenGL::SupportsProgramBinary())
		{
			UE_LOG(LogRHI, Warning, TEXT("Requesting OpenGL program LRU cache, but program binary is not supported by driver. Falling back to non-lru cache."));
		}

		bUseLRUCache = CVarEnableLRU.GetValueOnAnyThread() == 1 && FOpenGL::SupportsProgramBinary();
		UE_LOG(LogRHI, Log, TEXT("Using OpenGL program LRU cache: %d"), bUseLRUCache ? 1 : 0);
	}

	FORCEINLINE_DEBUGGABLE bool IsUsingLRU() const
	{
		return bUseLRUCache;
	}

	FORCEINLINE_DEBUGGABLE void Touch(FOpenGLLinkedProgram* LinkedProgram)
	{
		if (bUseLRUCache)
		{
			ProgramCacheLRU.Touch(LinkedProgram);
		}
	}

	FORCEINLINE_DEBUGGABLE FOpenGLLinkedProgram* Find(const FOpenGLProgramKey& ProgramKey, bool bFindAndCreateEvictedProgram)
	{
		if (bUseLRUCache)
		{
			return ProgramCacheLRU.Find(ProgramKey, bFindAndCreateEvictedProgram);
		}
		else
		{
			FOpenGLLinkedProgram** FoundProgram = ProgramCache.Find(ProgramKey);
			return FoundProgram ? *FoundProgram : nullptr;
		}
	}

	FORCEINLINE_DEBUGGABLE void Add(const FOpenGLProgramKey& ProgramKey, FOpenGLLinkedProgram* LinkedProgram)
	{
		if (bUseLRUCache)
		{
			ProgramCacheLRU.Add(ProgramKey, LinkedProgram);
		}
		else
		{
			check(!ProgramCache.Contains(ProgramKey));
			ProgramCache.Add(ProgramKey, LinkedProgram);
		}
	}

	void Empty()
	{
		if (bUseLRUCache)
		{
			ProgramCacheLRU.Empty();
		}
		else
		{
			// delete all FOpenGLLinkedPrograms from ProgramCache
			for (FOpenGLProgramsMap::TIterator It(ProgramCache); It; ++It)
			{
				delete It.Value();
			}
			ProgramCache.Empty();
		}
	}

	bool IsLRUAtCapacity() const
	{
		if (bUseLRUCache)
		{
			ProgramCacheLRU.IsLRUAtCapacity();
		}

		return false;
	}

	void EvictMostRecent()
	{
		check(IsUsingLRU());
		if( ProgramCacheLRU.LRU.Num() )
		{
			ProgramCacheLRU.EvictMostRecentFromLRU();
		}
	}

	void EvictProgram(const FOpenGLProgramKey& ProgramKey)
	{
		check(IsUsingLRU());
		ProgramCacheLRU.EvictProgramFromLRU(ProgramKey);
	}

	void AddAsEvicted(const FOpenGLProgramKey& ProgramKey, TArray<uint8>&& ProgramBinary)
	{
		check(IsUsingLRU());
		ProgramCacheLRU.AddAsEvicted(ProgramKey, MoveTemp(ProgramBinary));
	}

	bool IsEvicted(const FOpenGLProgramKey& ProgramKey)
	{
		check(IsUsingLRU());
		return ProgramCacheLRU.IsEvicted(ProgramKey);
	}

	void EnumerateLinkedPrograms(TFunction<void(FOpenGLLinkedProgram*)> EnumFunc)
	{
		if (bUseLRUCache)
		{
			ProgramCacheLRU.EnumerateLinkedPrograms(EnumFunc);
		}
		else
		{
			// all programs are retained in map.
			for (FOpenGLProgramsMap::TIterator It(ProgramCache); It; ++It)
			{
				EnumFunc(It.Value());
			}
		}
	}
};

static FGLProgramCache& GetOpenGLProgramsCache()
{
	static FGLProgramCache ProgramsCache;
	return ProgramsCache;
}


void FDelayedEvictionContainer::Init()
{
	const int32 EvictLatencyTicks = GEvictOnBSSDestructLatency;
	const int32 NumLatencyBuckets = 3;
	TotalBuckets = NumLatencyBuckets + 1;
	Buckets.SetNum(TotalBuckets);
	TimePerBucket = (EvictLatencyTicks)/(NumLatencyBuckets-1);
	CurrentBucketTickCount = TimePerBucket;
	NewProgramBucket = 0;
	EvictBucketIndex = 1;
}

void FDelayedEvictionContainer::Add(FOpenGLLinkedProgram* LinkedProgram)
{
	if (GEvictOnBSSDestructLatency == 0)
	{
		GetOpenGLProgramsCache().EvictProgram(LinkedProgram->Config.ProgramKey);
		return;
	}

	checkf(!GetOpenGLProgramsCache().IsEvicted(LinkedProgram->Config.ProgramKey), TEXT("FDelayedEvictionContainer::Add is already evicted! [%s], %d"), *LinkedProgram->Config.ProgramKey.ToString(), LinkedProgram->LRUInfo.EvictBucket);

	if (LinkedProgram->LRUInfo.EvictBucket >=0 )
	{
		Remove(LinkedProgram);
	}
	Buckets[NewProgramBucket].ProgramsToEvict.Add(LinkedProgram);
	LinkedProgram->LRUInfo.EvictBucket = NewProgramBucket;
}

void FDelayedEvictionContainer::Remove(FOpenGLLinkedProgram* RemoveMe)
{
	if (GEvictOnBSSDestructLatency == 0)
	{
		return;
	}
	check(RemoveMe->LRUInfo.EvictBucket >= 0);
	ensure( Buckets[RemoveMe->LRUInfo.EvictBucket].ProgramsToEvict.Remove(RemoveMe) == 1 );
	RemoveMe->LRUInfo.EvictBucket = -1;
}

void FDelayedEvictionContainer::Tick()
{
	if (GEvictOnBSSDestructLatency == 0)
	{
		return;
	}

	FDelayEvictBucket& EvictionBucket = Buckets[EvictBucketIndex];

	const int32 NumToFree = EvictionBucket.ProgramsToEvict.Num();
	if (NumToFree)
	{
		auto It = EvictionBucket.ProgramsToEvict.CreateIterator();
		for (int32 i= FMath::Min(EvictionBucket.NumToFreePerTick, NumToFree)-1;i>=0;i--)
		{
			FOpenGLLinkedProgram* LinkedProgram = *It;
			It.RemoveCurrent();
			++It;
			bMeasureEviction = true;
			check(LinkedProgram->LRUInfo.EvictBucket == EvictBucketIndex);
			LinkedProgram->LRUInfo.EvictBucket = -3; // Mark EvictBucket to indicated evicted from ProgramsToEvict, Prevent EvictProgram from attempting to remove again.
			GetOpenGLProgramsCache().EvictProgram(LinkedProgram->Config.ProgramKey);
			bMeasureEviction = false;
		}
	}

	if (--CurrentBucketTickCount == 0)
	{
		check(EvictionBucket.ProgramsToEvict.Num() == 0);
		EvictBucketIndex = (EvictBucketIndex+1) % Buckets.Num();
		NewProgramBucket = (NewProgramBucket+1) % Buckets.Num();
		CurrentBucketTickCount = TimePerBucket;
		Buckets[EvictBucketIndex].NumToFreePerTick = (Buckets[EvictBucketIndex].ProgramsToEvict.Num() -1)/TimePerBucket + 1;
	}
}

// This short queue preceding released programs cache is here because usually the programs are requested again
// very shortly after they're released, so looking through recently released programs first provides tangible
// performance improvement.

#define LAST_RELEASED_PROGRAMS_CACHE_COUNT 10

static FOpenGLLinkedProgram* StaticLastReleasedPrograms[LAST_RELEASED_PROGRAMS_CACHE_COUNT] = { 0 };
static int32 StaticLastReleasedProgramsIndex = 0;

// ============================================================================================================================

static int32 CountSetBits(const TBitArray<>& Array)
{
	int32 Result = 0;
	for (TBitArray<>::FConstIterator BitIt(Array); BitIt; ++BitIt)
	{
		Result += BitIt.GetValue();
	}
	return Result;
}

void FOpenGLLinkedProgram::ConfigureShaderStage( int Stage, uint32 FirstUniformBuffer )
{
	static const GLint FirstTextureUnit[CrossCompiler::NUM_SHADER_STAGES] =
	{
		FOpenGL::GetFirstVertexTextureUnit(),
		FOpenGL::GetFirstPixelTextureUnit(),
		FOpenGL::GetFirstGeometryTextureUnit(),
		0,
		0,
		FOpenGL::GetFirstComputeTextureUnit()
	};

	static const GLint MaxTextureUnit[CrossCompiler::NUM_SHADER_STAGES] =
	{
		FOpenGL::GetMaxVertexTextureImageUnits(),
		FOpenGL::GetMaxTextureImageUnits(),
		FOpenGL::GetMaxGeometryTextureImageUnits(),
		0,
		0,
		FOpenGL::GetMaxComputeTextureImageUnits()
	};

	static const GLint FirstUAVUnit[CrossCompiler::NUM_SHADER_STAGES] =
	{
		FOpenGL::GetFirstVertexUAVUnit(),
		FOpenGL::GetFirstPixelUAVUnit(),
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		FOpenGL::GetFirstComputeUAVUnit()
	};
	
	// verify that only CS and PS uses UAVs
	check(!(Stage == CrossCompiler::SHADER_STAGE_COMPUTE || Stage == CrossCompiler::SHADER_STAGE_PIXEL) ? (CountSetBits(UAVStageNeeds) == 0) : true);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderBindParameterTime);
	VERIFY_GL_SCOPE();

	FOpenGLUniformName Name;
	Name.Buffer[0] = CrossCompiler::ShaderStageIndexToTypeName(Stage);

	GLuint StageProgram = Program;
	
	// Bind Global uniform arrays (vu_h, pu_i, etc)
	{
		Name.Buffer[1] = 'u';
		Name.Buffer[2] = '_';
		Name.Buffer[3] = 0;
		Name.Buffer[4] = 0;

		TArray<FPackedUniformInfo> PackedUniformInfos;
		for (uint8 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
		{
			uint8 ArrayIndexType = CrossCompiler::PackedTypeIndexToTypeName(Index);
			Name.Buffer[3] = ArrayIndexType;
			GLint Location = glGetUniformLocation(StageProgram, Name.Buffer);
			if ((int32)Location != -1)
			{
				FPackedUniformInfo Info = {Location, ArrayIndexType, Index};
				PackedUniformInfos.Add(Info);
			}
		}

		SortPackedUniformInfos(PackedUniformInfos, Config.Shaders[Stage].Bindings.PackedGlobalArrays, StagePackedUniformInfo[Stage].PackedUniformInfos);
	}

	// Bind uniform buffer packed arrays (vc0_h, pc2_i, etc)
	{
		Name.Buffer[1] = 'c';
		Name.Buffer[2] = 0;
		Name.Buffer[3] = 0;
		Name.Buffer[4] = 0;
		Name.Buffer[5] = 0;
		Name.Buffer[6] = 0;

		check(StagePackedUniformInfo[Stage].PackedUniformBufferInfos.Num() == 0);
		int32 NumUniformBuffers = Config.Shaders[Stage].Bindings.NumUniformBuffers;
		StagePackedUniformInfo[Stage].PackedUniformBufferInfos.SetNum(NumUniformBuffers);
		int32 NumPackedUniformBuffers = Config.Shaders[Stage].Bindings.PackedUniformBuffers.Num();
		check(NumPackedUniformBuffers <= NumUniformBuffers);

		for (int32 UB = 0; UB < NumPackedUniformBuffers; ++UB)
		{
			const TArray<CrossCompiler::FPackedArrayInfo>& PackedInfo = Config.Shaders[Stage].Bindings.PackedUniformBuffers[UB];
			TArray<FPackedUniformInfo>& PackedBuffers = StagePackedUniformInfo[Stage].PackedUniformBufferInfos[UB];

			ANSICHAR* Str = SetIndex(Name.Buffer, 2, UB);
			*Str++ = '_';
			Str[1] = 0;
			for (uint8 Index = 0; Index < PackedInfo.Num(); ++Index)
			{
				Str[0] = PackedInfo[Index].TypeName;
				GLint Location = glGetUniformLocation(StageProgram, Name.Buffer); // This could be -1 if optimized out
				FPackedUniformInfo Info = {Location, PackedInfo[Index].TypeName,  PackedInfo[Index].TypeIndex};
				PackedBuffers.Add(Info);
			}
		}
	}

	// Reserve and setup Space for Emulated Uniform Buffers
	StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet.Empty(Config.Shaders[Stage].Bindings.NumUniformBuffers);
	StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet.AddZeroed(Config.Shaders[Stage].Bindings.NumUniformBuffers);

	// Bind samplers.
	Name.Buffer[1] = 's';
	Name.Buffer[2] = 0;
	Name.Buffer[3] = 0;
	Name.Buffer[4] = 0;
	int32 LastFoundIndex = -1;
	for (int32 SamplerIndex = 0; SamplerIndex < Config.Shaders[Stage].Bindings.NumSamplers; ++SamplerIndex)
	{
		SetIndex(Name.Buffer, 2, SamplerIndex);
		GLint Location = glGetUniformLocation(StageProgram, Name.Buffer);
		if (Location == -1)
		{
			if (LastFoundIndex != -1)
			{
				// It may be an array of samplers. Get the initial element location, if available, and count from it.
				SetIndex(Name.Buffer, 2, LastFoundIndex);
				int32 OffsetOfArraySpecifier = (LastFoundIndex>9)?4:3;
				int32 ArrayIndex = SamplerIndex-LastFoundIndex;
				Name.Buffer[OffsetOfArraySpecifier] = '[';
				ANSICHAR* EndBracket = SetIndex(Name.Buffer, OffsetOfArraySpecifier+1, ArrayIndex);
				*EndBracket++ = ']';
				*EndBracket = 0;
				Location = glGetUniformLocation(StageProgram, Name.Buffer);
			}
		}
		else
		{
			LastFoundIndex = SamplerIndex;
		}

		if (Location != -1)
		{
			if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
			{
				// Non-bindless, setup the unit info
				FOpenGL::ProgramUniform1i(StageProgram, Location, FirstTextureUnit[Stage] + SamplerIndex);
				TextureStageNeeds[ FirstTextureUnit[Stage] + SamplerIndex ] = true;
				MaxTextureStage = FMath::Max( MaxTextureStage, FirstTextureUnit[Stage] + SamplerIndex);
				if (SamplerIndex >= MaxTextureUnit[Stage])
				{
					UE_LOG(LogShaders, Error, TEXT("%s has a shader using too many textures (idx %d, max allowed %d) at stage %d"), *Config.ProgramKey.ToString(), SamplerIndex, MaxTextureUnit[Stage]-1, Stage);
					checkNoEntry();
				}
			}
			else
			{
				//Bindless, save off the slot information
				FOpenGLBindlessSamplerInfo Info;
				Info.Handle = Location;
				Info.Slot = FirstTextureUnit[Stage] + SamplerIndex;
				Samplers.Add(Info);
			}
		}
	}

	// Bind UAVs/images.
	Name.Buffer[1] = 'i';
	Name.Buffer[2] = 0;
	Name.Buffer[3] = 0;
	Name.Buffer[4] = 0;
	int32 LastFoundUAVIndex = -1;
	for (int32 UAVIndex = 0; UAVIndex < Config.Shaders[Stage].Bindings.NumUAVs; ++UAVIndex)
	{
		ANSICHAR* Str = SetIndex(Name.Buffer, 2, UAVIndex);
		GLint Location = glGetUniformLocation(StageProgram, Name.Buffer);
		if (Location == -1)
		{
			// SSBO
			Str[0] = '_';
			Str[1] = 'V';
			Str[2] = 'A';
			Str[3] = 'R';
			Str[4] = '\0';
			Location = glGetProgramResourceIndex(StageProgram, GL_SHADER_STORAGE_BLOCK, Name.Buffer);
		}

		if (Location == -1)
		{
			if (LastFoundUAVIndex != -1)
			{
				// It may be an array of UAVs. Get the initial element location, if available, and count from it.
				SetIndex(Name.Buffer, 2, LastFoundUAVIndex);
				int32 OffsetOfArraySpecifier = (LastFoundUAVIndex>9)?4:3;
				int32 ArrayIndex = UAVIndex-LastFoundUAVIndex;
				Name.Buffer[OffsetOfArraySpecifier] = '[';
				ANSICHAR* EndBracket = SetIndex(Name.Buffer, OffsetOfArraySpecifier+1, ArrayIndex);
				*EndBracket++ = ']';
				*EndBracket = '\0';
				Location = glGetUniformLocation(StageProgram, Name.Buffer);
			}
		}
		else
		{
			LastFoundUAVIndex = UAVIndex;
		}

		if (Location != -1)
		{
			// compute shaders have layout(binding) for images
			// glUniform1i(Location, FirstUAVUnit[Stage] + UAVIndex);
			
			UAVStageNeeds[ FirstUAVUnit[Stage] + UAVIndex ] = true;
			MaxUAVUnitUsed = FMath::Max(MaxUAVUnitUsed, FirstUAVUnit[Stage] + UAVIndex);
		}
	}

	// Bind uniform buffers.
	if (FOpenGL::SupportsUniformBuffers())
	{
		Name.Buffer[1] = 'b';
		Name.Buffer[2] = 0;
		Name.Buffer[3] = 0;
		Name.Buffer[4] = 0;
		for (int32 BufferIndex = 0; BufferIndex < Config.Shaders[Stage].Bindings.NumUniformBuffers; ++BufferIndex)
		{
			SetIndex(Name.Buffer, 2, BufferIndex);
			GLint Location = GetOpenGLProgramUniformBlockIndex(StageProgram, Name);
			if (Location >= 0)
			{
				GetOpenGLProgramUniformBlockBinding(StageProgram, Location, FirstUniformBuffer + BufferIndex);
			}
		}
	}
}

#if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION

#define ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097 1
/*
	As of CL 1862097 uniform buffer names are mangled to avoid collisions between variables referenced
	in different shaders of the same program

	layout(std140) uniform _vb0
	{
	#define View View_vb0
	anon_struct_0000 View;
	};

	layout(std140) uniform _vb1
	{
	#define Primitive Primitive_vb1
	anon_struct_0001 Primitive;
	};
*/
	

struct UniformData
{
	UniformData(uint32 InOffset, uint32 InArrayElements)
		: Offset(InOffset)
		, ArrayElements(InArrayElements)
	{
	}
	uint32 Offset;
	uint32 ArrayElements;

	bool operator == (const UniformData& RHS) const
	{
		return	Offset == RHS.Offset &&	ArrayElements == RHS.ArrayElements;
	}
	bool operator != (const UniformData& RHS) const
	{
		return	!(*this == RHS);
	}
};
#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
static void VerifyUniformLayout(const FString& BlockName, const TCHAR* UniformName, const UniformData& GLSLUniform)
#else
static void VerifyUniformLayout(const TCHAR* UniformName, const UniformData& GLSLUniform)
#endif //#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
{
	static TMap<FString, UniformData> Uniforms;

	if(!Uniforms.Num())
	{
		for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
		{
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
			UE_LOG(LogRHI, Log, TEXT("UniformBufferStruct %s %s %d"),
				StructIt->GetStructTypeName(),
				StructIt->GetShaderVariableName(),
				StructIt->GetSize()
				);
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
			const TArray<FShaderParametersMetadata::FMember>& StructMembers = StructIt->GetMembers();
			for(int32 MemberIndex = 0;MemberIndex < StructMembers.Num();++MemberIndex)
			{
				const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];

				FString BaseTypeName;
				switch(Member.GetBaseType())
				{
					case UBMT_NESTED_STRUCT:  BaseTypeName = TEXT("struct");  break;
					case UBMT_INT32:   BaseTypeName = TEXT("int"); break;
					case UBMT_UINT32:  BaseTypeName = TEXT("uint"); break;
					case UBMT_FLOAT32: BaseTypeName = TEXT("float"); break;
					case UBMT_TEXTURE: BaseTypeName = TEXT("texture"); break;
					case UBMT_SAMPLER: BaseTypeName = TEXT("sampler"); break;
					default:           UE_LOG(LogShaders, Fatal,TEXT("Unrecognized uniform buffer struct member base type."));
				};
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
				UE_LOG(LogRHI, Log, TEXT("  +%d %s%dx%d %s[%d]"),
					Member.GetOffset(),
					*BaseTypeName,
					Member.GetNumRows(),
					Member.GetNumColumns(),
					Member.GetName(),
					Member.GetNumElements()
					);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
				FString CompositeName = FString(StructIt->GetShaderVariableName()) + TEXT("_") + Member.GetName();

				// GLSL returns array members with a "[0]" suffix
				if(Member.GetNumElements())
				{
					CompositeName += TEXT("[0]");
				}

				check(!Uniforms.Contains(CompositeName));
				Uniforms.Add(CompositeName, UniformData(Member.GetOffset(), Member.GetNumElements()));
			}
		}
	}

#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
	/* unmangle the uniform name by stripping the block name from it
	
	layout(std140) uniform _vb0
	{
	#define View View_vb0
		anon_struct_0000 View;
	};
	*/
	FString RequestedUniformName(UniformName);
	RequestedUniformName = RequestedUniformName.Replace(*BlockName, TEXT(""));
	if(RequestedUniformName.StartsWith(TEXT("."), ESearchCase::CaseSensitive))
	{
		RequestedUniformName.RightChopInline(1, false);
	}
#else
	FString RequestedUniformName = UniformName;
#endif

	const UniformData* FoundUniform = Uniforms.Find(RequestedUniformName);

	// MaterialTemplate uniform buffer does not have an entry in the FShaderParametersMetadatas list, so skipping it here
	if(!(RequestedUniformName.StartsWith("Material_") || RequestedUniformName.StartsWith("MaterialCollection")))
	{
		if(!FoundUniform || (*FoundUniform != GLSLUniform))
		{
			UE_LOG(LogRHI, Fatal, TEXT("uniform buffer member %s in the GLSL source doesn't match it's declaration in it's FShaderParametersMetadata"), *RequestedUniformName);
		}
	}
}

static void VerifyUniformBufferLayouts(GLuint Program)
{
	GLint NumBlocks = 0;
	glGetProgramiv(Program, GL_ACTIVE_UNIFORM_BLOCKS, &NumBlocks);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
	UE_LOG(LogRHI, Log, TEXT("program %d has %d uniform blocks"), Program, NumBlocks);
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP

	for(GLint BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
	{
		const GLsizei BufferSize = 256;
		char Buffer[BufferSize] = {0};
		GLsizei Length = 0;

		GLint ActiveUniforms = 0;
		GLint BlockBytes = 0;

		glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &ActiveUniforms);
		glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &BlockBytes);
		glGetActiveUniformBlockName(Program, BlockIndex, BufferSize, &Length, Buffer);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
		FString BlockName(Buffer);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097

		FString ReferencedBy;
		{
			GLint ReferencedByVS = 0;
			GLint ReferencedByPS = 0;
			GLint ReferencedByGS = 0;
			GLint ReferencedByHS = 0;
			GLint ReferencedByDS = 0;
			GLint ReferencedByCS = 0;

			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, &ReferencedByVS);
			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, &ReferencedByPS);
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER
			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER, &ReferencedByGS);
#endif
			if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
			{
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER, &ReferencedByHS);
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER, &ReferencedByDS);
#endif
			}
			
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER, &ReferencedByCS);
#endif

			if(ReferencedByVS) {ReferencedBy += TEXT("V");}
			if(ReferencedByHS) {ReferencedBy += TEXT("H");}
			if(ReferencedByDS) {ReferencedBy += TEXT("D");}
			if(ReferencedByGS) {ReferencedBy += TEXT("G");}
			if(ReferencedByPS) {ReferencedBy += TEXT("P");}
			if(ReferencedByCS) {ReferencedBy += TEXT("C");}
		}
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		UE_LOG(LogRHI, Log, TEXT("  [%d] uniform block (%s) = %s, %d active uniforms, %d bytes {"),
			BlockIndex,
			*ReferencedBy,
			ANSI_TO_TCHAR(Buffer),
			ActiveUniforms,
			BlockBytes
			);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		if(ActiveUniforms)
		{
			// the other TArrays copy construct this to get the proper array size
			TArray<GLint> ActiveUniformIndices;
			ActiveUniformIndices.Init(ActiveUniforms);

			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, ActiveUniformIndices.GetData());
			
			TArray<GLint> ActiveUniformOffsets(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_OFFSET, ActiveUniformOffsets.GetData());

			TArray<GLint> ActiveUniformSizes(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_SIZE, ActiveUniformSizes.GetData());

			TArray<GLint> ActiveUniformTypes(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_TYPE, ActiveUniformTypes.GetData());

			TArray<GLint> ActiveUniformArrayStrides(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_ARRAY_STRIDE, ActiveUniformArrayStrides.GetData());

			extern const TCHAR* GetGLUniformTypeString( GLint UniformType );

			for(GLint i = 0; i < ActiveUniformIndices.Num(); ++i)
			{
				const GLint UniformIndex = ActiveUniformIndices[i];
				GLsizei Size = 0;
				GLenum Type = 0;
				glGetActiveUniform(Program, UniformIndex , BufferSize, &Length, &Size, &Type, Buffer);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
				UE_LOG(LogRHI, Log, TEXT("    [%d] +%d %s %s %d elements %d array stride"),
					UniformIndex,
					ActiveUniformOffsets[i],
					GetGLUniformTypeString(ActiveUniformTypes[i]),
					ANSI_TO_TCHAR(Buffer),
					ActiveUniformSizes[i],
					ActiveUniformArrayStrides[i]
				);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		
				const UniformData GLSLUniform
				(
					ActiveUniformOffsets[i],
					ActiveUniformArrayStrides[i] > 0 ? ActiveUniformSizes[i] : 0 // GLSL has 1 as array size for non-array uniforms, but FShaderParametersMetadata assumes 0
				);
#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
				VerifyUniformLayout(BlockName, ANSI_TO_TCHAR(Buffer), GLSLUniform);
#else
				VerifyUniformLayout(ANSI_TO_TCHAR(Buffer), GLSLUniform);
#endif
			}
		}
	}
}
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION
#define PROGRAM_BINARY_RETRIEVABLE_HINT             0x8257

/**
 * Link vertex and pixel shaders in to an OpenGL program.
 */
static FOpenGLLinkedProgram* LinkProgram( const FOpenGLLinkedProgramConfiguration& Config, bool bFromPSOFileCache)
{
	ANSICHAR Buf[32] = {0};

	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLinkTime);
	VERIFY_GL_SCOPE();

	// ensure that compute shaders are always alone
	check( (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource == 0) != (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource == 0));
	check( (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource == 0) != (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource == 0));

	TArray<uint8> CachedProgramBinary;
	GLuint Program = 0;
	bool bShouldLinkProgram = true;
	if (FOpenGLProgramBinaryCache::IsEnabled())
	{
		// Try to create program from a saved binary
		bShouldLinkProgram = !FOpenGLProgramBinaryCache::UseCachedProgram(Program, Config.ProgramKey, CachedProgramBinary);
		if (bShouldLinkProgram)
		{
			// In case there is no saved binary in the cache, compile required shaders we have deferred before
			UE::OpenGL::CompilePendingShaders(Config);
		}
	}

	if (Program == 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_glGenProgramPipelines);
		FOpenGL::GenProgramPipelines(1, &Program);
	}

	if (bShouldLinkProgram)
	{
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_VERTEX_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_FRAGMENT_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_GEOMETRY_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_COMPUTE_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource);
		}
	
		if(FOpenGLProgramBinaryCache::IsEnabled() || GetOpenGLProgramsCache().IsUsingLRU())
		{
			FOpenGL::ProgramParameter(Program, PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
		}

		// Link.
		glLinkProgram(Program);
	}

	if (VerifyLinkedProgram(Program))
	{
		if (bShouldLinkProgram)
		{
			SetNewProgramStats(Program);

			if (FOpenGLProgramBinaryCache::IsEnabled())
			{
				check(CachedProgramBinary.Num() == 0);
				FOpenGLProgramBinaryCache::CacheProgram(Program, Config.ProgramKey, CachedProgramBinary);
			}
		}
	}
	else
	{
		return nullptr;
	}
	
	FOpenGL::BindProgramPipeline(Program);

	FOpenGLLinkedProgram* LinkedProgram = new FOpenGLLinkedProgram(Config, Program);

	if (GetOpenGLProgramsCache().IsUsingLRU() && CVarLRUKeepProgramBinaryResident.GetValueOnAnyThread() && CachedProgramBinary.Num())
	{
		// Store the binary data in LRUInfo, this avoids requesting a program binary from the driver when this program is evicted.
		INC_MEMORY_STAT_BY(STAT_OpenGLShaderLRUProgramMemory, CachedProgramBinary.Num());
		LinkedProgram->LRUInfo.CachedProgramBinary = MoveTemp(CachedProgramBinary);
	}
	ConfigureStageStates(LinkedProgram);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION
	VerifyUniformBufferLayouts(Program);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION
	return LinkedProgram;
}

static bool LinkComputeShader(FRHIComputeShader* ComputeShaderRHI, FOpenGLComputeShader* ComputeShader)
{
	check(ComputeShader);
	check(ComputeShader->Resource != 0);
	check(ComputeShaderRHI->GetHash() != FSHAHash());

	FOpenGLLinkedProgramConfiguration Config;
	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource = ComputeShader->Resource;
	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Bindings = ComputeShader->Bindings;
	Config.ProgramKey.ShaderHashes[CrossCompiler::SHADER_STAGE_COMPUTE] = ComputeShaderRHI->GetHash();
	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].ShaderKey = FOpenGLDynamicRHI::ResourceCastProxy(ComputeShaderRHI)->GetCompiledShaderKey();

	ComputeShader->LinkedProgram = GetOpenGLProgramsCache().Find(Config.ProgramKey, true);

	if (ComputeShader->LinkedProgram == nullptr)
	{
		ComputeShader->LinkedProgram = LinkProgram(Config, false);
		if(ComputeShader->LinkedProgram == nullptr)
		{
		#if DEBUG_GL_SHADERS
			UE_LOG(LogRHI, Error, TEXT("Compute Shader:\n%s"), ANSI_TO_TCHAR(ComputeShader->GlslCode.GetData()));
		#endif //DEBUG_GL_SHADERS
			checkf(ComputeShader->LinkedProgram, TEXT("Compute shader failed to compile & link."));

			FName LinkFailurePanic = FName("FailedComputeProgramLink");
			RHIGetPanicDelegate().ExecuteIfBound(LinkFailurePanic);
			UE_LOG(LogRHI, Fatal, TEXT("Failed to link compute program [%s]. Current total programs: %d"), *Config.ProgramKey.ToString(), GNumPrograms);
			return false;
		}
		GetOpenGLProgramsCache().Add(Config.ProgramKey, ComputeShader->LinkedProgram);
	}

	return true;
}

FOpenGLLinkedProgram* FOpenGLDynamicRHI::GetLinkedComputeProgram(FRHIComputeShader* ComputeShaderRHI)
{
	VERIFY_GL_SCOPE();
	check(ComputeShaderRHI->GetHash() != FSHAHash());
	FOpenGLComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);

	FOpenGLLinkedProgramConfiguration Config;

	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource = ComputeShader->Resource;
	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Bindings = ComputeShader->Bindings;
	Config.ProgramKey.ShaderHashes[CrossCompiler::SHADER_STAGE_COMPUTE] = ComputeShaderRHI->GetHash();

	FOpenGLLinkedProgram* LinkedProgram = GetOpenGLProgramsCache().Find(Config.ProgramKey, true);
	if (!LinkedProgram)
	{
		// ensure that pending request for this program has been completed before attempting to link
		if (FOpenGLProgramBinaryCache::CheckSinglePendingGLProgramCreateRequest(Config.ProgramKey))
		{
			LinkedProgram = GetOpenGLProgramsCache().Find(Config.ProgramKey, true);
		}
	}

	if (LinkedProgram == nullptr)
	{
		// Not in the cache. Create and add the program here.
		// We can now link the compute shader, by now the shader hash has been set.
		LinkComputeShader(ComputeShaderRHI, ComputeShader);
		check(ComputeShader->LinkedProgram);
		LinkedProgram = ComputeShader->LinkedProgram;
	}
	else if (!LinkedProgram->bConfigIsInitalized)
	{
		// this has been loaded via binary program cache, properly initialize it here:
		LinkedProgram->SetConfig(Config);
		// We now have the config for this program, we must configure the program for use.
		ConfigureGLProgramStageStates(LinkedProgram);
	}
	check(LinkedProgram->bConfigIsInitalized);
	return LinkedProgram;
}

FComputeShaderRHIRef FOpenGLDynamicRHI::RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateProxyShader<FRHIComputeShader>(Code, Hash);
}

template<class TOpenGLStage>
static FString GetShaderStageSource(TOpenGLStage* Shader)
{
	FString Source;
#if DEBUG_GL_SHADERS
	Source = Shader->GlslCodeString;
#else
	GLsizei NumShaders = 0;
	glGetProgramiv(Shader->Resource, GL_ATTACHED_SHADERS, (GLint*)&NumShaders);
	if(NumShaders > 0)
	{
		GLuint* Shaders = (GLuint*)alloca(sizeof(GLuint)*NumShaders);
		glGetAttachedShaders(Shader->Resource, NumShaders, &NumShaders, Shaders);
		for(int32 i = 0; i < NumShaders; i++)
		{
			GLint Len = 0;
			glGetShaderiv(Shaders[i], GL_SHADER_SOURCE_LENGTH, &Len);
			if(Len > 0)
			{
				ANSICHAR* Code = new ANSICHAR[Len + 1];
				glGetShaderSource(Shaders[i], Len + 1, &Len, Code);
				Source += Code;
				delete [] Code;
			}
		}
	}
#endif
	return Source;
}

// ============================================================================================================================

struct FOpenGLShaderVaryingMapping
{
	FAnsiCharArray Name;
	int32 WriteLoc;
	int32 ReadLoc;
};

typedef TMap<FOpenGLLinkedProgramConfiguration,FOpenGLLinkedProgramConfiguration::ShaderInfo> FOpenGLSeparateShaderObjectCache;

template<class TOpenGLStage0RHI, class TOpenGLStage1RHI>
static void BindShaderStage(FOpenGLLinkedProgramConfiguration& Config, CrossCompiler::EShaderStage NextStage, TOpenGLStage0RHI* NextStageShaderIn, CrossCompiler::EShaderStage PrevStage, TOpenGLStage1RHI* PrevStageShaderIn)
{
	auto* PrevStageShader = FOpenGLDynamicRHI::ResourceCast(PrevStageShaderIn);
	auto* NextStageShader = FOpenGLDynamicRHI::ResourceCast(NextStageShaderIn);

	check(NextStageShader && PrevStageShader);

	typedef typename TOpenGLResourceTraits<TOpenGLStage0RHI>::TConcreteType::ContainedGLType TOpenGLStage0;
	typedef typename TOpenGLResourceTraits<TOpenGLStage1RHI>::TConcreteType::ContainedGLType TOpenGLStage1;

	FOpenGLLinkedProgramConfiguration::ShaderInfo& ShaderInfo = Config.Shaders[NextStage];
	FOpenGLLinkedProgramConfiguration::ShaderInfo& PrevInfo = Config.Shaders[PrevStage];

	GLuint NextStageResource = NextStageShader->Resource;
	FOpenGLShaderBindings NextStageBindings = NextStageShader->Bindings;
	
	ShaderInfo.Bindings = NextStageBindings;
	ShaderInfo.Resource = NextStageResource;
}

// ============================================================================================================================
static FCriticalSection GProgramBinaryCacheCS;



static FOpenGLLinkedProgramConfiguration CreateConfig(FRHIVertexShader* VertexShaderRHI, FRHIPixelShader* PixelShaderRHI, FRHIGeometryShader* GeometryShaderRHI )
{
	FOpenGLVertexShader* VertexShader = FOpenGLDynamicRHI::ResourceCast(VertexShaderRHI);
	FOpenGLPixelShader* PixelShader = FOpenGLDynamicRHI::ResourceCast(PixelShaderRHI);
	FOpenGLGeometryShader* GeometryShader = FOpenGLDynamicRHI::ResourceCast(GeometryShaderRHI);

	FOpenGLLinkedProgramConfiguration Config;

	check(VertexShaderRHI);
	check(PixelShaderRHI);

	// Fill-in the configuration
	Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings = VertexShader->Bindings;
	Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource = VertexShader->Resource;
	Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].ShaderKey = FOpenGLDynamicRHI::ResourceCastProxy(VertexShaderRHI)->GetCompiledShaderKey();
	Config.ProgramKey.ShaderHashes[CrossCompiler::SHADER_STAGE_VERTEX] = VertexShaderRHI->GetHash();

	if (GeometryShaderRHI)
	{
		check(VertexShader);
		BindShaderStage(Config, CrossCompiler::SHADER_STAGE_GEOMETRY, GeometryShaderRHI, CrossCompiler::SHADER_STAGE_VERTEX, VertexShaderRHI);
		Config.ProgramKey.ShaderHashes[CrossCompiler::SHADER_STAGE_GEOMETRY] = GeometryShaderRHI->GetHash();
		Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].ShaderKey = FOpenGLDynamicRHI::ResourceCastProxy(GeometryShaderRHI)->GetCompiledShaderKey();
	}

	check(GeometryShaderRHI || VertexShaderRHI);
	if (GeometryShaderRHI)
	{
		BindShaderStage(Config, CrossCompiler::SHADER_STAGE_PIXEL, PixelShaderRHI, CrossCompiler::SHADER_STAGE_GEOMETRY, GeometryShaderRHI);
	}
	else
	{
		BindShaderStage(Config, CrossCompiler::SHADER_STAGE_PIXEL, PixelShaderRHI, CrossCompiler::SHADER_STAGE_VERTEX, VertexShaderRHI);
	}
	Config.ProgramKey.ShaderHashes[CrossCompiler::SHADER_STAGE_PIXEL] = PixelShaderRHI->GetHash();
	Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].ShaderKey = FOpenGLDynamicRHI::ResourceCastProxy(PixelShaderRHI)->GetCompiledShaderKey();


	return Config;
};


static bool CanCreateExternally(bool bIsFromPSO)
{
#if PLATFORM_ANDROID
	if (bIsFromPSO && FOpenGLProgramBinaryCache::IsBuildingCache() && FAndroidOpenGL::AreRemoteCompileServicesActive())
	{
		return true;
	}
#endif
	return false;
}

void FOpenGLDynamicRHI::PrepareGFXBoundShaderState(const FGraphicsPipelineStateInitializer& Initializer)
{
#if PLATFORM_ANDROID
	if (CanCreateExternally(Initializer.bFromPSOFileCache))
	{
		FRHIVertexShader* VertexShaderRHI = Initializer.BoundShaderState.GetVertexShader();
		FRHIPixelShader* PixelShaderRHI = Initializer.BoundShaderState.GetPixelShader();

		if (!PixelShaderRHI)
		{
			// use special null pixel shader when PixelShader was set to NULL
			PixelShaderRHI = TShaderMapRef<FNULLPS>(GetGlobalShaderMap(GMaxRHIFeatureLevel)).GetPixelShader();
		}
	
		FOpenGLProgramKey ProgramKey;
		ProgramKey.ShaderHashes[CrossCompiler::SHADER_STAGE_VERTEX] = VertexShaderRHI->GetHash();
		ProgramKey.ShaderHashes[CrossCompiler::SHADER_STAGE_PIXEL] = PixelShaderRHI->GetHash();

		// add it to the runtime container. (this prevents createboundshader state from adding it to the binary cache.)
		{
			FScopeLock Lock(&GProgramBinaryCacheCS);
			check(GetOpenGLProgramsCache().IsUsingLRU());
			if (GetOpenGLProgramsCache().Find(ProgramKey, false) != nullptr)
			{
				//already done?
				UE_LOG(LogRHI, Warning, TEXT("PrepareGFXBoundShaderState Already done? %s is pso %d "), *ProgramKey.ToString(), Initializer.bFromPSOFileCache ? 1 : 0);
				return;
			}
		}

		// compile externally, sit and wait for the linked result	
		const FOpenGLCompiledShaderKey& VSKey = ResourceCastProxy(VertexShaderRHI)->GetCompiledShaderKey();
		const FOpenGLCompiledShaderKey& PSKey = ResourceCastProxy(PixelShaderRHI)->GetCompiledShaderKey();

		TArray<ANSICHAR> VSCode;
		TArray<ANSICHAR> PSCode;
		TArray<ANSICHAR> ComputeGlslCode; 
		{
			FScopeLock Lock(&GCompiledShaderCacheCS);
			VSCode = GetOpenGLCompiledShaderCache().FindRef(VSKey)->GetUncompressedShader();
			PSCode = GetOpenGLCompiledShaderCache().FindRef(PSKey)->GetUncompressedShader();
		}

		FString FailLog;
		TArray<uint8> CompiledProgramResult = FAndroidOpenGL::DispatchAndWaitForRemoteGLProgramCompile(TArrayView<uint8>((uint8*)&ProgramKey, sizeof(ProgramKey)), VSCode, PSCode, ComputeGlslCode, FailLog);

		if(FailLog.IsEmpty())
		{
			GLenum glFormat = *(GLenum*)CompiledProgramResult.GetData();		
			if (UE::OpenGL::IsStoringCompressedBinaryPrograms())
			{
				
				TArray<uint8> CompressedCompiledProgramResult;
				UE::OpenGL::CompressProgramBinary(CompiledProgramResult, CompressedCompiledProgramResult);
				CompiledProgramResult = MoveTemp(CompressedCompiledProgramResult);
			}

			// add it to the binary file.
			FOpenGLProgramBinaryCache::CacheProgramBinary(ProgramKey, CompiledProgramResult);

			// add it to the runtime container. (this prevents createboundshader state from adding it to the binary cache.)
			{
				FScopeLock Lock(&GProgramBinaryCacheCS);
				check(GetOpenGLProgramsCache().IsUsingLRU());
				if (GetOpenGLProgramsCache().Find(ProgramKey, false) == nullptr)
				{
					GetOpenGLProgramsCache().AddAsEvicted(ProgramKey, MoveTemp(CompiledProgramResult));
				}
				else
				{
					// beaten to it by another thread.
					UE_LOG(LogRHI, Warning, TEXT("PrepareGFXBoundShaderState skipped add. %s is pso %d "), *ProgramKey.ToString(), Initializer.bFromPSOFileCache ? 1 : 0);
				}
			}

		}
		else
		{
			UE_LOG(LogRHI, Error, TEXT("External compile of program %s failed: %s "), *ProgramKey.ToString(), *FailLog);
#if DEBUG_GL_SHADERS
			if (VSCode.Num())
			{
				UE_LOG(LogRHI, Error, TEXT("Vertex Shader:\n%s"), ANSI_TO_TCHAR(VSCode.GetData()));
			}
			if (PSCode.Num())
			{
				UE_LOG(LogRHI, Error, TEXT("Pixel Shader:\n%s"), ANSI_TO_TCHAR(PSCode.GetData()));
			}
#endif //DEBUG_GL_SHADERS
		}
	}
#endif
}

FBoundShaderStateRHIRef FOpenGLDynamicRHI::RHICreateBoundShaderState_OnThisThread(
	FRHIVertexDeclaration* VertexDeclarationRHI,
	FRHIVertexShader* VertexShaderRHI,
	FRHIPixelShader* PixelShaderRHI,
	FRHIGeometryShader* GeometryShaderRHI,
	bool bFromPSOFileCache
	)
{
	check(IsInRenderingThread() || IsInRHIThread());

	FScopeLock Lock(&GProgramBinaryCacheCS);

	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateBoundShaderStateTime);

	if (!PixelShaderRHI)
	{
		// use special null pixel shader when PixelShader was set to NULL
		PixelShaderRHI = GetNULLPixelShader();
	}



	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		GeometryShaderRHI
		);

	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		FOpenGLBoundShaderState* BoundShaderState = ResourceCast(CachedBoundShaderStateLink->BoundShaderState);
		FOpenGLLinkedProgram* LinkedProgram = BoundShaderState->LinkedProgram;
		GetOpenGLProgramsCache().Touch(LinkedProgram);

		if (!LinkedProgram->bConfigIsInitalized)
		{
			// touch has unevicted the program, set it up.
			FOpenGLLinkedProgramConfiguration Config = CreateConfig(VertexShaderRHI, PixelShaderRHI, GeometryShaderRHI );
			LinkedProgram->SetConfig(Config);
			// We now have the config for this program, we must configure the program for use.
			ConfigureGLProgramStageStates(LinkedProgram);
		}
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		FOpenGLLinkedProgramConfiguration Config = CreateConfig(VertexShaderRHI, PixelShaderRHI, GeometryShaderRHI);

		// Check if we already have such a program in released programs cache. Use it, if we do.
		FOpenGLLinkedProgram* LinkedProgram = 0;

		int32 Index = StaticLastReleasedProgramsIndex;
		for( int CacheIndex = 0; CacheIndex < LAST_RELEASED_PROGRAMS_CACHE_COUNT; ++CacheIndex )
		{
			FOpenGLLinkedProgram* Prog = StaticLastReleasedPrograms[Index];
			if( Prog && Prog->Config == Config )
			{
				StaticLastReleasedPrograms[Index] = 0;
				LinkedProgram = Prog;
				GetOpenGLProgramsCache().Touch(LinkedProgram);
				break;
			}
			Index = (Index == LAST_RELEASED_PROGRAMS_CACHE_COUNT-1) ? 0 : Index+1;
		}

		if (!LinkedProgram)
		{
			bool bFindAndCreateEvictedProgram = true;
			// If this is this a request from the PSOFC then do not create an evicted program.
			if (bFromPSOFileCache && GetOpenGLProgramsCache().IsUsingLRU())
			{
				bFindAndCreateEvictedProgram = false;
			}

			FOpenGLLinkedProgram* CachedProgram = GetOpenGLProgramsCache().Find(Config.ProgramKey, bFindAndCreateEvictedProgram);
			if (!CachedProgram)
			{
				// ensure that pending request for this program has been completed before
				if (FOpenGLProgramBinaryCache::CheckSinglePendingGLProgramCreateRequest(Config.ProgramKey))
				{
					CachedProgram = GetOpenGLProgramsCache().Find(Config.ProgramKey, bFindAndCreateEvictedProgram);
				}
			}

			if (CachedProgram)
			{
				LinkedProgram = CachedProgram;
				if (!LinkedProgram->bConfigIsInitalized && bFindAndCreateEvictedProgram)
				{
					LinkedProgram->SetConfig(Config);
					// We now have the config for this program, we must configure the program for use.
					ConfigureGLProgramStageStates(LinkedProgram);
				}
			}
			else
			{
				FOpenGLVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
				FOpenGLPixelShader* PixelShader = ResourceCast(PixelShaderRHI);
				FOpenGLGeometryShader* GeometryShader = ResourceCast(GeometryShaderRHI);
		
				// Make sure we have OpenGL context set up, and invalidate the parameters cache and current program (as we'll link a new one soon)
				GetContextStateForCurrentContext().Program = -1;
				MarkShaderParameterCachesDirty(PendingState.ShaderParameters, false);
				PendingState.LinkedProgramAndDirtyFlag = nullptr;

				// Link program, using the data provided in config
				LinkedProgram = LinkProgram(Config, bFromPSOFileCache);

				if (LinkedProgram == NULL)
				{
#if DEBUG_GL_SHADERS
					if (VertexShader)
					{
						UE_LOG(LogRHI, Error, TEXT("Vertex Shader:\n%s"), ANSI_TO_TCHAR(VertexShader->GlslCode.GetData()));
					}
					if (PixelShader)
					{
						UE_LOG(LogRHI, Error, TEXT("Pixel Shader:\n%s"), ANSI_TO_TCHAR(PixelShader->GlslCode.GetData()));
					}
					if (GeometryShader)
					{
						UE_LOG(LogRHI, Error, TEXT("Geometry Shader:\n%s"), ANSI_TO_TCHAR(GeometryShader->GlslCode.GetData()));
					}
#endif //DEBUG_GL_SHADERS
					FName LinkFailurePanic = bFromPSOFileCache ? FName("FailedProgramLinkDuringPrecompile") : FName("FailedProgramLink");
					RHIGetPanicDelegate().ExecuteIfBound(LinkFailurePanic);
					UE_LOG(LogRHI, Fatal, TEXT("Failed to link program [%s]. Current total programs: %d, precompile: %d"), *Config.ProgramKey.ToString(), GNumPrograms, (uint32)bFromPSOFileCache);
				}

				GetOpenGLProgramsCache().Add(Config.ProgramKey, LinkedProgram);

				// if building the cache file and using the LRU then evict the last shader created. this will reduce the risk of fragmentation of the driver's program memory.
				if (bFindAndCreateEvictedProgram == false && FOpenGLProgramBinaryCache::IsBuildingCache())
				{
					GetOpenGLProgramsCache().EvictMostRecent();
				}
			}
		}

		check(VertexDeclarationRHI);
		
		FOpenGLVertexDeclaration* VertexDeclaration = ResourceCast(VertexDeclarationRHI);
		FOpenGLBoundShaderState* BoundShaderState = new FOpenGLBoundShaderState(
			LinkedProgram,
			VertexDeclarationRHI,
			VertexShaderRHI,
			PixelShaderRHI,
			GeometryShaderRHI
			);

		return BoundShaderState;
	}
}

void DestroyShadersAndPrograms()
{
	VERIFY_GL_SCOPE();
	GetOpenGLUniformBlockLocations().Empty();
	GetOpenGLUniformBlockBindings().Empty();
	
	GetOpenGLProgramsCache().Empty();


	StaticLastReleasedProgramsIndex = 0;

	{
		FScopeLock Lock(&GCompiledShaderCacheCS);
		FOpenGLCompiledShaderCache& ShaderCache = GetOpenGLCompiledShaderCache();
		for (FOpenGLCompiledShaderCache::TIterator It(ShaderCache); It; ++It)
		{
			FOpenGL::DeleteShader(It.Value()->Resource);
		}
		ShaderCache.Empty();
	}
	{
		FOpenGLCompiledLibraryShaderCache& ShaderCache = GetOpenGLCompiledLibraryShaderCache();
		for (FOpenGLCompiledLibraryShaderCache::TIterator It(ShaderCache); It; ++It)
		{
			delete It.Value().Header;
		}
		ShaderCache.Empty();
	}
}

struct FSamplerPair
{
	GLuint Texture;
	GLuint Sampler;

	friend bool operator ==(const FSamplerPair& A,const FSamplerPair& B)
	{
		return A.Texture == B.Texture && A.Sampler == B.Sampler;
	}

	friend uint32 GetTypeHash(const FSamplerPair &Key)
	{
		return Key.Texture ^ (Key.Sampler << 18);
	}
};

static TMap<FSamplerPair, GLuint64> BindlessSamplerMap;

void FOpenGLDynamicRHI::SetupBindlessTextures( FOpenGLContextState& ContextState, const TArray<FOpenGLBindlessSamplerInfo> &Samplers )
{
	if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
	{
		return;
	}
	VERIFY_GL_SCOPE();

	// Bind all textures via Bindless
	for (int32 Texture = 0; Texture < Samplers.Num(); Texture++)
	{
		const FOpenGLBindlessSamplerInfo &Sampler = Samplers[Texture];

		GLuint64 BindlessSampler = 0xffffffff;
		FSamplerPair Pair;
		Pair.Texture = PendingState.Textures[Sampler.Slot].Resource;
		Pair.Sampler = (PendingState.SamplerStates[Sampler.Slot] != NULL) ? PendingState.SamplerStates[Sampler.Slot]->Resource : 0;

		if (Pair.Texture)
		{
			// Find Sampler pair
			if ( BindlessSamplerMap.Contains(Pair))
			{
				BindlessSampler = BindlessSamplerMap[Pair];
			}
			else
			{
				// if !found, create

				if (Pair.Sampler)
				{
					BindlessSampler = FOpenGL::GetTextureSamplerHandle( Pair.Texture, Pair.Sampler);
				}
				else
				{
					BindlessSampler = FOpenGL::GetTextureHandle( Pair.Texture);
				}

				FOpenGL::MakeTextureHandleResident( BindlessSampler);

				BindlessSamplerMap.Add( Pair, BindlessSampler);
			}

			FOpenGL::UniformHandleui64( Sampler.Handle, BindlessSampler);
		}
	}
}


void FOpenGLDynamicRHI::BindPendingShaderState( FOpenGLContextState& ContextState )
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLShaderBindTime);
	VERIFY_GL_SCOPE();

	bool ForceUniformBindingUpdate = false;

	GLuint PendingProgram = PendingState.BoundShaderState->LinkedProgram->Program;
	if (ContextState.Program != PendingProgram)
	{
		FOpenGL::BindProgramPipeline(PendingProgram);
		ContextState.Program = PendingProgram;
		MarkShaderParameterCachesDirty(PendingState.ShaderParameters, false);
		PendingState.LinkedProgramAndDirtyFlag = nullptr;
	}

	if (PendingState.bAnyDirtyRealUniformBuffers[SF_Vertex] || 
		PendingState.bAnyDirtyRealUniformBuffers[SF_Pixel] || 
		PendingState.bAnyDirtyRealUniformBuffers[SF_Geometry])
	{
		int32 NextUniformBufferIndex = OGL_FIRST_UNIFORM_BUFFER;

		static_assert(SF_NumGraphicsFrequencies == 5 && SF_NumFrequencies == 10, "Unexpected SF_ ordering");
		static_assert(SF_RayGen > SF_NumGraphicsFrequencies, "SF_NumGraphicsFrequencies be the number of frequencies supported in OpenGL");

		int32 NumUniformBuffers[SF_NumGraphicsFrequencies];

		PendingState.BoundShaderState->GetNumUniformBuffers(NumUniformBuffers);

		if (PendingState.bAnyDirtyRealUniformBuffers[SF_Vertex])
		{
			BindUniformBufferBase(
				ContextState,
				NumUniformBuffers[SF_Vertex],
				PendingState.BoundUniformBuffers[SF_Vertex],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
		}
		NextUniformBufferIndex += NumUniformBuffers[SF_Vertex];

		if (PendingState.bAnyDirtyRealUniformBuffers[SF_Pixel])
		{
			BindUniformBufferBase(
				ContextState,
				NumUniformBuffers[SF_Pixel],
				PendingState.BoundUniformBuffers[SF_Pixel],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
		}
		NextUniformBufferIndex += NumUniformBuffers[SF_Pixel];

		if (NumUniformBuffers[SF_Geometry] >= 0 && PendingState.bAnyDirtyRealUniformBuffers[SF_Geometry])
		{
			BindUniformBufferBase(
				ContextState,
				NumUniformBuffers[SF_Geometry],
				PendingState.BoundUniformBuffers[SF_Geometry],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
			NextUniformBufferIndex += NumUniformBuffers[SF_Geometry];
		}

		PendingState.bAnyDirtyRealUniformBuffers[SF_Vertex] = false;
		PendingState.bAnyDirtyRealUniformBuffers[SF_Pixel] = false;
		PendingState.bAnyDirtyRealUniformBuffers[SF_Geometry] = false;
	}

	if (FOpenGL::SupportsBindlessTexture())
	{
		SetupBindlessTextures(ContextState, PendingState.BoundShaderState->LinkedProgram->Samplers);
	}
}

FOpenGLBoundShaderState::FOpenGLBoundShaderState(
	FOpenGLLinkedProgram* InLinkedProgram,
	FRHIVertexDeclaration* InVertexDeclarationRHI,
	FRHIVertexShader* InVertexShaderRHI,
	FRHIPixelShader* InPixelShaderRHI,
	FRHIGeometryShader* InGeometryShaderRHI
	)
	:	CacheLink(InVertexDeclarationRHI, InVertexShaderRHI, InPixelShaderRHI,
		InGeometryShaderRHI, this)
{
	FOpenGLVertexDeclaration* InVertexDeclaration = FOpenGLDynamicRHI::ResourceCast(InVertexDeclarationRHI);
	VertexDeclaration = InVertexDeclaration;
	VertexShaderProxy = static_cast<FOpenGLVertexShaderProxy*>(InVertexShaderRHI);
	PixelShaderProxy = static_cast<FOpenGLPixelShaderProxy*>(InPixelShaderRHI);
	GeometryShaderProxy = static_cast<FOpenGLGeometryShaderProxy*>(InGeometryShaderRHI);

	LinkedProgram = InLinkedProgram;

	if (InVertexDeclaration)
	{
		FMemory::Memcpy(StreamStrides, InVertexDeclaration->StreamStrides, sizeof(StreamStrides));
	}
	else
	{
		FMemory::Memzero(StreamStrides, sizeof(StreamStrides));
	}
}

TAutoConsoleVariable<int32> CVarEvictOnBssDestruct(
	TEXT("r.OpenGL.EvictOnBSSDestruct"),
	0,
	TEXT(""),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);


FOpenGLBoundShaderState::~FOpenGLBoundShaderState()
{
	check(LinkedProgram);
	RunOnGLRenderContextThread([LinkedProgram = LinkedProgram]()
	{
		const bool bIsEvicted = GetOpenGLProgramsCache().IsUsingLRU() && GetOpenGLProgramsCache().IsEvicted(LinkedProgram->Config.ProgramKey);
		if( !bIsEvicted )
		{
			FOpenGLLinkedProgram* Prog = StaticLastReleasedPrograms[StaticLastReleasedProgramsIndex];
			StaticLastReleasedPrograms[StaticLastReleasedProgramsIndex++] = LinkedProgram;
			if (StaticLastReleasedProgramsIndex == LAST_RELEASED_PROGRAMS_CACHE_COUNT)
			{
				StaticLastReleasedProgramsIndex = 0;
			}

			if (CVarEvictOnBssDestruct.GetValueOnAnyThread() && GetOpenGLProgramsCache().IsUsingLRU())
			{
				FDelayedEvictionContainer::Get().Add(LinkedProgram);
			}

			OnProgramDeletion(LinkedProgram->Program);
		}
	});
}

bool FOpenGLBoundShaderState::NeedsTextureStage(int32 TextureStageIndex)
{
	return LinkedProgram->TextureStageNeeds[TextureStageIndex];
}

int32 FOpenGLBoundShaderState::MaxTextureStageUsed()
{
	return LinkedProgram->MaxTextureStage;
}

const TBitArray<>& FOpenGLBoundShaderState::GetTextureNeeds(int32& OutMaxTextureStageUsed)
{
	OutMaxTextureStageUsed = LinkedProgram->MaxTextureStage;
	return LinkedProgram->TextureStageNeeds;
}

const TBitArray<>& FOpenGLBoundShaderState::GetUAVNeeds(int32& OutMaxUAVUnitUsed) const
{
	OutMaxUAVUnitUsed = LinkedProgram->MaxUAVUnitUsed;
	return LinkedProgram->UAVStageNeeds;
}

void FOpenGLBoundShaderState::GetNumUniformBuffers(int32 NumUniformBuffers[SF_NumGraphicsFrequencies])
{
	if (IsRunningRHIInSeparateThread())
	{
		// fast path, no need to check any fences....
		check(IsInRHIThread());
		check(IsValidRef(VertexShaderProxy) && IsValidRef(PixelShaderProxy));


		NumUniformBuffers[SF_Vertex] = VertexShaderProxy->GetGLResourceObject_OnRHIThread()->Bindings.NumUniformBuffers;
		NumUniformBuffers[SF_Pixel] = PixelShaderProxy->GetGLResourceObject_OnRHIThread()->Bindings.NumUniformBuffers;
		NumUniformBuffers[SF_Geometry] = GeometryShaderProxy ? GeometryShaderProxy->GetGLResourceObject_OnRHIThread()->Bindings.NumUniformBuffers : -1;
	}
	else
	{
		NumUniformBuffers[SF_Vertex] = VertexShaderProxy->GetGLResourceObject()->Bindings.NumUniformBuffers;
		NumUniformBuffers[SF_Pixel] = PixelShaderProxy->GetGLResourceObject()->Bindings.NumUniformBuffers;
		NumUniformBuffers[SF_Geometry] = GeometryShaderProxy ? GeometryShaderProxy->GetGLResourceObject()->Bindings.NumUniformBuffers : -1;
	}
}


bool FOpenGLBoundShaderState::RequiresDriverInstantiation()
{
	check(LinkedProgram);
	bool const bDrawn = LinkedProgram->bDrawn;
	LinkedProgram->bDrawn = true;
	return !bDrawn;
}

bool FOpenGLComputeShader::NeedsTextureStage(int32 TextureStageIndex)
{
	return LinkedProgram->TextureStageNeeds[TextureStageIndex];
}

int32 FOpenGLComputeShader::MaxTextureStageUsed()
{
	return LinkedProgram->MaxTextureStage;
}

const TBitArray<>& FOpenGLComputeShader::GetTextureNeeds(int32& OutMaxTextureStageUsed)
{
	OutMaxTextureStageUsed = LinkedProgram->MaxTextureStage;
	return LinkedProgram->TextureStageNeeds;
}

const TBitArray<>& FOpenGLComputeShader::GetUAVNeeds(int32& OutMaxUAVUnitUsed) const
{
	OutMaxUAVUnitUsed = LinkedProgram->MaxUAVUnitUsed;
	return LinkedProgram->UAVStageNeeds;
}

bool FOpenGLComputeShader::NeedsUAVStage(int32 UAVStageIndex) const
{
	return LinkedProgram->UAVStageNeeds[UAVStageIndex];
}

void FOpenGLDynamicRHI::BindPendingComputeShaderState(FOpenGLContextState& ContextState, FOpenGLComputeShader* ComputeShader)
{
	VERIFY_GL_SCOPE();
	bool ForceUniformBindingUpdate = false;

	GetOpenGLProgramsCache().Touch(ComputeShader->LinkedProgram);

	GLuint PendingProgram = ComputeShader->LinkedProgram->Program;
	if (ContextState.Program != PendingProgram)
	{
		FOpenGL::BindProgramPipeline(PendingProgram);
		ContextState.Program = PendingProgram;
		MarkShaderParameterCachesDirty(PendingState.ShaderParameters, true);
		PendingState.LinkedProgramAndDirtyFlag = nullptr;
		ForceUniformBindingUpdate = true;
	}

	if (PendingState.bAnyDirtyRealUniformBuffers[SF_Compute])
	{
		BindUniformBufferBase(
			ContextState,
			ComputeShader->Bindings.NumUniformBuffers,
			PendingState.BoundUniformBuffers[SF_Compute],
			OGL_FIRST_UNIFORM_BUFFER,
			ForceUniformBindingUpdate);

		PendingState.bAnyDirtyRealUniformBuffers[SF_Compute] = 0;
	}
	SetupBindlessTextures( ContextState, ComputeShader->LinkedProgram->Samplers );
}

/** Constructor. */
FOpenGLShaderParameterCache::FOpenGLShaderParameterCache()
	: GlobalUniformArraySize(-1)
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = 0;
	}
}

void FOpenGLShaderParameterCache::InitializeResources(int32 UniformArraySize)
{
	check(GlobalUniformArraySize == -1);

	// Uniform arrays have to be multiples of float4s.
	UniformArraySize = Align(UniformArraySize,SizeOfFloat4);

	PackedGlobalUniforms[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	PackedUniformsScratch[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);

	FMemory::Memzero(PackedGlobalUniforms[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	FMemory::Memzero(PackedUniformsScratch[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	for (int32 ArrayIndex = 1; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniforms[ArrayIndex] = PackedGlobalUniforms[ArrayIndex - 1] + UniformArraySize;
		PackedUniformsScratch[ArrayIndex] = PackedUniformsScratch[ArrayIndex - 1] + UniformArraySize;
	}
	GlobalUniformArraySize = UniformArraySize;

	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = UniformArraySize / SizeOfFloat4;
	}
}

/** Destructor. */
FOpenGLShaderParameterCache::~FOpenGLShaderParameterCache()
{
	if (GlobalUniformArraySize > 0)
	{
		FMemory::Free(PackedUniformsScratch[0]);
		FMemory::Free(PackedGlobalUniforms[0]);
	}

	FMemory::Memzero(PackedUniformsScratch);
	FMemory::Memzero(PackedGlobalUniforms);

	GlobalUniformArraySize = -1;
}

/**
 * Marks all uniform arrays as dirty.
 */
void FOpenGLShaderParameterCache::MarkAllDirty()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = GlobalUniformArraySize / SizeOfFloat4;
	}
}

/**
 * Set parameter values.
 */
void FOpenGLShaderParameterCache::Set(uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValues)
{
	uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
	check(GlobalUniformArraySize != -1);
	check(BufferIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
	check(ByteOffset + NumBytes <= (uint32)GlobalUniformArraySize);
	PackedGlobalUniformDirty[BufferIndex].MarkDirtyRange(ByteOffset / SizeOfFloat4, (NumBytes + SizeOfFloat4 - 1) / SizeOfFloat4);
	FMemory::Memcpy(PackedGlobalUniforms[BufferIndex] + ByteOffset, NewValues, NumBytes);
}

/**
 * Commit shader parameters to the currently bound program.
 * @param ParameterTable - Information on the bound uniform arrays for the program.
 */


void FOpenGLShaderParameterCache::CommitPackedGlobals(const FOpenGLLinkedProgram* LinkedProgram, int32 Stage)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLUniformCommitTime);
	VERIFY_GL_SCOPE();
	const uint32 BytesPerRegister = 16;

	/**
	 * Note that this always uploads the entire uniform array when it is dirty.
	 * The arrays are marked dirty either when the bound shader state changes or
	 * a value in the array is modified. OpenGL actually caches uniforms per-
	 * program. If we shadowed those per-program uniforms we could avoid calling
	 * glUniform4?v for values that have not changed since the last invocation
	 * of the program.
	 *
	 * It's unclear whether the driver does the same thing and whether there is
	 * a performance benefit. Even if there is, this type of caching makes any
	 * multithreading vastly more difficult, so for now uniforms are not cached
	 * per-program.
	 */
	const TArray<FOpenGLLinkedProgram::FPackedUniformInfo>& PackedUniforms = LinkedProgram->StagePackedUniformInfo[Stage].PackedUniformInfos;
	const TArray<CrossCompiler::FPackedArrayInfo>& PackedArrays = LinkedProgram->Config.Shaders[Stage].Bindings.PackedGlobalArrays;
	for (int32 PackedUniform = 0; PackedUniform < PackedUniforms.Num(); ++PackedUniform)
	{
		const FOpenGLLinkedProgram::FPackedUniformInfo& UniformInfo = PackedUniforms[PackedUniform];
		GLint Location = UniformInfo.Location;
		const uint32 ArrayIndex = UniformInfo.Index;
		if (Location >= 0 && // Probably this uniform array was optimized away in a linked program
			PackedGlobalUniformDirty[ArrayIndex].NumVectors > 0)
		{
			check(ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
			const uint32 NumVectors = PackedArrays[PackedUniform].Size / BytesPerRegister;
			const void* UniformData = PackedGlobalUniforms[ArrayIndex];

			const uint32 StartVector = PackedGlobalUniformDirty[ArrayIndex].StartVector;
			int32 NumDirtyVectors = FMath::Min(PackedGlobalUniformDirty[ArrayIndex].NumVectors, NumVectors - StartVector);
			check(NumDirtyVectors);
			UniformData = (uint8*)UniformData + StartVector * sizeof(float) * 4;
			Location += StartVector;
			switch (UniformInfo.Index)
			{
			case CrossCompiler::PACKED_TYPEINDEX_HIGHP:
			case CrossCompiler::PACKED_TYPEINDEX_MEDIUMP:
			case CrossCompiler::PACKED_TYPEINDEX_LOWP:
				FOpenGL::ProgramUniform4fv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLfloat*)UniformData);
				break;

			case CrossCompiler::PACKED_TYPEINDEX_INT:
				FOpenGL::ProgramUniform4iv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLint*)UniformData);
				break;

			case CrossCompiler::PACKED_TYPEINDEX_UINT:
				FOpenGL::ProgramUniform4uiv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLuint*)UniformData);
				break;
			}

			PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
			PackedGlobalUniformDirty[ArrayIndex].NumVectors = 0;
		}
	}
}

void FOpenGLShaderParameterCache::CommitPackedUniformBuffers(FOpenGLLinkedProgram* LinkedProgram, int32 Stage, FUniformBufferRHIRef* RHIUniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLConstantBufferUpdateTime);
	VERIFY_GL_SCOPE();

	// Uniform Buffers are split into precision/type; the list of RHI UBs is traversed and if a new one was set, its
	// contents are copied per precision/type into corresponding scratch buffers which are then uploaded to the program
	const FOpenGLShaderBindings& Bindings = LinkedProgram->Config.Shaders[Stage].Bindings;
	check(Bindings.NumUniformBuffers <= FOpenGLRHIState::MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE);

	if (Bindings.bFlattenUB)
	{
		int32 LastInfoIndex = 0;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FOpenGLUniformBuffer* UniformBuffer = (FOpenGLUniformBuffer*)RHIUniformBuffers[BufferIndex].GetReference();
			check(UniformBuffer);

			if (!UniformBuffer->bIsEmulatedUniformBuffer)
			{
				continue;
			}

			const uint32* RESTRICT SourceData = UniformBuffer->EmulatedBufferData->Data.GetData();
			for (int32 InfoIndex = LastInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
			{
				const CrossCompiler::FUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
				if (Info.SourceUBIndex == BufferIndex)
				{
					check((Info.DestOffsetInFloats + Info.SizeInFloats) * sizeof(float) <= (uint32)GlobalUniformArraySize);
					float* RESTRICT ScratchMem = (float*)PackedGlobalUniforms[Info.DestUBTypeIndex];
					ScratchMem += Info.DestOffsetInFloats;
					FMemory::Memcpy(ScratchMem, SourceData + Info.SourceOffsetInFloats, Info.SizeInFloats * sizeof(float));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].MarkDirtyRange(Info.DestOffsetInFloats / NumFloatsInFloat4, (Info.SizeInFloats + NumFloatsInFloat4 - 1) / NumFloatsInFloat4);
				}
				else
				{
					LastInfoIndex = InfoIndex;
					break;
				}
			}
		}
	}
	else
	{
		const auto& PackedUniformBufferInfos = LinkedProgram->StagePackedUniformInfo[Stage].PackedUniformBufferInfos;
		int32 LastCopyInfoIndex = 0;
		auto& EmulatedUniformBufferSet = LinkedProgram->StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FOpenGLUniformBuffer* UniformBuffer = (FOpenGLUniformBuffer*)RHIUniformBuffers[BufferIndex].GetReference();

			if (UniformBuffer && !UniformBuffer->bIsEmulatedUniformBuffer)
			{
				continue;
			}

			// Workaround for null UBs (FORT-323429), additional logging here is to give us a chance to investigate the higher level issue causing the null UB.
#if !UE_BUILD_SHIPPING
			UE_CLOG(UniformBuffer == nullptr && EmulatedUniformBufferSet.IsValidIndex(BufferIndex), LogRHI, Fatal, TEXT("CommitPackedUniformBuffers null UB stage %d, idx %d (%d), %s"), Stage, BufferIndex, EmulatedUniformBufferSet.Num(), *LinkedProgram->Config.ProgramKey.ToString());
#endif
			if (UniformBuffer && EmulatedUniformBufferSet.IsValidIndex(BufferIndex) && EmulatedUniformBufferSet[BufferIndex] != UniformBuffer->UniqueID)
			{
				EmulatedUniformBufferSet[BufferIndex] = UniformBuffer->UniqueID;

				// Go through the list of copy commands and perform the appropriate copy into the scratch buffer
				for (int32 InfoIndex = LastCopyInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
				{
					const CrossCompiler::FUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
					if (Info.SourceUBIndex == BufferIndex)
					{
						const uint32* RESTRICT SourceData = UniformBuffer->EmulatedBufferData->Data.GetData();
						SourceData += Info.SourceOffsetInFloats;
						float* RESTRICT ScratchMem = (float*)PackedUniformsScratch[Info.DestUBTypeIndex];
						ScratchMem += Info.DestOffsetInFloats;
						FMemory::Memcpy(ScratchMem, SourceData, Info.SizeInFloats * sizeof(float));
					}
					else if (Info.SourceUBIndex > BufferIndex)
					{
						// Done finding current copies
						LastCopyInfoIndex = InfoIndex;
						break;
					}

					// keep going since we could have skipped this loop when skipping cached UBs...
				}

				// Upload the split buffers to the program
				const auto& UniformBufferUploadInfoList = PackedUniformBufferInfos[BufferIndex];
				for (int32 InfoIndex = 0; InfoIndex < UniformBufferUploadInfoList.Num(); ++InfoIndex)
				{
					auto& UBInfo = Bindings.PackedUniformBuffers[BufferIndex];
					const auto& UniformInfo = UniformBufferUploadInfoList[InfoIndex];
					if (UniformInfo.Location < 0)
					{
						// Optimized out
						continue;
					}
					
					const void* RESTRICT UniformData = PackedUniformsScratch[UniformInfo.Index];
					int32 NumVectors = UBInfo[InfoIndex].Size / SizeOfFloat4;
					check(UniformInfo.ArrayType == UBInfo[InfoIndex].TypeName);
					switch (UniformInfo.Index)
					{
					case CrossCompiler::PACKED_TYPEINDEX_HIGHP:
					case CrossCompiler::PACKED_TYPEINDEX_MEDIUMP:
					case CrossCompiler::PACKED_TYPEINDEX_LOWP:
						FOpenGL::ProgramUniform4fv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLfloat*)UniformData);
						break;

					case CrossCompiler::PACKED_TYPEINDEX_INT:
						FOpenGL::ProgramUniform4iv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLint*)UniformData);
						break;

					case CrossCompiler::PACKED_TYPEINDEX_UINT:
						FOpenGL::ProgramUniform4uiv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLuint*)UniformData);
						break;
					}
				}
			}
		}
	}
}


namespace UE
{
	namespace OpenGL
	{
		// Called from the binary file cache when program loads from disk.
		void OnGLProgramLoadedFromBinaryCache(const FOpenGLProgramKey& ProgramKey, TArray<uint8>&& ProgramBinaryData)
		{
			const bool bProgramExists = GetOpenGLProgramsCache().Find(ProgramKey, false) != nullptr;

			if (GetOpenGLProgramsCache().IsUsingLRU())
			{
				if (!bProgramExists)
				{
					// Always add programs as evicted, 1st use will create them as programs.
					// This will reduce pressure on driver by ensuring only used programs
					// are created.
					// In this case do not create the GL program.
					GetOpenGLProgramsCache().AddAsEvicted(ProgramKey, MoveTemp(ProgramBinaryData));
				}
				else
				{
					// The program is already in use, discard the binary data.
					ProgramBinaryData.Empty();
				}
			}
			else
			{
				if (!bProgramExists)
				{
					GLuint GLProgramId = 0;
					bool bSuccess = UE::OpenGL::CreateGLProgramFromBinary(GLProgramId, ProgramBinaryData);
					if (!bSuccess)
					{
						UE_LOG(LogRHI, Log, TEXT("[%s, %d, %d]"), *ProgramKey.ToString(), GLProgramId, ProgramBinaryData.Num());
						RHIGetPanicDelegate().ExecuteIfBound(FName("FailedBinaryProgramCreateLoadRequest"));
						UE_LOG(LogRHI, Fatal, TEXT("CompleteLoadedGLProgramRequest_internal : Failed to create GL program from binary data! [%s]"), *ProgramKey.ToString());
					}
					FOpenGLLinkedProgram* NewLinkedProgram = new FOpenGLLinkedProgram(ProgramKey, GLProgramId);
					GetOpenGLProgramsCache().Add(ProgramKey, NewLinkedProgram);
					SetNewProgramStats(GLProgramId);
				}

				// Finished with binary data.
				ProgramBinaryData.Empty();
			}
		}
	}
}

void FOpenGLDynamicRHI::EndFrameTick()
{
	FDelayedEvictionContainer::Get().Tick();
	FOpenGLProgramBinaryCache::CheckPendingGLProgramCreateRequests();
	FTextureEvictionLRU::Get().TickEviction();
}

static FDelegateHandle OnSharedShaderCodeRequest;
//static FDelegateHandle OnSharedShaderCodeRelease;

void OnShaderLibraryRequestShaderCode(const FSHAHash& Hash, FArchive* Ar)
{
	FOpenGLProgramBinaryCache::OnShaderLibraryRequestShaderCode(Hash, Ar);
}

//void OnShaderLibraryReleaseShaderCode(const FSHAHash& Hash)
//{
//}


void FOpenGLDynamicRHI::RegisterSharedShaderCodeDelegates()
{
	OnSharedShaderCodeRequest = FShaderCodeLibrary::RegisterSharedShaderCodeRequestDelegate_Handle(FSharedShaderCodeRequest::FDelegate::CreateStatic(&OnShaderLibraryRequestShaderCode));
	//OnSharedShaderCodeRelease = FShaderCodeLibrary::RegisterSharedShaderCodeReleaseDelegate_Handle(FSharedShaderCodeRelease::FDelegate::CreateStatic(&OnShaderLibraryReleaseShaderCode));
}

void FOpenGLDynamicRHI::UnregisterSharedShaderCodeDelegates()
{
	FShaderCodeLibrary::UnregisterSharedShaderCodeRequestDelegate_Handle(OnSharedShaderCodeRequest);
	//FShaderCodeLibrary::UnregisterSharedShaderCodeReleaseDelegate_Handle(OnSharedShaderCodeRelease);
}
