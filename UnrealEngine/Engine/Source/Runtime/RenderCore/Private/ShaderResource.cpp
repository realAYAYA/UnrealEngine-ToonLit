// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderResource.cpp: ShaderResource implementation.
=============================================================================*/

#include "Shader.h"
#include "Misc/CoreMisc.h"
#include "Misc/StringBuilder.h"
#include "Stats/StatsMisc.h"
#include "Serialization/MemoryWriter.h"
#include "VertexFactory.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IShaderFormat.h"
#include "ShaderCodeLibrary.h"
#include "ShaderCore.h"
#include "RenderUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Misc/MemStack.h"
#include "ShaderCompilerCore.h"
#include "Compression/OodleDataCompression.h"

#if WITH_EDITORONLY_DATA
#include "Interfaces/IShaderFormat.h"
#endif

DECLARE_LOG_CATEGORY_CLASS(LogShaderWarnings, Log, Log);

int32 GShaderCompressionFormatChoice = 2;
static FAutoConsoleVariableRef CVarShaderCompressionFormatChoice(
	TEXT("r.Shaders.CompressionFormat"),
	GShaderCompressionFormatChoice,
	TEXT("Select the compression methods for the shader code.\n")
	TEXT(" 0: None (uncompressed)\n")
	TEXT(" 1: LZ4\n")
	TEXT(" 2: Oodle (default)\n")
	TEXT(" 3: ZLib\n"),
	ECVF_ReadOnly);

int32 GShaderCompressionOodleAlgo = 2;
static FAutoConsoleVariableRef CVarShaderCompressionOodleAlgo(
	TEXT("r.Shaders.CompressionFormat.Oodle.Algo"),
	GShaderCompressionOodleAlgo,
	TEXT("Oodle compression method for the shader code, from fastest to slowest to decode.\n")
	TEXT(" 0: None (invalid setting)\n")
	TEXT(" 1: Selkie (fastest to decode)\n")
	TEXT(" 2: Mermaid\n")
	TEXT(" 3: Kraken\n")
	TEXT(" 4: Leviathan (slowest to decode)\n"),
	ECVF_ReadOnly);

int32 GShaderCompressionOodleLevel = 6;
static FAutoConsoleVariableRef CVarShaderCompressionOodleAlgoChoice(
	TEXT("r.Shaders.CompressionFormat.Oodle.Level"),
	GShaderCompressionOodleLevel,
	TEXT("Oodle compression level. This mostly trades encode speed vs compression ratio, decode speed is determined by r.Shaders.CompressionFormat.Oodle.Algo\n")
	TEXT(" -4 : HyperFast4\n")
	TEXT(" -3 : HyperFast3\n")
	TEXT(" -2 : HyperFast2\n")
	TEXT(" -1 : HyperFast1\n")
	TEXT("  0 : None\n")
	TEXT("  1 : SuperFast\n")
	TEXT("  2 : VeryFast\n")
	TEXT("  3 : Fast\n")
	TEXT("  4 : Normal\n")
	TEXT("  5 : Optimal1\n")
	TEXT("  6 : Optimal2\n")
	TEXT("  7 : Optimal3\n")
	TEXT("  8 : Optimal4\n"),
	ECVF_ReadOnly);

static int32 GShaderCompilerEmitWarningsOnLoad = 0;
static FAutoConsoleVariableRef CVarShaderCompilerEmitWarningsOnLoad(
	TEXT("r.ShaderCompiler.EmitWarningsOnLoad"),
	GShaderCompilerEmitWarningsOnLoad,
	TEXT("When 1, shader compiler warnings are emitted to the log for all shaders as they are loaded."),
	ECVF_Default
);

FName GetShaderCompressionFormat(const FName& ShaderFormat)
{
	// support an older developer-only CVar for compatibility and make it preempt
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const IConsoleVariable* CVarSkipCompression = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.SkipCompression"));
	static bool bSkipCompression = (CVarSkipCompression && CVarSkipCompression->GetInt() != 0);
	if (UNLIKELY(bSkipCompression))
	{
		return NAME_None;
	}
#endif

	static FName Formats[]
	{
		NAME_None,
		NAME_LZ4,
		NAME_Oodle,
		NAME_Zlib
	};
	
	//GShaderCompressionFormatChoice = (GShaderCompressionFormatChoice < 0) ? 0 : GShaderCompressionFormatChoice;
	GShaderCompressionFormatChoice = FMath::Clamp<int32>(GShaderCompressionFormatChoice, 0, UE_ARRAY_COUNT(Formats) - 1);
	return Formats[GShaderCompressionFormatChoice];
}

void GetShaderCompressionOodleSettings(FOodleDataCompression::ECompressor& OutCompressor, FOodleDataCompression::ECompressionLevel& OutLevel, const FName& ShaderFormat)
{
	// support an older developer-only CVar for compatibility and make it preempt
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const IConsoleVariable* CVarSkipCompression = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.SkipCompression"));
	static bool bSkipCompression = (CVarSkipCompression && CVarSkipCompression->GetInt() != 0);
	if (UNLIKELY(bSkipCompression))
	{
		OutCompressor = FOodleDataCompression::ECompressor::Selkie;
		OutLevel = FOodleDataCompression::ECompressionLevel::None;
		return;
	}
#endif

	GShaderCompressionOodleAlgo = FMath::Clamp(GShaderCompressionOodleAlgo, static_cast<int32>(FOodleDataCompression::ECompressor::NotSet), static_cast<int32>(FOodleDataCompression::ECompressor::Leviathan));
	OutCompressor = static_cast<FOodleDataCompression::ECompressor>(GShaderCompressionOodleAlgo);

	GShaderCompressionOodleLevel = FMath::Clamp(GShaderCompressionOodleLevel, static_cast<int32>(FOodleDataCompression::ECompressionLevel::HyperFast4), static_cast<int32>(FOodleDataCompression::ECompressionLevel::Optimal4));
	OutLevel = static_cast<FOodleDataCompression::ECompressionLevel>(GShaderCompressionOodleLevel);
}

bool FShaderMapResource::ArePlatformsCompatible(EShaderPlatform CurrentPlatform, EShaderPlatform TargetPlatform)
{
	bool bFeatureLevelCompatible = CurrentPlatform == TargetPlatform;

	if (!bFeatureLevelCompatible && IsPCPlatform(CurrentPlatform) && IsPCPlatform(TargetPlatform))
	{
		bFeatureLevelCompatible = GetMaxSupportedFeatureLevel(CurrentPlatform) >= GetMaxSupportedFeatureLevel(TargetPlatform);

		bool const bIsTargetD3D = IsD3DPlatform(TargetPlatform);

		bool const bIsCurrentPlatformD3D = IsD3DPlatform(CurrentPlatform);

		// For Metal in Editor we can switch feature-levels, but not in cooked projects when using Metal shader librariss.
		bool const bIsCurrentMetal = IsMetalPlatform(CurrentPlatform);
		bool const bIsTargetMetal = IsMetalPlatform(TargetPlatform);
		bool const bIsMetalCompatible = (bIsCurrentMetal == bIsTargetMetal)
#if !WITH_EDITOR	// Static analysis doesn't like (|| WITH_EDITOR)
			&& (!IsMetalPlatform(CurrentPlatform) || (CurrentPlatform == TargetPlatform))
#endif
			;

		bool const bIsCurrentOpenGL = IsOpenGLPlatform(CurrentPlatform);
		bool const bIsTargetOpenGL = IsOpenGLPlatform(TargetPlatform);

		bFeatureLevelCompatible = bFeatureLevelCompatible && (bIsCurrentPlatformD3D == bIsTargetD3D && bIsMetalCompatible && bIsCurrentOpenGL == bIsTargetOpenGL);
	}

	return bFeatureLevelCompatible;
}

#if RHI_RAYTRACING
class FRayTracingShaderLibrary
{
public:
	uint32 AddShader(FRHIRayTracingShader* Shader)
	{
		FScopeLock Lock(&CS);

		if (UnusedIndicies.Num() != 0)
		{
			uint32 Index = UnusedIndicies.Pop(false);
			checkSlow(Shaders[Index] == nullptr);
			Shaders[Index] = Shader;
			return Index;
		}
		else
		{
			Shaders.Add(Shader);
			return Shaders.Num() - 1;
		}
	}

	void RemoveShader(uint32 Index)
	{
		if (Index != ~0u)
		{
			FScopeLock Lock(&CS);
			UnusedIndicies.Push(Index);
			Shaders[Index] = nullptr;
		}
	}

	void GetShaders(TArray<FRHIRayTracingShader*>& OutShaders, FRHIRayTracingShader* DefaultShader)
	{
		FScopeLock Lock(&CS);
		OutShaders = Shaders;

		for (uint32 Index : UnusedIndicies)
		{
			OutShaders[Index] = DefaultShader;
		}
	}

private:
	TArray<uint32> UnusedIndicies;
	TArray<FRHIRayTracingShader*> Shaders;
	FCriticalSection CS;
};

static FRayTracingShaderLibrary GlobalRayTracingHitGroupLibrary;
static FRayTracingShaderLibrary GlobalRayTracingCallableShaderLibrary;
static FRayTracingShaderLibrary GlobalRayTracingMissShaderLibrary;

void FShaderMapResource::GetRayTracingHitGroupLibrary(TArray<FRHIRayTracingShader*>& RayTracingShaders, FRHIRayTracingShader* DefaultShader)
{
	GlobalRayTracingHitGroupLibrary.GetShaders(RayTracingShaders, DefaultShader);
}

void FShaderMapResource::GetRayTracingCallableShaderLibrary(TArray<FRHIRayTracingShader*>& RayTracingCallableShaders, FRHIRayTracingShader* DefaultShader)
{
	GlobalRayTracingCallableShaderLibrary.GetShaders(RayTracingCallableShaders, DefaultShader);
}

void FShaderMapResource::GetRayTracingMissShaderLibrary(TArray<FRHIRayTracingShader*>& RayTracingMissShaders, FRHIRayTracingShader* DefaultShader)
{
	GlobalRayTracingMissShaderLibrary.GetShaders(RayTracingMissShaders, DefaultShader);
}
#endif // RHI_RAYTRACING

static void ApplyResourceStats(FShaderMapResourceCode& Resource)
{
#if STATS
	INC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, Resource.GetSizeBytes());
	for (const FShaderMapResourceCode::FShaderEntry& Shader : Resource.ShaderEntries)
	{
		INC_DWORD_STAT_BY_FName(GetMemoryStatType(Shader.Frequency).GetName(), Shader.Code.Num());
	}
#endif // STATS
}

static void RemoveResourceStats(FShaderMapResourceCode& Resource)
{
#if STATS
	DEC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, Resource.GetSizeBytes());
	for (const FShaderMapResourceCode::FShaderEntry& Shader : Resource.ShaderEntries)
	{
		DEC_DWORD_STAT_BY_FName(GetMemoryStatType(Shader.Frequency).GetName(), Shader.Code.Num());
	}
#endif // STATS
}

FShaderMapResourceCode::FShaderMapResourceCode(const FShaderMapResourceCode& Other)
{
	ResourceHash = Other.ResourceHash;
	ShaderHashes = Other.ShaderHashes;
	ShaderEntries = Other.ShaderEntries;

#if WITH_EDITORONLY_DATA
	PlatformDebugData = Other.PlatformDebugData;
	PlatformDebugDataHashes = Other.PlatformDebugDataHashes;
#endif // WITH_EDITORONLY_DATA
}

FShaderMapResourceCode::~FShaderMapResourceCode()
{
	RemoveResourceStats(*this);
}

void FShaderMapResourceCode::Finalize()
{
	FSHA1 Hasher;
	Hasher.Update((uint8*)ShaderHashes.GetData(), ShaderHashes.Num() * sizeof(FSHAHash));
	Hasher.Final();
	Hasher.GetHash(ResourceHash.Hash);
	ApplyResourceStats(*this);

#if WITH_EDITORONLY_DATA
	LogShaderCompilerWarnings();
#endif
}

uint32 FShaderMapResourceCode::GetSizeBytes() const
{
	uint64 Size = sizeof(*this) + ShaderHashes.GetAllocatedSize() + ShaderEntries.GetAllocatedSize();
	for (const FShaderEntry& Entry : ShaderEntries)
	{
		Size += Entry.Code.GetAllocatedSize();
	}
	check(Size <= TNumericLimits<uint32>::Max());
	return static_cast<uint32>(Size);
}

int32 FShaderMapResourceCode::FindShaderIndex(const FSHAHash& InHash) const
{
	return Algo::BinarySearch(ShaderHashes, InHash);
}

void FShaderMapResourceCode::AddShaderCompilerOutput(const FShaderCompilerOutput& Output)
{
#if WITH_EDITORONLY_DATA
	AddPlatformDebugData(Output.PlatformDebugData);

	for (const FShaderCompilerError& Error : Output.Errors)
	{
		CompilerWarnings.Add(Error.GetErrorString());
	}
#endif
	AddShaderCode(Output.Target.GetFrequency(), Output.OutputHash, Output.ShaderCode);
}

void FShaderMapResourceCode::AddShaderCode(EShaderFrequency InFrequency, const FSHAHash& InHash, const FShaderCode& InCode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderMapResourceCode::AddShaderCode);

	const int32 Index = Algo::LowerBound(ShaderHashes, InHash);
	if (Index >= ShaderHashes.Num() || ShaderHashes[Index] != InHash)
	{
		ShaderHashes.Insert(InHash, Index);

		FShaderEntry& Entry = ShaderEntries.InsertDefaulted_GetRef(Index);
		Entry.Frequency = InFrequency;
		const TArray<uint8>& ShaderCode = InCode.GetReadAccess();

		FName ShaderCompressionFormat = GetShaderCompressionFormat();
		if (ShaderCompressionFormat != NAME_None)
		{
			Entry.UncompressedSize = InCode.GetUncompressedSize();

			// we trust that SCWs also obeyed by the same CVar, so we expect a compressed shader code at this point
			// However, if we see an uncompressed shader, it perhaps means that SCW tried to compress it, but the result was worse than uncompressed. 
			// Because of that we special-case NAME_None here
			if (ShaderCompressionFormat != InCode.GetCompressionFormat())
			{
				if (InCode.GetCompressionFormat() != NAME_None)
				{
					UE_LOG(LogShaders, Fatal, TEXT("Shader %s is expected to be compressed with %s, but it is compressed with %s instead."),
						*InHash.ToString(),
						*ShaderCompressionFormat.ToString(),
						*InCode.GetCompressionFormat().ToString()
						);
					// unreachable
					return;
				}
				
				// assume uncompressed due to worse ratio than the compression
				Entry.UncompressedSize = ShaderCode.Num();
				UE_LOG(LogShaders, Verbose, TEXT("Shader %s is expected to be compressed with %s, but it arrived uncompressed (size=%d). Assuming compressing made it longer and storing uncompressed."),
					*InHash.ToString(),
					*ShaderCompressionFormat.ToString(),
					ShaderCode.Num()
				);
			}
			else if (ShaderCompressionFormat == NAME_Oodle)
			{
				// check if Oodle-specific settings match
				FOodleDataCompression::ECompressor OodleCompressor;
				FOodleDataCompression::ECompressionLevel OodleLevel;
				GetShaderCompressionOodleSettings(OodleCompressor, OodleLevel);

				if (InCode.GetOodleCompressor() != OodleCompressor || InCode.GetOodleLevel() != OodleLevel)
				{
					UE_LOG(LogShaders, Fatal, TEXT("Shader %s is expected to be compressed with Oodle compressor %d level %d, but it is compressed with compressor %d level %d instead."),
						*InHash.ToString(),
						static_cast<int32>(OodleCompressor),
						static_cast<int32>(OodleLevel),
						static_cast<int32>(InCode.GetOodleCompressor()),
						static_cast<int32>(InCode.GetOodleLevel())
					);
					// unreachable
					return;
				}
			}
		}
		else
		{
			Entry.UncompressedSize = ShaderCode.Num();
		}

		Entry.Code = ShaderCode;
	}
}

#if WITH_EDITORONLY_DATA
void FShaderMapResourceCode::AddPlatformDebugData(TConstArrayView<uint8> InPlatformDebugData)
{
	if (InPlatformDebugData.Num() == 0)
	{
		return;
	}

	FSHAHash Hash;
	{
		FSHA1 Hasher;
		Hasher.Update(InPlatformDebugData.GetData(), InPlatformDebugData.Num());
		Hasher.Final();
		Hasher.GetHash(Hash.Hash);
	}

	const int32 Index = Algo::LowerBound(PlatformDebugDataHashes, Hash);
	if (Index >= PlatformDebugDataHashes.Num() || PlatformDebugDataHashes[Index] != Hash)
	{
		PlatformDebugDataHashes.Insert(Hash, Index);
		PlatformDebugData.EmplaceAt(Index, InPlatformDebugData.GetData(), InPlatformDebugData.Num());
	}
}

void FShaderMapResourceCode::LogShaderCompilerWarnings()
{
	if (CompilerWarnings.Num() > 0 && GShaderCompilerEmitWarningsOnLoad != 0)
	{
		// Emit all the compiler warnings seen whilst serializing/loading this shader to the log.
		// Since successfully compiled shaders are stored in the DDC, we'll get the compiler warnings
		// even if we didn't compile the shader this run.
		for (const FString& CompilerWarning : CompilerWarnings)
		{
			UE_LOG(LogShaderWarnings, Warning, TEXT("%s"), *CompilerWarning);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void FShaderMapResourceCode::ToString(FStringBuilderBase& OutString) const
{
	OutString.Appendf(TEXT("Shaders: Num=%d\n"), ShaderHashes.Num());
	for (int32 i = 0; i < ShaderHashes.Num(); ++i)
	{
		const FShaderEntry& Entry = ShaderEntries[i];
		OutString.Appendf(TEXT("    [%d]: { Hash: %s, Freq: %s, Size: %d, UncompressedSize: %d }\n"),
			i, *ShaderHashes[i].ToString(), GetShaderFrequencyString(Entry.Frequency), Entry.Code.Num(), Entry.UncompressedSize);
	}
}

void FShaderMapResourceCode::Serialize(FArchive& Ar, bool bLoadedByCookedMaterial)
{
	Ar << ResourceHash;
	Ar << ShaderHashes;
	Ar << ShaderEntries;
	check(ShaderEntries.Num() == ShaderHashes.Num());
#if WITH_EDITORONLY_DATA
	const bool bSerializeEditorOnlyData = !bLoadedByCookedMaterial && (!Ar.IsCooking() || Ar.CookingTarget()->HasEditorOnlyData());
	if (bSerializeEditorOnlyData)
	{
		Ar << PlatformDebugDataHashes;
		Ar << PlatformDebugData;
		Ar << CompilerWarnings;
	}
#endif // WITH_EDITORONLY_DATA
	ApplyResourceStats(*this);

#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading())
	{
		LogShaderCompilerWarnings();
	}
#endif
}

#if WITH_EDITORONLY_DATA
void FShaderMapResourceCode::NotifyShadersCompiled(FName FormatName)
{
#if WITH_ENGINE
	// Notify the platform shader format that this particular shader is being used in the cook.
	// We discard this data in cooked builds unless Ar.CookingTarget()->HasEditorOnlyData() is true.
	if (PlatformDebugData.Num())
	{
		if (const IShaderFormat* ShaderFormat = GetTargetPlatformManagerRef().FindShaderFormat(FormatName))
		{
			for (const TArray<uint8>& Entry : PlatformDebugData)
			{
				ShaderFormat->NotifyShaderCompiled(Entry, FormatName);
			}
		}
	}
#endif // WITH_ENGINE
}

void FShaderMapResourceCode::NotifyShadersCooked(const ITargetPlatform* TargetPlatform)
{
#if WITH_ENGINE
	TArray<FName> ShaderFormatNames;
	TargetPlatform->GetAllTargetedShaderFormats(ShaderFormatNames);
	for (FName FormatName : ShaderFormatNames)
	{
		NotifyShadersCompiled(FormatName);
	}
#endif
}
#endif // WITH_EDITORONLY_DATA

FShaderMapResource::FShaderMapResource(EShaderPlatform InPlatform, int32 NumShaders)
	: NumRHIShaders(NumShaders)
	, Platform(InPlatform)
	, NumRefs(0)
{
	RHIShaders = MakeUnique<std::atomic<FRHIShader*>[]>(NumRHIShaders); // this MakeUnique() zero-initializes the array
#if RHI_RAYTRACING
	if (GRHISupportsRayTracing && GRHISupportsRayTracingShaders)
	{
		RayTracingLibraryIndices.AddUninitialized(NumShaders);
		FMemory::Memset(RayTracingLibraryIndices.GetData(), 0xff, NumShaders * RayTracingLibraryIndices.GetTypeSize());
	}
#endif // RHI_RAYTRACING
}

FShaderMapResource::~FShaderMapResource()
{
	ReleaseShaders();
	check(NumRefs.load(std::memory_order_relaxed) == 0);
}

void FShaderMapResource::AddRef()
{
	NumRefs.fetch_add(1, std::memory_order_relaxed);
}

void FShaderMapResource::Release()
{
	check(NumRefs.load(std::memory_order_relaxed) > 0);
	if (NumRefs.fetch_sub(1, std::memory_order_release) - 1 == 0 && TryRelease())
	{
		//check https://www.boost.org/doc/libs/1_55_0/doc/html/atomic/usage_examples.html for explanation
		std::atomic_thread_fence(std::memory_order_acquire);
		// Send a release message to the rendering thread when the shader loses its last reference.
		BeginReleaseResource(this);
		BeginCleanup(this);

		DEC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, GetSizeBytes());
	}
}

void FShaderMapResource::ReleaseShaders()
{
	if (RHIShaders)
	{
		for (int32 Idx = 0; Idx < NumRHIShaders; ++Idx)
		{
			if (FRHIShader* Shader = RHIShaders[Idx].load(std::memory_order_acquire))
			{
				Shader->Release();
				DEC_DWORD_STAT(STAT_Shaders_NumShadersUsedForRendering);
			}
		}
		RHIShaders = nullptr;
		NumRHIShaders = 0;
	}
}


void FShaderMapResource::ReleaseRHI()
{
#if RHI_RAYTRACING
	if (GRHISupportsRayTracing && GRHISupportsRayTracingShaders)
	{
		check(NumRHIShaders == RayTracingLibraryIndices.Num());

		for (int32 Idx = 0; Idx < NumRHIShaders; ++Idx)
		{
			if (FRHIShader* Shader = RHIShaders[Idx].load(std::memory_order_acquire))
			{
				int32 IndexInLibrary = RayTracingLibraryIndices[Idx];
				switch (Shader->GetFrequency())
				{
				case SF_RayHitGroup:
					GlobalRayTracingHitGroupLibrary.RemoveShader(IndexInLibrary);
					break;
				case SF_RayCallable:
					GlobalRayTracingCallableShaderLibrary.RemoveShader(IndexInLibrary);
					break;
				case SF_RayMiss:
					GlobalRayTracingMissShaderLibrary.RemoveShader(IndexInLibrary);
					break;
				default:
					break;
				}
			}
		}
	}
	RayTracingLibraryIndices.Empty();
#endif // RHI_RAYTRACING

	ReleaseShaders();
}

void FShaderMapResource::BeginCreateAllShaders()
{
	FShaderMapResource* Resource = this;
	ENQUEUE_RENDER_COMMAND(InitCommand)(
		[Resource](FRHICommandListImmediate& RHICmdList)
	{
		for (int32 ShaderIndex = 0; ShaderIndex < Resource->GetNumShaders(); ++ShaderIndex)
		{
			Resource->GetShader(ShaderIndex);
		}
	});
}

FRHIShader* FShaderMapResource::CreateShader(int32 ShaderIndex)
{	
	check(!RHIShaders[ShaderIndex].load(std::memory_order_acquire));

	TRefCountPtr<FRHIShader> RHIShader = CreateRHIShader(ShaderIndex);
#if RHI_RAYTRACING
	if (GRHISupportsRayTracing && GRHISupportsRayTracingShaders && RHIShader.IsValid())
	{
		switch (RHIShader->GetFrequency())
		{
		case SF_RayHitGroup:
			RayTracingLibraryIndices[ShaderIndex] = GlobalRayTracingHitGroupLibrary.AddShader(static_cast<FRHIRayTracingShader*>(RHIShader.GetReference()));
			break;
		case SF_RayCallable:
			RayTracingLibraryIndices[ShaderIndex] = GlobalRayTracingCallableShaderLibrary.AddShader(static_cast<FRHIRayTracingShader*>(RHIShader.GetReference()));
			break;
		case SF_RayMiss:
			RayTracingLibraryIndices[ShaderIndex] = GlobalRayTracingMissShaderLibrary.AddShader(static_cast<FRHIRayTracingShader*>(RHIShader.GetReference()));
			break;
		default:
			break;
		}
	}
#endif // RHI_RAYTRACING

	// keep the reference alive (the caller will release)
	if (RHIShader.IsValid())
	{
		RHIShader->AddRef();
	}
	return RHIShader.GetReference();
}

TRefCountPtr<FRHIShader> FShaderMapResource_InlineCode::CreateRHIShader(int32 ShaderIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderMapResource_InlineCode::CreateRHIShader);
#if STATS
	double TimeFunctionEntered = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		if (IsInRenderingThread())
		{
			double ShaderCreationTime = FPlatformTime::Seconds() - TimeFunctionEntered;
			INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime, ShaderCreationTime);
		}
	};
#endif

	// we can't have this called on the wrong platform's shaders
	if (!ArePlatformsCompatible(GMaxRHIShaderPlatform, GetPlatform()))
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogShaders, Fatal, TEXT("FShaderMapResource_InlineCode::InitRHI got platform %s but it is not compatible with %s"),
				*LegacyShaderPlatformToShaderFormat(GetPlatform()).ToString(), *LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString());
		}
		return TRefCountPtr<FRHIShader>();
	}

	FMemStackBase& MemStack = FMemStack::Get();
	const FShaderMapResourceCode::FShaderEntry& ShaderEntry = Code->ShaderEntries[ShaderIndex];
	const uint8* ShaderCode = ShaderEntry.Code.GetData();

	FMemMark Mark(MemStack);
	if (ShaderEntry.Code.Num() != ShaderEntry.UncompressedSize)
	{
		void* UncompressedCode = MemStack.Alloc(ShaderEntry.UncompressedSize, 16);
		bool bSucceed = FCompression::UncompressMemory(GetShaderCompressionFormat(), UncompressedCode, ShaderEntry.UncompressedSize, ShaderCode, ShaderEntry.Code.Num());
		check(bSucceed);
		ShaderCode = (uint8*)UncompressedCode;
	}

	const auto ShaderCodeView = MakeArrayView(ShaderCode, ShaderEntry.UncompressedSize);
	const FSHAHash& ShaderHash = Code->ShaderHashes[ShaderIndex];
	const EShaderFrequency Frequency = ShaderEntry.Frequency;

	TRefCountPtr<FRHIShader> RHIShader;
	switch (Frequency)
	{
	case SF_Vertex: RHIShader = RHICreateVertexShader(ShaderCodeView, ShaderHash); break;
	case SF_Mesh: RHIShader = RHICreateMeshShader(ShaderCodeView, ShaderHash); break;
	case SF_Amplification: RHIShader = RHICreateAmplificationShader(ShaderCodeView, ShaderHash); break;
	case SF_Pixel: RHIShader = RHICreatePixelShader(ShaderCodeView, ShaderHash); break;
	case SF_Geometry: RHIShader = RHICreateGeometryShader(ShaderCodeView, ShaderHash); break;
	case SF_Compute: RHIShader = RHICreateComputeShader(ShaderCodeView, ShaderHash); break;
	case SF_RayGen: case SF_RayMiss: case SF_RayHitGroup: case SF_RayCallable:
#if RHI_RAYTRACING
		if (GRHISupportsRayTracing && GRHISupportsRayTracingShaders)
		{
			RHIShader = RHICreateRayTracingShader(ShaderCodeView, ShaderHash, Frequency);
		}
#endif // RHI_RAYTRACING
		break;
	default:
		checkNoEntry();
		break;
	}

	if (RHIShader)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShadersUsedForRendering);
		RHIShader->SetHash(ShaderHash);
	}
	return RHIShader;
}
