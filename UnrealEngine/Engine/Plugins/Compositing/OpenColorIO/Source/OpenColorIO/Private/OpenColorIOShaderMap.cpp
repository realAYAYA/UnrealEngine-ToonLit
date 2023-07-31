// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOShader.h"

#include "Engine/VolumeTexture.h"
#include "OpenColorIOShared.h"
#include "OpenColorIODerivedDataVersion.h"
#include "OpenColorIOShaderCompilationManager.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "ShaderCompiler.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
	#include "DerivedDataCacheInterface.h"
	#include "Interfaces/ITargetPlatformManagerModule.h"
	#include "TickableEditorObject.h"

#if WITH_OCIO
	#include "OpenColorIO/OpenColorIO.h"
#endif //WITH_OCIO

#endif


#if ENABLE_COOK_STATS
namespace OpenColorIOShaderCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("OpenColorIOShader.Usage"), TEXT(""));
		AddStat(TEXT("OpenColorIOShader.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
		));
	});
}
#endif


//
// Globals
//
TMap<FOpenColorIOShaderMapId, FOpenColorIOShaderMap*> FOpenColorIOShaderMap::GIdToOpenColorIOShaderMap[SP_NumPlatforms];
TArray<FOpenColorIOShaderMap*> FOpenColorIOShaderMap::AllOpenColorIOShaderMaps;

// The Id of 0 is reserved for global shaders
uint32 FOpenColorIOShaderMap::NextCompilingId = 2;


/** 
 * Tracks FOpenColorIOTransformResource and their shader maps that are being compiled.
 * Uses a TRefCountPtr as this will be the only reference to a shader map while it is being compiled.
 */
TMap<TRefCountPtr<FOpenColorIOShaderMap>, TArray<FOpenColorIOTransformResource*> > FOpenColorIOShaderMap::OpenColorIOShaderMapsBeingCompiled;



static inline bool ShouldCacheOpenColorIOShader(const FOpenColorIOShaderType* InShaderType, EShaderPlatform InPlatform, const FOpenColorIOTransformResource* InColorTransformShader)
{
	return InShaderType->ShouldCache(InPlatform, InColorTransformShader) && InColorTransformShader->ShouldCache(InPlatform, InShaderType);
}



/** Called for every color transform shader to update the appropriate stats. */
void UpdateOpenColorIOShaderCompilingStats(const FOpenColorIOTransformResource* InShader)
{
	INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTotalOpenColorIOShaders,1);
}

/*void FOpenColorIOShaderMapId::Serialize(FArchive& Ar)
{
	// You must bump OPENCOLORIO_DERIVEDDATA_VER if changing the serialization of FOpenColorIOShaderMapId.

	Ar << ShaderCodeHash;
	Ar << (int32&)FeatureLevel;
	Ar << ShaderTypeDependencies;
}*/

/** Hashes the color transform specific part of this shader map Id. */
void FOpenColorIOShaderMapId::GetOpenColorIOHash(FSHAHash& OutHash) const
{
	FSHA1 HashState;

	HashState.UpdateWithString(*ShaderCodeAndConfigHash, ShaderCodeAndConfigHash.Len());
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
bool FOpenColorIOShaderMapId::operator==(const FOpenColorIOShaderMapId& InReferenceSet) const
{
	if (  ShaderCodeAndConfigHash != InReferenceSet.ShaderCodeAndConfigHash
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

void FOpenColorIOShaderMapId::AppendKeyString(FString& OutKeyString) const
{
#if WITH_EDITOR
	OutKeyString += ShaderCodeAndConfigHash;
	OutKeyString += TEXT("_");

	FString FeatureLevelString;
	GetFeatureLevelName(FeatureLevel, FeatureLevelString);

	{
		const FSHAHash LayoutHash = Freeze::HashLayout(StaticGetTypeLayoutDesc<FOpenColorIOShaderMapContent>(), LayoutParams);
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
 * @param InColorTransform - The ColorTransform to link the shader with.
 */
void FOpenColorIOShaderType::BeginCompileShader(
	uint32 InShaderMapId,
	const FOpenColorIOTransformResource* InColorTransform,
	FSharedShaderCompilerEnvironment* InCompilationEnvironment,
	EShaderPlatform InPlatform,
	TArray<FShaderCommonCompileJobPtr>& OutNewJobs,
	FShaderTarget InTarget
	)
{
	FShaderCompileJob* NewJob = GShaderCompilingManager->PrepareShaderCompileJob(InShaderMapId, FShaderCompileJobKey(this), EShaderCompileJobPriority::Normal);
	if (!NewJob)
	{
		return;
	}

	NewJob->Input.SharedEnvironment = InCompilationEnvironment;
	NewJob->Input.Target = InTarget;
	NewJob->Input.ShaderFormat = LegacyShaderPlatformToShaderFormat(InPlatform);
	NewJob->Input.VirtualSourceFilePath = TEXT("/Engine/Plugins/Compositing/OpenColorIO/Shaders/Private/OpenColorIOShader.usf");
	NewJob->Input.EntryPointName = TEXT("MainPS");
	NewJob->Input.Environment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/OpenColorIOTransformShader.ush"), InColorTransform->ShaderCode);

	AddReferencedUniformBufferIncludes(NewJob->Input.Environment, NewJob->Input.SourceFilePrefix, InPlatform);

	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	//update InColorTransform shader stats
	UpdateOpenColorIOShaderCompilingStats(InColorTransform);

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(InPlatform, InColorTransform, ShaderEnvironment);

	::GlobalBeginCompileShader(
		InColorTransform->GetFriendlyName(),
		nullptr,
		this,
		nullptr,//ShaderPipeline
		0, // PermutationId
		TEXT("/Plugin/OpenColorIO/Private/OpenColorIOShader.usf"),
		TEXT("MainPS"),
		FShaderTarget(GetFrequency(), InPlatform),
		NewJob->Input
	);

	OutNewJobs.Add(FShaderCommonCompileJobPtr(NewJob));
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param InShaderMapHash - Precomputed hash of the shader map 
 * @param InCurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FOpenColorIOShaderType::FinishCompileShader(
	const FSHAHash& InShaderMapHash,
	const FShaderCompileJob& InCurrentJob,
	const FString& InDebugDescription
	) const
{
	check(InCurrentJob.bSucceeded);

	const int32 PermutationId = 0;
	FShader* Shader = ConstructCompiled(FOpenColorIOShaderType::CompiledShaderInitializerType(this, PermutationId, InCurrentJob.Output, InShaderMapHash, InDebugDescription));
	InCurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), InCurrentJob.Output.Target, nullptr);

	return Shader;
}

/**
 * Finds the shader map for a color transform.
 * @param InShaderMapId - The color transform id and static parameter set identifying the shader map
 * @param InPlatform - The platform to lookup for
 * @return nullptr if no cached shader map was found.
 */
FOpenColorIOShaderMap* FOpenColorIOShaderMap::FindId(const FOpenColorIOShaderMapId& InShaderMapId, EShaderPlatform InPlatform)
{
	check(!InShaderMapId.ShaderCodeAndConfigHash.IsEmpty());
	return GIdToOpenColorIOShaderMap[InPlatform].FindRef(InShaderMapId);
}

void OpenColorIOShaderMapAppendKeyString(EShaderPlatform InPlatform, FString& OutKeyString)
{
#if WITH_EDITOR && WITH_OCIO
	//Keep library version in the DDC key to invalidate it once we move to a new library
	OutKeyString += TEXT("OCIOVersion");
	OutKeyString += TEXT(OCIO_VERSION);
	OutKeyString += TEXT("_");
#endif //WITH_EDITOR && WITH_OCIO
}

/** Creates a string key for the derived data cache given a shader map id. */
static FString GetOpenColorIOShaderMapKeyString(const FOpenColorIOShaderMapId& InShaderMapId, EShaderPlatform InPlatform)
{
#if WITH_EDITOR
	const FName Format = LegacyShaderPlatformToShaderFormat(InPlatform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	OpenColorIOShaderMapAppendKeyString(InPlatform, ShaderMapKeyString);
	ShaderMapAppendKeyString(InPlatform, ShaderMapKeyString);
	InShaderMapId.AppendKeyString(ShaderMapKeyString);
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("OCIOSM"), OPENCOLORIO_DERIVEDDATA_VER, *ShaderMapKeyString);
#else
	return FString();
#endif
}

void FOpenColorIOShaderMap::LoadFromDerivedDataCache(const FOpenColorIOTransformResource* InColorTransform, const FOpenColorIOShaderMapId& InShaderMapId, EShaderPlatform InPlatform, TRefCountPtr<FOpenColorIOShaderMap>& InOutShaderMap)
{
#if WITH_EDITOR
	if (InOutShaderMap != nullptr)
	{
		check(InOutShaderMap->GetShaderPlatform() == InPlatform);
	}
	else
	{
		// Shader map was not found in memory, try to load it from the DDC
		STAT(double OpenColorIOShaderDDCTime = 0);
		{
			SCOPE_SECONDS_COUNTER(OpenColorIOShaderDDCTime);
			COOK_STAT(auto Timer = OpenColorIOShaderCookStats::UsageStats.TimeSyncWork());

			TArray<uint8> CachedData;
			const FString DataKey = GetOpenColorIOShaderMapKeyString(InShaderMapId, InPlatform);

			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData, InColorTransform->GetFriendlyName()))
			{
				COOK_STAT(Timer.AddHit(CachedData.Num()));
				InOutShaderMap = new FOpenColorIOShaderMap();
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
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_DDCLoading,(float)OpenColorIOShaderDDCTime);
	}
#endif
}

void FOpenColorIOShaderMap::SaveToDerivedDataCache()
{
#if WITH_EDITOR
	COOK_STAT(auto Timer = OpenColorIOShaderCookStats::UsageStats.TimeSyncWork());
	TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	Serialize(Ar);

	GetDerivedDataCacheRef().Put(*GetOpenColorIOShaderMapKeyString(GetContent()->ShaderMapId, GetShaderPlatform()), SaveData, FStringView(*GetFriendlyName()));
	COOK_STAT(Timer.AddMiss(SaveData.Num()));
#endif
}

/**
* Compiles the shaders for a color transform and caches them in this shader map.
* @param InColorTransform - The ColorTransform to compile shaders for.
* @param InShaderMapId - the color transform id and set of static parameters to compile for
* @param InPlatform - The platform to compile to
*/
void FOpenColorIOShaderMap::Compile(FOpenColorIOTransformResource* InColorTransform
									, const FOpenColorIOShaderMapId& InShaderMapId
									, TRefCountPtr<FSharedShaderCompilerEnvironment> InCompilationEnvironment
									, const FOpenColorIOCompilationOutput& InOpenColorIOCompilationOutput
									, EShaderPlatform InPlatform
									, bool bSynchronousCompile
									, bool bApplyCompletedShaderMapForRendering)
{
	if (FPlatformProperties::RequiresCookedData())
	{
		UE_LOG(LogShaders, Fatal, TEXT("Trying to compile OpenColorIO shader %s at run-time, which is not supported on consoles!"), *InColorTransform->GetFriendlyName());
	}
	else
	{
		// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
		// Since it creates a temporary ref counted pointer.
		check(NumRefs > 0);
  
		//All access to OpenColorIOShaderMapsBeingCompiled must be done on the game thread!
		check(IsInGameThread());
		// Add this shader map and to OpenColorIOShaderMapsBeingCompiled
		TArray<FOpenColorIOTransformResource*>* CorrespondingTransform = OpenColorIOShaderMapsBeingCompiled.Find(this);
  
		if (CorrespondingTransform)
		{
			check(!bSynchronousCompile);
			CorrespondingTransform->AddUnique(InColorTransform);
		}
		else
		{
			InColorTransform->RemoveOutstandingCompileId(CompilingId);
			// Assign a unique identifier so that shaders from this shader map can be associated with it after a deferred compile
			CompilingId = FShaderCommonCompileJob::GetNextJobId();
			InColorTransform->AddCompileId(CompilingId);
  
			TArray<FOpenColorIOTransformResource*> NewCorrespondingTransforms;
			NewCorrespondingTransforms.Add(InColorTransform);
			OpenColorIOShaderMapsBeingCompiled.Add(this, NewCorrespondingTransforms);
#if DEBUG_INFINITESHADERCOMPILE
			UE_LOG(LogTemp, Display, TEXT("Added OpenColorIO ShaderMap 0x%08X%08X with ColorTransform 0x%08X%08X to OpenColorIOShaderMapsBeingCompiled"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(InColorTransform) >> 32), (int)((int64)(InColorTransform)));
#endif  
			// Setup the compilation environment.
			InColorTransform->SetupShaderCompilationEnvironment(InPlatform, *InCompilationEnvironment);
  
			// Store the ColorTransform name for debugging purposes.
			FOpenColorIOShaderMapContent* NewContent = new FOpenColorIOShaderMapContent(InPlatform);
			NewContent->FriendlyName = InColorTransform->GetFriendlyName();
			NewContent->OpenColorIOCompilationOutput = InOpenColorIOCompilationOutput;
			NewContent->ShaderMapId = InShaderMapId;
			AssignContent(NewContent);

			uint32 NumShaders = 0;
			TArray<FShaderCommonCompileJobPtr> NewJobs;
	
			// Iterate over all shader types.
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FOpenColorIOShaderType* ShaderType = ShaderTypeIt->GetOpenColorIOShaderType();
				if (ShaderType && ShouldCacheOpenColorIOShader(ShaderType, InPlatform, InColorTransform))
				{
					// Compile this OpenColorIO shader .
					TArray<FString> ShaderErrors;
  
					// Only compile the shader if we don't already have it
					if (!NewContent->HasShader(ShaderType, /* PermutationId = */ 0))
					{
						ShaderType->BeginCompileShader(
							CompilingId,
							InColorTransform,
							InCompilationEnvironment,
							InPlatform,
							NewJobs,
							FShaderTarget(ShaderType->GetFrequency(), GetShaderPlatform())
						);
					}
					NumShaders++;
				}
				else if (ShaderType)
				{
					UE_LOG(LogShaders, Display, TEXT("Skipping compilation of %s as it isn't supported on this target type."), *InColorTransform->GetFriendlyName());
					InColorTransform->RemoveOutstandingCompileId(CompilingId);
				}
			}
  
			if (!CorrespondingTransform)
			{
				UE_LOG(LogShaders, Log, TEXT("		%u Shaders"), NumShaders);
			}

			// Register this shader map in the global script->shadermap map
			Register(InPlatform);
  
			// Mark the shader map as not having been finalized with ProcessCompilationResults
			bCompilationFinalized = false;
  
			// Mark as not having been compiled
			bCompiledSuccessfully = false;
  
			GOpenColorIOShaderCompilationManager.AddJobs(NewJobs);
  
			// Compile the shaders for this shader map now if not deferring and deferred compiles are not enabled globally
			if (bSynchronousCompile)
			{
				TArray<int32> CurrentShaderMapId;
				CurrentShaderMapId.Add(CompilingId);
				GOpenColorIOShaderCompilationManager.FinishCompilation(*NewContent->FriendlyName, CurrentShaderMapId);
			}
		}
	}
}

FShader* FOpenColorIOShaderMap::ProcessCompilationResultsForSingleJob(const TRefCountPtr<class FShaderCommonCompileJob>& SingleJob, const FSHAHash& InShaderMapHash)
{
	FShaderCompileJob* CurrentJob = SingleJob->GetSingleShaderJob();
	check(CurrentJob->Id == CompilingId);

	GetResourceCode()->AddShaderCompilerOutput(CurrentJob->Output);

	FShader* Shader = nullptr;

	const FOpenColorIOShaderType* OpenColorIOShaderType = CurrentJob->Key.ShaderType->GetOpenColorIOShaderType();
	check(OpenColorIOShaderType);
	Shader = OpenColorIOShaderType->FinishCompileShader(InShaderMapHash, *CurrentJob, GetContent()->FriendlyName);
	bCompiledSuccessfully = CurrentJob->bSucceeded;

	check(Shader && Shader->GetCodeSize() > 0);
	check(!GetContent()->HasShader(OpenColorIOShaderType, /* PermutationId = */ 0));
	return GetMutableContent()->FindOrAddShader(OpenColorIOShaderType->GetHashedName(), CurrentJob->Key.PermutationId, Shader);
}

bool FOpenColorIOShaderMap::ProcessCompilationResults(const TArray<FShaderCommonCompileJobPtr>& InCompilationResults, int32& InOutJobIndex, float& InOutTimeBudget)
{
	check(InOutJobIndex < InCompilationResults.Num());

	double StartTime = FPlatformTime::Seconds();

	FSHAHash ShaderMapHash;
	GetContent()->ShaderMapId.GetOpenColorIOHash(ShaderMapHash);

	do
	{
		ProcessCompilationResultsForSingleJob(InCompilationResults[InOutJobIndex], ShaderMapHash);

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

bool FOpenColorIOShaderMap::TryToAddToExistingCompilationTask(FOpenColorIOTransformResource* InColorTransform)
{
	check(NumRefs > 0);
	TArray<FOpenColorIOTransformResource*>* CorrespondingColorTransforms = FOpenColorIOShaderMap::OpenColorIOShaderMapsBeingCompiled.Find(this);

	if (CorrespondingColorTransforms)
	{
		CorrespondingColorTransforms->AddUnique(InColorTransform);

		UE_LOG(LogShaders, Log, TEXT("TryToAddToExistingCompilationTask %p %d"), InColorTransform, GetCompilingId());

#if DEBUG_INFINITESHADERCOMPILE
		UE_LOG(LogTemp, Display, TEXT("Added shader map 0x%08X%08X from OpenColorIO transform 0x%08X%08X"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(InColorTransform) >> 32), (int)((int64)(InColorTransform)));
#endif
		return true;
	}

	return false;
}

bool FOpenColorIOShaderMap::IsOpenColorIOShaderComplete(const FOpenColorIOTransformResource* InColorTransform, const FOpenColorIOShaderType* InShaderType, bool bSilent)
{
	// If we should cache this color transform, it's incomplete if the shader is missing
	if (ShouldCacheOpenColorIOShader(InShaderType, GetShaderPlatform(), InColorTransform) && !GetContent()->HasShader((FShaderType*)InShaderType, /* PermutationId = */ 0))
	{
		if (!bSilent)
		{
			UE_LOG(LogShaders, Warning, TEXT("Incomplete shader %s, missing FOpenColorIOShader %s."), *InColorTransform->GetFriendlyName(), InShaderType->GetName());
		}
		return false;
	}

	return true;
}

bool FOpenColorIOShaderMap::IsComplete(const FOpenColorIOTransformResource* InColorTransform, bool bSilent)
{
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);
	const TArray<FOpenColorIOTransformResource*>* CorrespondingColorTransforms = FOpenColorIOShaderMap::OpenColorIOShaderMapsBeingCompiled.Find(this);

	if (CorrespondingColorTransforms)
	{
		check(!bCompilationFinalized);
		return false;
	}

	// Iterate over all shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		// Find this shader type in the ColorTransform's shader map.
		const FOpenColorIOShaderType* ShaderType = ShaderTypeIt->GetOpenColorIOShaderType();
		if (ShaderType && !IsOpenColorIOShaderComplete(InColorTransform, ShaderType, bSilent))
		{
			return false;
		}
	}

	return true;
}

void FOpenColorIOShaderMap::GetShaderList(TMap<FShaderId, TShaderRef<FShader>>& OutShaders) const
{
	GetContent()->GetShaderList(*this, FSHAHash(), OutShaders);
}

/**
 * Registers a OpenColorIO shader map in the global map.
 */
void FOpenColorIOShaderMap::Register(EShaderPlatform InShaderPlatform)
{
	if (!bRegistered)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
	}

	GIdToOpenColorIOShaderMap[GetShaderPlatform()].Add(GetContent()->ShaderMapId,this);
	bRegistered = true;
}

void FOpenColorIOShaderMap::AddRef()
{
	check(!bDeletedThroughDeferredCleanup);
	++NumRefs;
}

void FOpenColorIOShaderMap::Release()
{
	check(NumRefs > 0);
	if(--NumRefs == 0)
	{
		if (bRegistered)
		{
			DEC_DWORD_STAT(STAT_Shaders_NumShaderMaps);

			GIdToOpenColorIOShaderMap[GetShaderPlatform()].Remove(GetContent()->ShaderMapId);
			bRegistered = false;
		}

		check(!bDeletedThroughDeferredCleanup);
		bDeletedThroughDeferredCleanup = true;
		BeginCleanup(this);
	}
}

FOpenColorIOShaderMap::FOpenColorIOShaderMap() :
	CompilingId(1),
	NumRefs(0),
	bDeletedThroughDeferredCleanup(false),
	bRegistered(false),
	bCompilationFinalized(true),
	bCompiledSuccessfully(true),
	bIsPersistent(true) 
{
	checkSlow(IsInGameThread() || IsAsyncLoading());
	AllOpenColorIOShaderMaps.Add(this);
}

FOpenColorIOShaderMap::~FOpenColorIOShaderMap()
{ 
	checkSlow(IsInGameThread() || IsAsyncLoading());
	check(bDeletedThroughDeferredCleanup);
	check(!bRegistered);
	AllOpenColorIOShaderMaps.RemoveSwap(this);
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FOpenColorIOShaderMap::FlushShadersByShaderType(const FShaderType* InShaderType)
{
	if (InShaderType->GetOpenColorIOShaderType())
	{
		GetMutableContent()->RemoveShaderTypePermutaion(InShaderType->GetOpenColorIOShaderType(), /* PermutationId = */ 0);	
	}
}


bool FOpenColorIOShaderMap::Serialize(FArchive& Ar, bool bInlineShaderResources)
{
	// Note: This is saved to the DDC, not into packages (except when cooked)
	// Backwards compatibility therefore will not work based on the version of Ar
	// Instead, just bump OPENCOLORIO_DERIVEDDATA_VER
	return Super::Serialize(Ar, bInlineShaderResources, false);
}

void FOpenColorIOShaderMap::RemovePendingColorTransform(FOpenColorIOTransformResource* InColorTransform)
{
	for (TMap<TRefCountPtr<FOpenColorIOShaderMap>, TArray<FOpenColorIOTransformResource*> >::TIterator It(OpenColorIOShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FOpenColorIOTransformResource*>& ColorTranforms = It.Value();
		int32 Result = ColorTranforms.Remove(InColorTransform);
		if (Result)
		{
			InColorTransform->RemoveOutstandingCompileId(It.Key()->CompilingId);
		}
#if DEBUG_INFINITESHADERCOMPILE
		if ( Result )
		{
			UE_LOG(LogTemp, Display, TEXT("Removed shader map 0x%08X%08X from color transform 0x%08X%08X"), (int)((int64)(It.Key().GetReference()) >> 32), (int)((int64)(It.Key().GetReference())), (int)((int64)(InColorTransform) >> 32), (int)((int64)(InColorTransform)));
		}
#endif
	}
}

