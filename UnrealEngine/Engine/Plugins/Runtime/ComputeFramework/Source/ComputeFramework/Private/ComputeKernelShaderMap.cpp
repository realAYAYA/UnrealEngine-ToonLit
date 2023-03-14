// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeKernelShader.h"

#include "ComputeFramework/ComputeKernelShared.h"
#include "ComputeFramework/ComputeKernelDerivedDataVersion.h"
#include "ComputeFramework/ComputeKernelShaderCompilationManager.h"
#include "ComputeFramework/ComputeFramework.h"
#include "DerivedDataCacheInterface.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "RHIShaderFormatDefinitions.inl"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "ShaderCompiler.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectGlobals.h"


#if ENABLE_COOK_STATS
namespace ComputeKernelShaderCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("ComputeKernelShader.Usage"), TEXT(""));
		AddStat(TEXT("ComputeKernelShader.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
		));
	});
}
#endif


//
// Globals
//
TMap<FComputeKernelShaderMapId, FComputeKernelShaderMap*> FComputeKernelShaderMap::GIdToComputeKernelShaderMap[SP_NumPlatforms];
TArray<FComputeKernelShaderMap*> FComputeKernelShaderMap::AllComputeKernelShaderMaps;

// The Id of 0 is reserved for global shaders
uint32 FComputeKernelShaderMap::NextCompilingId = 2;


/** 
 * Tracks FComputeKernelResource and their shader maps that are being compiled.
 * Uses a TRefCountPtr as this will be the only reference to a shader map while it is being compiled.
 */
TMap<TRefCountPtr<FComputeKernelShaderMap>, TArray<FComputeKernelResource*> > FComputeKernelShaderMap::ComputeKernelShaderMapsBeingCompiled;



static inline bool ShouldCacheComputeKernelShader(const FComputeKernelShaderType* InShaderType, EShaderPlatform InPlatform, const FComputeKernelResource* InKernel)
{
	return InShaderType->ShouldCache(InPlatform, InKernel) && InKernel->ShouldCache(InPlatform, InShaderType);
}


/** Hashes the kernel specific part of this shader map Id. */
void FComputeKernelShaderMapId::GetComputeKernelHash(FSHAHash& OutHash) const
{
	FSHA1 HashState;

	HashState.Update((const uint8*)&ShaderCodeHash, sizeof(ShaderCodeHash));
	HashState.Update((const uint8*)&FeatureLevel, sizeof(FeatureLevel));

	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}

/** 
* Tests this set against another for equality, disregarding override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				true if the sets are equal
*/
bool FComputeKernelShaderMapId::operator==(const FComputeKernelShaderMapId& InReferenceSet) const
{
	if (  ShaderCodeHash != InReferenceSet.ShaderCodeHash
		|| FeatureLevel != InReferenceSet.FeatureLevel)
	{
		return false;
	}

	if (ShaderTypeDependencies.Num() != InReferenceSet.ShaderTypeDependencies.Num())
	{
		return false;
	}

	if (LayoutParams != InReferenceSet.LayoutParams)
	{
		return false;
	}

	for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
	{
		const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];

		if (ShaderTypeDependency != InReferenceSet.ShaderTypeDependencies[ShaderIndex])
		{
			return false;
		}
	}

	return true;
}

void FComputeKernelShaderMapId::AppendKeyString(FString& OutKeyString) const
{
#if WITH_EDITOR
	OutKeyString += FString::Printf(TEXT("%llX"), ShaderCodeHash);
	OutKeyString += TEXT("_");

	FString FeatureLevelString;
	GetFeatureLevelName(FeatureLevel, FeatureLevelString);

	{
		const FSHAHash LayoutHash = Freeze::HashLayout(StaticGetTypeLayoutDesc<FComputeKernelShaderMapContent>(), LayoutParams);
		OutKeyString += TEXT("_");
		OutKeyString += LayoutHash.ToString();
		OutKeyString += TEXT("_");
	}

	TSortedMap<const TCHAR*, FCachedUniformBufferDeclaration, FDefaultAllocator, FUniformBufferNameSortOrder> ReferencedUniformBuffers;

	// Add the inputs for any shaders that are stored inline in the shader map
	for (const FShaderTypeDependency& ShaderTypeDependency : ShaderTypeDependencies)
	{
		const FShaderType* ShaderType = FindShaderTypeByName(ShaderTypeDependency.ShaderTypeName);
		OutKeyString += TEXT("_");
		OutKeyString += ShaderType->GetName();
		OutKeyString += ShaderTypeDependency.SourceHash.ToString();
		
		const FSHAHash LayoutHash = Freeze::HashLayout(ShaderType->GetLayout(), LayoutParams);
		OutKeyString += LayoutHash.ToString();

		const TMap<const TCHAR*, FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = ShaderType->GetReferencedUniformBufferStructsCache();

		for (TMap<const TCHAR*, FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
		{
			ReferencedUniformBuffers.Add(It.Key(), It.Value());
		}
	}

	{
		TArray<uint8> TempData;
		FSerializationHistory SerializationHistory;
		FMemoryWriter Ar(TempData, true);
		FShaderSaveArchive SaveArchive(Ar, SerializationHistory);

		// Save uniform buffer member info so we can detect when layout has changed
		SerializeUniformBufferInfo(SaveArchive, ReferencedUniformBuffers);

		SerializationHistory.AppendKeyString(OutKeyString);
	}
#endif //WITH_EDITOR
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param InKernel - The kernel to link the shader with.
 */
void FComputeKernelShaderType::BeginCompileShader(
	uint32 InShaderMapId,
	int32 PermutationId,
	const FComputeKernelResource* InKernel,
	FSharedShaderCompilerEnvironment* InCompilationEnvironment,
	EShaderPlatform InPlatform,
	TArray<FShaderCommonCompileJobPtr>& NewJobs,
	FShaderTarget InTarget
	)
{
	FShaderCompileJob* NewJob = GShaderCompilingManager->PrepareShaderCompileJob(
		InShaderMapId, 
		FShaderCompileJobKey(this, nullptr, PermutationId),
		EShaderCompileJobPriority::Normal
		);

	FString virtualSourcePath = FString::Printf(TEXT("/Engine/Generated/ComputeFramework/Kernel_%s.usf"), *InKernel->GetEntryPoint());

	NewJob->ShaderParameters = MakeShared<const FParameters, ESPMode::ThreadSafe>(*InKernel->GetShaderParamMetadata());
	NewJob->Input.SharedEnvironment = InCompilationEnvironment;
	NewJob->Input.Target = InTarget;
	NewJob->Input.ShaderFormat = LegacyShaderPlatformToShaderFormat(InPlatform);
	NewJob->Input.VirtualSourceFilePath = virtualSourcePath;
	NewJob->Input.EntryPointName = InKernel->GetEntryPoint();
	NewJob->Input.Environment.IncludeVirtualPathToContentsMap.Add(virtualSourcePath, InKernel->GetHLSLSource());
	UE_LOG(LogComputeFramework, Verbose, TEXT("%s"), *InKernel->GetHLSLSource());
	
	AddReferencedUniformBufferIncludes(NewJob->Input.Environment, NewJob->Input.SourceFilePrefix, InPlatform);

	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogComputeFramework, Verbose, TEXT("			%s"), GetName());
	COOK_STAT(ComputeKernelShaderCookStats::ShadersCompiled++);

	InKernel->SetupCompileEnvironment(PermutationId, ShaderEnvironment);

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(InPlatform, InKernel, ShaderEnvironment);

	::GlobalBeginCompileShader(
		InKernel->GetFriendlyName(),
		nullptr,
		this,
		nullptr,//ShaderPipeline,
		PermutationId,
		*virtualSourcePath,
		*InKernel->GetEntryPoint(),
		FShaderTarget(GetFrequency(), InPlatform),
		NewJob->Input
		);

	NewJobs.Add(FShaderCommonCompileJobPtr(NewJob));
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param InShaderMapHash - Precomputed hash of the shader map 
 * @param InCurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FComputeKernelShaderType::FinishCompileShader(
	const FSHAHash& InShaderMapHash,
	const FShaderCompileJob& InCurrentJob,
	const FString& InDebugDescription
	) const
{
	check(InCurrentJob.bSucceeded);

	FShader* Shader = ConstructCompiled(
		FComputeKernelShaderType::CompiledShaderInitializerType(
			this,
			static_cast<const FParameters*>(InCurrentJob.ShaderParameters.Get()),
			InCurrentJob.Key.PermutationId,
			InCurrentJob.Output,
			InShaderMapHash,
			InDebugDescription
			)
		);

	InCurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), InCurrentJob.Output.Target, InCurrentJob.Key.VFType);

	return Shader;
}

/**
 * Finds the shader map for a kernel.
 * @param InShaderMapId - The kernel id and static parameter set identifying the shader map
 * @param InPlatform - The platform to lookup for
 * @return nullptr if no cached shader map was found.
 */
FComputeKernelShaderMap* FComputeKernelShaderMap::FindId(const FComputeKernelShaderMapId& InShaderMapId, EShaderPlatform InPlatform)
{
	FComputeKernelShaderMap* Result = GIdToComputeKernelShaderMap[InPlatform].FindRef(InShaderMapId);
	check(Result == nullptr || !Result->bDeletedThroughDeferredCleanup);
	return Result;
}

/** Creates a string key for the derived data cache given a shader map id. */
static FString GetComputeKernelShaderMapKeyString(const FComputeKernelShaderMapId& InShaderMapId, EShaderPlatform InPlatform)
{
#if WITH_EDITOR
	const FName Format = LegacyShaderPlatformToShaderFormat(InPlatform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	ShaderMapAppendKeyString(InPlatform, ShaderMapKeyString);
	InShaderMapId.AppendKeyString(ShaderMapKeyString);
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("CKERSM"), COMPUTEKERNEL_DERIVEDDATA_VER, *ShaderMapKeyString);
#else
	return FString();
#endif
}

void FComputeKernelShaderMap::LoadFromDerivedDataCache(const FComputeKernelResource* InKernel, const FComputeKernelShaderMapId& InShaderMapId, EShaderPlatform InPlatform, TRefCountPtr<FComputeKernelShaderMap>& InOutShaderMap)
{
#if WITH_EDITOR
	if (InOutShaderMap != nullptr)
	{
		check(InOutShaderMap->GetShaderPlatform() == InPlatform);
		// If the shader map was non-NULL then it was found in memory but is incomplete, attempt to load the missing entries from memory
		InOutShaderMap->LoadMissingShadersFromMemory(InKernel);
	}
	else
	{
		// Shader map was not found in memory, try to load it from the DDC
		STAT(double ComputeKernelShaderDDCTime = 0);
		{
			SCOPE_SECONDS_COUNTER(ComputeKernelShaderDDCTime);
			COOK_STAT(auto Timer = ComputeKernelShaderCookStats::UsageStats.TimeSyncWork());

			TArray<uint8> CachedData;
			const FString DataKey = GetComputeKernelShaderMapKeyString(InShaderMapId, InPlatform);

			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData, InKernel->GetFriendlyName()))
			{
				COOK_STAT(Timer.AddHit(CachedData.Num()));
				InOutShaderMap = new FComputeKernelShaderMap();
				FMemoryReader Ar(CachedData, true);

				// Deserialize from the cached data
				InOutShaderMap->Serialize(Ar);
				//InOutShaderMap->RegisterSerializedShaders(false);

				checkSlow(InOutShaderMap->GetShaderMapId() == InShaderMapId);

				// Register in the global map
				InOutShaderMap->Register(InPlatform);
			}
			else
			{
				// We should be build the data later, and we can track that the resource was built there when we push it to the DDC.
				COOK_STAT(Timer.TrackCyclesOnly());
				InOutShaderMap = nullptr;
			}
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_DDCLoading,(float)ComputeKernelShaderDDCTime);
	}
#endif
}

void FComputeKernelShaderMap::SaveToDerivedDataCache()
{
#if WITH_EDITOR
	COOK_STAT(auto Timer = ComputeKernelShaderCookStats::UsageStats.TimeSyncWork());
	
TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	Serialize(Ar);

	const FString DataKey = GetComputeKernelShaderMapKeyString(GetContent()->ShaderMapId, GetShaderPlatform());

	GetDerivedDataCacheRef().Put(*DataKey, SaveData, GetFriendlyName());
	
COOK_STAT(Timer.AddMiss(SaveData.Num()));
#endif
}

/**
* Compiles the shaders for a kernel and caches them in this shader map.
* @param InKernel - The kernel to compile shaders for.
* @param InShaderMapId - the kernel id and set of static parameters to compile for
* @param InPlatform - The platform to compile to
*/
void FComputeKernelShaderMap::Compile(
	FComputeKernelResource* InKernel, 
	const FComputeKernelShaderMapId& InShaderMapId,
	TRefCountPtr<FSharedShaderCompilerEnvironment> InCompilationEnvironment,
	const FComputeKernelCompilationOutput& InComputeKernelCompilationOutput,
	EShaderPlatform InPlatform,
	bool bSynchronousCompile,
	bool bApplyCompletedShaderMapForRendering
	)
{
	if (FPlatformProperties::RequiresCookedData())
	{
		UE_LOG(LogComputeFramework, Fatal, TEXT("Trying to compile ComputeKernel shader %s at run-time, which is not supported on consoles!"), *InKernel->GetFriendlyName() );
	}
#if WITH_EDITOR
	else
	{
		// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
		// Since it creates a temporary ref counted pointer.
		check(NumRefs > 0);
  
		// Add this shader map and to ComputeKernelShaderMapsBeingCompiled
		TArray<FComputeKernelResource*>* Kernel = ComputeKernelShaderMapsBeingCompiled.Find(this);
  
		if (Kernel)
		{
			check(!bSynchronousCompile);
			Kernel->AddUnique(InKernel);
		}
		else
		{
			// Assign a unique identifier so that shaders from this shader map can be associated with it after a deferred compile
			CompilingId = NextCompilingId;
			UE_LOG(LogComputeFramework, Log, TEXT("CompilingId = %p %d"), InKernel, CompilingId);
			InKernel->AddCompileId(CompilingId);

			check(NextCompilingId < UINT_MAX);
			NextCompilingId++;
  
			// Setup the compilation environment.
			InKernel->SetupShaderCompilationEnvironment(InPlatform, *InCompilationEnvironment);
  
			// Store the kernel name for debugging purposes.
			FComputeKernelShaderMapContent* NewContent = new FComputeKernelShaderMapContent(InPlatform);
			NewContent->FriendlyName = InKernel->GetFriendlyName();
			NewContent->CompilationOutput = InComputeKernelCompilationOutput;
			NewContent->ShaderMapId = InShaderMapId;
			AssignContent(NewContent);

			uint32 NumShaders = 0;
			TArray<FShaderCommonCompileJobPtr> NewJobs;
	
			// Iterate over all shader types.
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FComputeKernelShaderType* ShaderType = ShaderTypeIt->GetComputeKernelShaderType();
				if (ShaderType && ShouldCacheComputeKernelShader(ShaderType, InPlatform, InKernel))
				{
					// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
					check(InShaderMapId.ContainsShaderType(ShaderType));
					
					// Compile this ComputeKernel shader.
					TArray<FString> ShaderErrors;
  
					for (int32 PermutationId = 0; PermutationId < InKernel->GetNumPermutations(); ++PermutationId)
					{
						// Only compile the shader if we don't already have it
						if (!NewContent->HasShader(ShaderType, PermutationId))
						{
							ShaderType->BeginCompileShader(
								CompilingId,
								PermutationId,
								InKernel,
								InCompilationEnvironment,
								InPlatform,
								NewJobs,
								FShaderTarget(ShaderType->GetFrequency(), GetShaderPlatform())
								);
						}
						NumShaders++;
					}
				}
				else if (ShaderType)
				{
					InKernel->RemoveOutstandingCompileId(CompilingId);

					FString Message = FString::Printf(TEXT("%s: Compilation not supported on %s."), 
						*InKernel->GetFriendlyName(), 
						*ShaderPlatformToShaderFormatName(InPlatform).ToString());
					InKernel->NotifyCompilationFinished(Message);
				}
			}
  
			if (!Kernel)
			{
				UE_LOG(LogComputeFramework, Log, TEXT("		%u Shaders"), NumShaders);
			}

			// Register this shader map in the global ComputeKernel->shadermap map
			Register(InPlatform);
  
			// Mark the shader map as not having been finalized with ProcessCompilationResults
			bCompilationFinalized = false;
  
			// Mark as not having been compiled
			bCompiledSuccessfully = false;
  
			if (NumShaders > 0)
			{
				GComputeKernelShaderCompilationManager.AddJobs(NewJobs);

				TArray<FComputeKernelResource*> NewCorrespondingKernels;
				NewCorrespondingKernels.Add(InKernel);
				ComputeKernelShaderMapsBeingCompiled.Add(this, NewCorrespondingKernels);
#if DEBUG_INFINITESHADERCOMPILE
				UE_LOG(LogTemp, Display, TEXT("Added ComputeKernel ShaderMap 0x%08X%08X with kernel 0x%08X%08X to ComputeKernelShaderMapsBeingCompiled"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(InKernel) >> 32), (int)((int64)(InKernel)));
#endif  
			}
  
			// Compile the shaders for this shader map now if not deferring and deferred compiles are not enabled globally
			if (bSynchronousCompile)
			{
				TArray<int32> CurrentShaderMapId;
				CurrentShaderMapId.Add(CompilingId);
				GComputeKernelShaderCompilationManager.FinishCompilation(*NewContent->FriendlyName, CurrentShaderMapId);
			}
		}
	}
#endif
}

FShader* FComputeKernelShaderMap::ProcessCompilationResultsForSingleJob(FShaderCompileJob& CurrentJob, const FSHAHash& InShaderMapHash)
{
	check(CurrentJob.Id == CompilingId);

	GetResourceCode()->AddShaderCompilerOutput(CurrentJob.Output);

	FShader* Shader = nullptr;

	const FComputeKernelShaderType* ComputeKernelShaderType = CurrentJob.Key.ShaderType->GetComputeKernelShaderType();
	check(ComputeKernelShaderType);
	Shader = ComputeKernelShaderType->FinishCompileShader(InShaderMapHash, CurrentJob, GetContent()->FriendlyName);
	bCompiledSuccessfully = CurrentJob.bSucceeded;

	FComputeKernelShader* ComputeKernelShader = static_cast<FComputeKernelShader*>(Shader);
	check(Shader);
	check(!GetContent()->HasShader(ComputeKernelShaderType, CurrentJob.Key.PermutationId));
	return GetMutableContent()->FindOrAddShader(ComputeKernelShaderType->GetHashedName(), CurrentJob.Key.PermutationId, Shader);
}

bool FComputeKernelShaderMap::ProcessCompilationResults(const TArray<FShaderCommonCompileJobPtr>& InCompilationResults, int32& InOutJobIndex, float& InOutTimeBudget)
{
	check(InOutJobIndex < InCompilationResults.Num());

	double StartTime = FPlatformTime::Seconds();

	FSHAHash ShaderMapHash;
	GetContent()->ShaderMapId.GetComputeKernelHash(ShaderMapHash);

	do
	{
		ProcessCompilationResultsForSingleJob(static_cast<FShaderCompileJob&>(*InCompilationResults[InOutJobIndex].GetReference()), ShaderMapHash);

		InOutJobIndex++;
		
		double NewStartTime = FPlatformTime::Seconds();
		InOutTimeBudget -= NewStartTime - StartTime;
		StartTime = NewStartTime;
	}
	while ((InOutTimeBudget > 0.0f) && (InOutJobIndex < InCompilationResults.Num()));

	if (InOutJobIndex == InCompilationResults.Num())
	{
		FinalizeContent();

		SaveToDerivedDataCache();
		// The shader map can now be used on the rendering thread
		bCompilationFinalized = true;
		return true;
	}

	return false;
}

bool FComputeKernelShaderMap::TryToAddToExistingCompilationTask(FComputeKernelResource* InKernel)
{
	check(NumRefs > 0);
	TArray<FComputeKernelResource*>* CorrespondingKernels = FComputeKernelShaderMap::ComputeKernelShaderMapsBeingCompiled.Find(this);

	if (CorrespondingKernels)
	{
		CorrespondingKernels->AddUnique(InKernel);

		UE_LOG(LogComputeFramework, Log, TEXT("TryToAddToExistingCompilationTask %p %d"), InKernel, GetCompilingId());

#if DEBUG_INFINITESHADERCOMPILE
		UE_LOG(LogTemp, Display, TEXT("Added shader map 0x%08X%08X from ComputeKernel 0x%08X%08X"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(InKernel) >> 32), (int)((int64)(InKernel)));
#endif
		return true;
	}

	return false;
}

bool FComputeKernelShaderMap::IsComputeKernelShaderComplete(const FComputeKernelResource* InKernel, const FComputeKernelShaderType* InShaderType, bool bSilent)
{
	// If we should cache this kernel, it's incomplete if the shader is missing
	if (ShouldCacheComputeKernelShader(InShaderType, GetShaderPlatform(), InKernel))
	{
		for (int32 PermutationId = 0; PermutationId < InKernel->GetNumPermutations(); ++PermutationId)
		{
			if (!GetContent()->HasShader((FShaderType*)InShaderType, PermutationId))
			{
				if (!bSilent)
				{
					UE_LOG(LogComputeFramework, Warning, TEXT("Incomplete shader %s, missing FComputeKernelShader %s."), *InKernel->GetFriendlyName(), InShaderType->GetName());
				}
				return false;
			}
		}
	}

	return true;
}

bool FComputeKernelShaderMap::IsComplete(const FComputeKernelResource* InKernel, bool bSilent)
{
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);
	const TArray<FComputeKernelResource*>* CorrespondingKernels = FComputeKernelShaderMap::ComputeKernelShaderMapsBeingCompiled.Find(this);

	if (CorrespondingKernels)
	{
		check(!bCompilationFinalized);
		return false;
	}

	// Iterate over all shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		// Find this shader type in the kernel's shader map.
		const FComputeKernelShaderType* ShaderType = ShaderTypeIt->GetComputeKernelShaderType();
		if (ShaderType && !IsComputeKernelShaderComplete(InKernel, ShaderType, bSilent))
		{
			return false;
		}
	}

	return true;
}

void FComputeKernelShaderMap::LoadMissingShadersFromMemory(const FComputeKernelResource* InKernel)
{
#if 0
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);

	const TArray<FComputeKernelResource*>* CorrespondingKernels = FComputeKernelShaderMap::ComputeKernelShaderMapsBeingCompiled.Find(this);

	if (CorrespondingKernels)
	{
		check(!bCompilationFinalized);
		return;
	}

	FSHAHash ShaderMapHash;
	ShaderMapId.GetComputeKernelHash(ShaderMapHash);

	// Try to find necessary FComputeKernelShaderType's in memory
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FComputeKernelShaderType* ShaderType = ShaderTypeIt->GetComputeKernelShaderType();
		if (ShaderType && ShouldCacheComputeKernelShader(ShaderType, Platform, InKernel) && !HasShader(ShaderType, /* PermutationId = */ 0))
		{
			FShaderKey ShaderKey(ShaderMapHash, nullptr, nullptr, /** PermutationId = */ 0, Platform);
			FShader* FoundShader = ShaderType->FindShaderByKey(ShaderKey);
			if (FoundShader)
			{
				AddShader(ShaderType, /* PermutationId = */ 0, FoundShader);
			}
		}
	}
#endif
}

void FComputeKernelShaderMap::GetShaderList(TMap<FShaderId, TShaderRef<FShader>>& OutShaders) const
{
	GetContent()->GetShaderList(*this, FSHAHash(), OutShaders);
}

/**
 * Registers a ComputeKernel shader map in the global map.
 */
void FComputeKernelShaderMap::Register(EShaderPlatform InShaderPlatform)
{
	if (!bRegistered)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
	}

	GIdToComputeKernelShaderMap[GetShaderPlatform()].Add(GetContent()->ShaderMapId,this);
	bRegistered = true;
}

void FComputeKernelShaderMap::AddRef()
{
	check(!bDeletedThroughDeferredCleanup);
	FPlatformAtomics::InterlockedIncrement(&NumRefs);
}

void FComputeKernelShaderMap::Release()
{
	check(NumRefs > 0);
	if(FPlatformAtomics::InterlockedDecrement(&NumRefs) == 0)
	{
		if (bRegistered)
		{
			DEC_DWORD_STAT(STAT_Shaders_NumShaderMaps);

			GIdToComputeKernelShaderMap[GetShaderPlatform()].Remove(GetContent()->ShaderMapId);
			bRegistered = false;
		}

		check(!bDeletedThroughDeferredCleanup);
		bDeletedThroughDeferredCleanup = true;
		BeginCleanup(this);
	}
}

FComputeKernelShaderMap::FComputeKernelShaderMap() :
	CompilingId(1),
	NumRefs(0),
	bDeletedThroughDeferredCleanup(false),
	bRegistered(false),
	bCompilationFinalized(true),
	bCompiledSuccessfully(true),
	bIsPersistent(true) 
{
	checkSlow(IsInGameThread() || IsAsyncLoading());
	AllComputeKernelShaderMaps.Add(this);
}

FComputeKernelShaderMap::~FComputeKernelShaderMap()
{ 
	checkSlow(IsInGameThread() || IsAsyncLoading());
	check(bDeletedThroughDeferredCleanup);
	check(!bRegistered);
	AllComputeKernelShaderMaps.RemoveSwap(this);
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FComputeKernelShaderMap::FlushShadersByShaderType(const FShaderType* InShaderType)
{
	if (InShaderType->GetComputeKernelShaderType())
	{
		const int32 PermutationCount = InShaderType->GetPermutationCount();
		for (int32 PermutationId = 0; PermutationId < PermutationCount; ++PermutationId)
		{
			GetMutableContent()->RemoveShaderTypePermutaion(InShaderType->GetComputeKernelShaderType(), PermutationId);	
		}
	}
}


bool FComputeKernelShaderMap::Serialize(FArchive& Ar, bool bInlineShaderResources)
{
	// Note: This is saved to the DDC, not into packages (except when cooked)
	// Backwards compatibility therefore will not work based on the version of Ar
	// Instead, just bump COMPUTEKERNEL_DERIVEDDATA_VER
	return Super::Serialize(Ar, bInlineShaderResources, false);
}

void FComputeKernelShaderMap::RemovePending(FComputeKernelResource* InKernel)
{
	for (TMap<TRefCountPtr<FComputeKernelShaderMap>, TArray<FComputeKernelResource*> >::TIterator It(ComputeKernelShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FComputeKernelResource*>& Kernels = It.Value();
		int32 Result = Kernels.Remove(InKernel);
		if (Result)
		{
			InKernel->RemoveOutstandingCompileId(It.Key()->CompilingId);
			InKernel->NotifyCompilationFinished(FString::Printf(TEXT("%s: Removing compilation."), *InKernel->GetFriendlyName()));
		}
#if DEBUG_INFINITESHADERCOMPILE
		if ( Result )
		{
			UE_LOG(LogTemp, Display, TEXT("Removed shader map 0x%08X%08X from kernel 0x%08X%08X"), (int)((int64)(It.Key().GetReference()) >> 32), (int)((int64)(It.Key().GetReference())), (int)((int64)(InKernel) >> 32), (int)((int64)(InKernel)));
		}
#endif
	}
}

