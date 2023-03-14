// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NiagaraShared.cpp: Shared Niagara compute shader implementation.
=============================================================================*/

#include "NiagaraShared.h"
#include "NiagaraShaderModule.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraShaderType.h"
#include "NiagaraShader.h"
#include "NiagaraScriptBase.h"
#include "NiagaraScript.h"		//-TODO: This should be fixed so we are not reading structures from modules we do not depend on
#include "Stats/StatsMisc.h"
#include "UObject/CoreObjectVersion.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ShaderCompiler.h"
#include "NiagaraShaderCompilationManager.h"
#include "RendererInterface.h"
#include "Modules/ModuleManager.h"
#include "NiagaraCustomVersion.h"
#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "ShaderCodeLibrary.h"
#endif
#include "ShaderParameterMetadataBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraShared)

IMPLEMENT_TYPE_LAYOUT(FNiagaraDataInterfaceParamRef);
IMPLEMENT_TYPE_LAYOUT(FNiagaraShaderMapContent);
IMPLEMENT_TYPE_LAYOUT(FNiagaraShaderMapId);
IMPLEMENT_TYPE_LAYOUT(FNiagaraComputeShaderCompilationOutput);

//* CVars */
int32 GNiagaraTranslatorFailIfNotSetSeverity = 3;
static FAutoConsoleVariableRef CVarNiagaraTranslatorSilenceFailIfNotSet(
	TEXT("fx.Niagara.FailIfNotSetSeverity"),
	GNiagaraTranslatorFailIfNotSetSeverity,
	TEXT("The severity of messages emitted by Parameters with Default Mode \"Fail If Not Set\". 3 = Error, 2 = Warning, 1= Log, 0 = Disabled.\n"),
	ECVF_Default
);

#if WITH_EDITOR
	FNiagaraCompilationQueue* FNiagaraCompilationQueue::Singleton = nullptr;
#endif

FNiagaraShaderScript::~FNiagaraShaderScript()
{
#if WITH_EDITOR
	check(IsInGameThread());
	CancelCompilation();
#endif
}

/** Populates OutEnvironment with defines needed to compile shaders for this script. */
void FNiagaraShaderScript::SetupShaderCompilationEnvironment(
	EShaderPlatform Platform,
	FShaderCompilerEnvironment& OutEnvironment
	) const
{
	OutEnvironment.SetDefine(TEXT("GPU_SIMULATION_SHADER"), TEXT("1"));
}


bool FNiagaraShaderScript::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType) const
{
	check(ShaderType->GetNiagaraShaderType() != nullptr);
	if (BaseVMScript)
	{
		if (!BaseVMScript->ShouldCompile(Platform))
		{
			return false;
		}
	}
	return true;
}

void FNiagaraShaderScript::ModifyCompilationEnvironment(EShaderPlatform Platform, struct FShaderCompilerEnvironment& OutEnvironment) const
{
	if ( BaseVMScript )
	{
		BaseVMScript->ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
}

bool FNiagaraShaderScript::GetUsesCompressedAttributes() const
{
	return AdditionalDefines.Contains(TEXT("CompressAttributes"));
}

void FNiagaraShaderScript::NotifyCompilationFinished()
{
	UpdateCachedData_PostCompile();

	OnCompilationCompleteDelegate.Broadcast();
}

void FNiagaraShaderScript::CancelCompilation()
{
#if WITH_EDITOR
	check(IsInGameThread());
	bool bWasPending = FNiagaraShaderMap::RemovePendingScript(this);
	FNiagaraCompilationQueue::Get()->RemovePending(this);

	// don't spam the log if no cancelling actually happened : 
	if (bWasPending)
	{
		UE_LOG(LogShaders, Log, TEXT("CancelCompilation %p."), this);
	}
	OutstandingCompileShaderMapIds.Empty();
#endif
}

void FNiagaraShaderScript::RemoveOutstandingCompileId(const int32 OldOutstandingCompileShaderMapId)
{
	check(IsInGameThread());
	if (0 <= OutstandingCompileShaderMapIds.Remove(OldOutstandingCompileShaderMapId))
	{
		//UE_LOG(LogShaders, Log, TEXT("RemoveOutstandingCompileId %p %d"), this, OldOutstandingCompileShaderMapId);
	}
}

void FNiagaraShaderScript::Invalidate()
{
	CancelCompilation();
	ReleaseShaderMap();
#if WITH_EDITOR
	CompileErrors.Empty();
	HlslOutput.Empty();
#endif
}

void FNiagaraShaderScript::LegacySerialize(FArchive& Ar)
{
}

bool FNiagaraShaderScript::IsSame(const FNiagaraShaderMapId& InId) const
{
	if (InId.ReferencedCompileHashes.Num() != ReferencedCompileHashes.Num() ||
		InId.AdditionalDefines.Num() != AdditionalDefines.Num() ||
		InId.AdditionalVariables.Num() != AdditionalVariables.Num())
	{
		return false;
	}

	for (int32 i = 0; i < ReferencedCompileHashes.Num(); ++i)
	{
		if (ReferencedCompileHashes[i] != InId.ReferencedCompileHashes[i])
		{
			return false;
		}
	}
	for (int32 i = 0; i < AdditionalDefines.Num(); ++i)
	{
		if (AdditionalDefines[i] != *InId.AdditionalDefines[i])
		{
			return false;
		}
	}
	for (int32 i = 0; i < AdditionalVariables.Num(); ++i)
	{
		if (AdditionalVariables[i] != *InId.AdditionalVariables[i])
		{
			return false;
		}
	}

	return
		InId.FeatureLevel == FeatureLevel &&/*
		InId.BaseScriptID == BaseScriptId &&*/
		InId.bUsesRapidIterationParams == bUsesRapidIterationParams &&
		InId.BaseCompileHash == BaseCompileHash &&
		InId.CompilerVersionID == CompilerVersionId;
}

bool FNiagaraShaderScript::IsShaderMapComplete() const
{
	if (GameThreadShaderMap == nullptr)
	{
		return false;
	}

	if (FNiagaraShaderMap::GetShaderMapBeingCompiled(this) != nullptr)
	{
		return false;
	}

	if (!GameThreadShaderMap->IsValid())
	{
		return false;
	}

	for (int i=0; i < GetNumPermutations(); ++i)
	{
		if (GameThreadShaderMap->GetShader<FNiagaraShader>(i).IsNull())
		{
			return false;
		}
	}
	return true;
}

void FNiagaraShaderScript::GetDependentShaderTypes(EShaderPlatform Platform, TArray<FShaderType*>& OutShaderTypes) const
{
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FNiagaraShaderType* ShaderType = ShaderTypeIt->GetNiagaraShaderType();

		if ( ShaderType && ShaderType->ShouldCache(Platform, this) && ShouldCache(Platform, ShaderType) )
		{
			OutShaderTypes.Add(ShaderType);
		}
	}
}



void FNiagaraShaderScript::GetShaderMapId(EShaderPlatform Platform, const ITargetPlatform* TargetPlatform, FNiagaraShaderMapId& OutId) const
{
	if (bLoadedCookedShaderMapId)
	{
		OutId = CookedShaderMapId;
	}
	else
	{
		INiagaraShaderModule* Module = INiagaraShaderModule::Get();

		OutId.FeatureLevel = GetFeatureLevel();
		OutId.bUsesRapidIterationParams = bUsesRapidIterationParams;		
		BaseCompileHash.ToSHAHash(OutId.BaseCompileHash);
		OutId.CompilerVersionID = FNiagaraCustomVersion::GetLatestScriptCompileVersion();

		OutId.ReferencedCompileHashes.Reserve(ReferencedCompileHashes.Num());
		for (const FNiagaraCompileHash& Hash : ReferencedCompileHashes)
		{
			Hash.ToSHAHash(OutId.ReferencedCompileHashes.AddDefaulted_GetRef());
		}

		OutId.AdditionalDefines.Empty(AdditionalDefines.Num());
		for(const FString& Define : AdditionalDefines)
		{
			OutId.AdditionalDefines.Emplace(Define);
		}

		OutId.AdditionalVariables.Empty(AdditionalVariables.Num());
		for(const FString& Variable : AdditionalVariables)
		{
			OutId.AdditionalVariables.Emplace(Variable);
		}

		TArray<FShaderType*> DependentShaderTypes;
		GetDependentShaderTypes(Platform, DependentShaderTypes);
		for (FShaderType* ShaderType : DependentShaderTypes)
		{
			OutId.ShaderTypeDependencies.Emplace(ShaderType, Platform);
		}

#if WITH_EDITOR
		if (TargetPlatform)
		{
			OutId.LayoutParams.InitializeForPlatform(TargetPlatform->IniPlatformName(), TargetPlatform->HasEditorOnlyData());
		}
		else
		{
			OutId.LayoutParams.InitializeForCurrent();
		}
#else
		if (TargetPlatform != nullptr)
		{
			UE_LOG(LogShaders, Error, TEXT("FNiagaraShaderScript::GetShaderMapId: TargetPlatform is not null, but a cooked executable cannot target platforms other than its own."));
		}
		OutId.LayoutParams.InitializeForCurrent();
#endif
	}
}



void FNiagaraShaderScript::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void  FNiagaraShaderScript::DiscardShaderMap()
{
	if (GameThreadShaderMap)
	{
		//GameThreadShaderMap->DiscardSerializedShaders();
	}
}

void FNiagaraShaderScript::ReleaseShaderMap()
{
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap = nullptr;

		if (!bQueuedForRelease)
		{
			FNiagaraShaderScript* Script = this;
			ENQUEUE_RENDER_COMMAND(ReleaseShaderMap)(
				[Script](FRHICommandListImmediate& RHICmdList)
				{
					Script->SetRenderingThreadShaderMap(nullptr);
				});
		}

		UpdateCachedData_All();
	}
}

void FNiagaraShaderScript::SerializeShaderMap(FArchive& Ar)
{
	bool bCooked = Ar.IsCooking();
	Ar << bCooked;
	Ar << NumPermutations;

	if (Ar.IsLoading())
	{
		bLoadedFromCookedMaterial = bCooked;
	}

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogShaders, Fatal, TEXT("This platform requires cooked packages, and shaders were not cooked into this Niagara script %s."), *GetFriendlyName());
	}

	if (bCooked)
	{
		if (Ar.IsCooking())
		{
#if WITH_EDITOR
			FinishCompilation();

			bool bValid = GameThreadShaderMap != nullptr && GameThreadShaderMap->CompiledSuccessfully();
			Ar << bValid;

			if (bValid)
			{
				// associate right here
				if (BaseVMScript)
				{
					GameThreadShaderMap->AssociateWithAsset(BaseVMScript->GetOutermost()->GetFName());
				}
				GameThreadShaderMap->Serialize(Ar);
			}
			//else if (GameThreadShaderMap != nullptr && !GameThreadShaderMap->CompiledSuccessfully())
			//{
			//	FString Name;
			//	UE_LOG(LogShaders, Error, TEXT("Failed to compile Niagara shader %s."), *GetFriendlyName());
			//}
#endif
		}
		else
		{
			bool bValid = false;
			Ar << bValid;

			if (bValid)
			{
				TRefCountPtr<FNiagaraShaderMap> LoadedShaderMap = new FNiagaraShaderMap();
				bool bLoaded = LoadedShaderMap->Serialize(Ar, true, true);

				// Toss the loaded shader data if this is a server only instance
				//@todo - don't cook it in the first place
				if (FApp::CanEverRender() && bLoaded)
				{
					GameThreadShaderMap = RenderingThreadShaderMap = LoadedShaderMap;

					UpdateCachedData_PostCompile(true);
				}
				else
				{
					//LoadedShaderMap->DiscardSerializedShaders();
				}
			}
		}
	}
}

#if WITH_EDITOR
void FNiagaraShaderScript::SaveShaderStableKeys(EShaderPlatform TargetShaderPlatform, FStableShaderKeyAndValue& SaveKeyVal)
{
	if (GameThreadShaderMap && GameThreadShaderMap->IsValid())
	{
		FString FeatureLevelName;
		GetFeatureLevelName(FeatureLevel, FeatureLevelName);
		SaveKeyVal.FeatureLevel = FName(*FeatureLevelName);

		static FName FName_Num(TEXT("Num")); // Niagara resources aren't associated with a quality level, so we use Num which for the materials means "Default"
		SaveKeyVal.QualityLevel = FName_Num;

		GameThreadShaderMap->SaveShaderStableKeys(TargetShaderPlatform, SaveKeyVal);
	}
}
#endif

void FNiagaraShaderScript::SetScript(UNiagaraScriptBase* InScript, ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform, const FGuid& InCompilerVersionID,  const TArray<FString>& InAdditionalDefines, const TArray<FString>& InAdditionalVariables,
		const FNiagaraCompileHash& InBaseCompileHash, const TArray<FNiagaraCompileHash>& InReferencedCompileHashes, 
		bool bInUsesRapidIterationParams, FString InFriendlyName)
{
	checkf(InBaseCompileHash.IsValid(), TEXT("Invalid base compile hash.  Script caching will fail."))
	BaseVMScript = InScript;
	CompilerVersionId = InCompilerVersionID;
	AdditionalDefines = InAdditionalDefines;
	AdditionalVariables = InAdditionalVariables;
	bUsesRapidIterationParams = bInUsesRapidIterationParams;
	BaseCompileHash = InBaseCompileHash;
	ReferencedCompileHashes = InReferencedCompileHashes;
	FriendlyName = InFriendlyName;
	SetFeatureLevel(InFeatureLevel);
	ShaderPlatform = InShaderPlatform;

	UpdateCachedData_All();
}

#if WITH_EDITOR
bool FNiagaraShaderScript::MatchesScript(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform, const FNiagaraVMExecutableDataId& ScriptId) const
{
	return CompilerVersionId == ScriptId.CompilerVersionID
		&& AdditionalDefines == ScriptId.AdditionalDefines
		&& bUsesRapidIterationParams == ScriptId.bUsesRapidIterationParams
		&& BaseCompileHash == ScriptId.BaseScriptCompileHash
		&& ReferencedCompileHashes == ScriptId.ReferencedCompileHashes
		&& FeatureLevel == InFeatureLevel
		&& ShaderPlatform == InShaderPlatform;
}
#endif

void FNiagaraShaderScript::SetRenderingThreadShaderMap(FNiagaraShaderMap* InShaderMap)
{
	check(IsInRenderingThread());
	RenderingThreadShaderMap = InShaderMap;
}

bool FNiagaraShaderScript::IsCompilationFinished() const
{
	check(IsInGameThread());
	bool bRet = GameThreadShaderMap && GameThreadShaderMap.IsValid() && GameThreadShaderMap->IsCompilationFinalized();
	if (OutstandingCompileShaderMapIds.Num() == 0)
	{
		return true;
	}
	return bRet;
}

void FNiagaraShaderScript::SetRenderThreadCachedData(const FNiagaraShaderMapCachedData& CachedData)
{
	CachedData_RenderThread = CachedData;
}

bool FNiagaraShaderScript::QueueForRelease(FThreadSafeBool& Fence)
{
	check(!bQueuedForRelease);

	if (BaseVMScript)
	{
		bQueuedForRelease = true;
		Fence = false;
		FThreadSafeBool* Released = &Fence;

		ENQUEUE_RENDER_COMMAND(BeginDestroyCommand)(
			[Released](FRHICommandListImmediate& RHICmdList)
			{
				*Released = true;
			});
	}

	return bQueuedForRelease;
}

void FNiagaraShaderScript::UpdateCachedData_All()
{
	UpdateCachedData_PreCompile();
	UpdateCachedData_PostCompile();
}

void FNiagaraShaderScript::UpdateCachedData_PreCompile()
{
	if (BaseVMScript)
	{
		TConstArrayView<FSimulationStageMetaData> SimulationStages = BaseVMScript->GetSimulationStageMetaData();
		NumPermutations = SimulationStages.Num();
	}
	else
	{
		NumPermutations = 0;
	}
}

void FNiagaraShaderScript::UpdateCachedData_PostCompile(bool bCalledFromSerialize)
{
	check(IsInGameThread() || bCalledFromSerialize);

	FNiagaraShaderMapCachedData CachedData;
	CachedData.NumPermutations = GetNumPermutations();
	CachedData.bIsComplete = 1;
	CachedData.bExternalConstantBufferUsed = 0;
	CachedData.bViewUniformBufferUsed = 0;

	if (GameThreadShaderMap != nullptr && GameThreadShaderMap->IsValid())
	{
		for (int32 iPermutation = 0; iPermutation < CachedData.NumPermutations; ++iPermutation)
		{
			TNiagaraShaderRef<FShader> Shader = GameThreadShaderMap->GetShader(&FNiagaraShader::StaticType, iPermutation);
			if (!Shader.IsValid())
			{
				CachedData.bIsComplete = 0;
				break;
			}
			FNiagaraShader* NiagaraShader = static_cast<FNiagaraShader*>(Shader.GetShader());

			for (int i = 0; i < 2; ++i)
			{
				const uint32 BitToSet = 1 << i;
				CachedData.bExternalConstantBufferUsed |= NiagaraShader->ExternalConstantBufferParam[i].IsBound() ? BitToSet : 0;
			}
			CachedData.bViewUniformBufferUsed |= NiagaraShader->bNeedsViewUniformBuffer;

			// request precache the compute shader
			if (IsResourcePSOPrecachingEnabled() || IsComponentPSOPrecachingEnabled())
			{
				check(NiagaraShader->GetFrequency() == SF_Compute)
				FRHIShader* RHIShader = GameThreadShaderMap->GetResource()->GetShader(Shader->GetResourceIndex());
				FRHIComputeShader* RHIComputeShader = static_cast<FRHIComputeShader*>(RHIShader);
				PipelineStateCache::PrecacheComputePipelineState(RHIComputeShader);
			}
		}
	}
	else
	{
		CachedData.bIsComplete = 0;
	}

	if (bCalledFromSerialize)
	{
		CachedData_RenderThread = MoveTemp(CachedData);
	}
	else if (!bQueuedForRelease)
	{
		ENQUEUE_RENDER_COMMAND(UpdateCachedData)(
				[Script_RT = this, CachedData_RT = CachedData](FRHICommandListImmediate& RHICmdList)
				{
					Script_RT->SetRenderThreadCachedData(CachedData_RT);
				});
	}
}

/**
* Cache the script's shaders
*/
#if WITH_EDITOR

bool FNiagaraShaderScript::CacheShaders(bool bApplyCompletedShaderMapForRendering, bool bForceRecompile, bool bSynchronous, const ITargetPlatform* TargetPlatform)
{
	FNiagaraShaderMapId NoStaticParametersId;
	GetShaderMapId(ShaderPlatform, TargetPlatform, NoStaticParametersId);
	return CacheShaders(NoStaticParametersId, bApplyCompletedShaderMapForRendering, bForceRecompile, bSynchronous);
}

/**
* Caches the shaders for this script
*/
bool FNiagaraShaderScript::CacheShaders(const FNiagaraShaderMapId& ShaderMapId, bool bApplyCompletedShaderMapForRendering, bool bForceRecompile, bool bSynchronous)
{
	bool bSucceeded = false;

	check(IsInGameThread());

	{
		GameThreadShaderMap = nullptr;
		{
			extern FCriticalSection GIdToNiagaraShaderMapCS;
			FScopeLock ScopeLock(&GIdToNiagaraShaderMapCS);
			// Find the script's cached shader map.
			GameThreadShaderMap = FNiagaraShaderMap::FindId(ShaderMapId, ShaderPlatform);
		}

		// Attempt to load from the derived data cache if we are uncooked
		if (!bForceRecompile && !GameThreadShaderMap && !FPlatformProperties::RequiresCookedData())
		{
			FNiagaraShaderMap::LoadFromDerivedDataCache(this, ShaderMapId, ShaderPlatform, GameThreadShaderMap);
			if (GameThreadShaderMap && GameThreadShaderMap->IsValid())
			{
				UE_LOG(LogShaders, Verbose, TEXT("Loaded shader %s for Niagara script %s from DDC"), *GameThreadShaderMap->GetFriendlyName(), *GetFriendlyName());
			}
			else
			{
				UE_LOG(LogShaders, Display, TEXT("Loading shader for Niagara script %s from DDC failed. Shader needs recompile."), *GetFriendlyName());
			}
		}
	}

	bool bAssumeShaderMapIsComplete = false;
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	bAssumeShaderMapIsComplete = FPlatformProperties::RequiresCookedData();
#endif

	UpdateCachedData_PreCompile();

	if (GameThreadShaderMap && GameThreadShaderMap->TryToAddToExistingCompilationTask(this))
	{
		//FNiagaraShaderMap::ShaderMapsBeingCompiled.Find(GameThreadShaderMap);
#if DEBUG_INFINITESHADERCOMPILE
		UE_LOG(LogTemp, Display, TEXT("Found existing compiling shader for Niagara script %s, linking to other GameThreadShaderMap 0x%08X%08X"), *GetFriendlyName(), (int)((int64)(GameThreadShaderMap.GetReference()) >> 32), (int)((int64)(GameThreadShaderMap.GetReference())));
#endif
		OutstandingCompileShaderMapIds.AddUnique(GameThreadShaderMap->GetCompilingId());
		UE_LOG(LogShaders, Log, TEXT("CacheShaders AddUniqueExisting %p %d"), this, GameThreadShaderMap->GetCompilingId());

		// Reset the shader map so we fall back to CPU sim until the compile finishes.
		GameThreadShaderMap = nullptr;
		bSucceeded = true;
	}
	else if (bForceRecompile || !GameThreadShaderMap || !(bAssumeShaderMapIsComplete || GameThreadShaderMap->IsComplete(this, false)))
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogShaders, Log, TEXT("Can't compile %s with cooked content!"), *GetFriendlyName());
			// Reset the shader map so we fall back to CPU sim
			GameThreadShaderMap = nullptr;
		}
		else
		{
			UE_LOG(LogShaders, Log, TEXT("%s cached shader map for script %s, compiling."), GameThreadShaderMap? TEXT("Incomplete") : TEXT("Missing"), *GetFriendlyName());

			// If there's no cached shader map for this script compile a new one.
			// This is just kicking off the compile, GameThreadShaderMap will not be complete yet
			bSucceeded = BeginCompileShaderMap(ShaderMapId, GameThreadShaderMap, bApplyCompletedShaderMapForRendering, bSynchronous);

			if (!bSucceeded)
			{
				GameThreadShaderMap = nullptr;
			}
		}
	}
	else
	{
		bSucceeded = true;
	}

	UpdateCachedData_PostCompile();

	if (!bQueuedForRelease)
	{
		FNiagaraShaderScript* Script = this;
		FNiagaraShaderMap* LoadedShaderMap = GameThreadShaderMap;
		ENQUEUE_RENDER_COMMAND(FSetShaderMapOnScriptResources)(
			[Script, LoadedShaderMap](FRHICommandListImmediate& RHICmdList)
			{
				Script->SetRenderingThreadShaderMap(LoadedShaderMap);
			});
	}

	return bSucceeded;
}

void FNiagaraShaderScript::FinishCompilation()
{
	TArray<int32> ShaderMapIdsToFinish;
	GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToFinish);

	if (ShaderMapIdsToFinish.Num() > 0)
	{
		for (int32 i = 0; i < ShaderMapIdsToFinish.Num(); i++)
		{
			UE_LOG(LogShaders, Log, TEXT("FinishCompilation()[%d] %s id %d!"), i, *GetFriendlyName(), ShaderMapIdsToFinish[i]);
		}
		// Block until the shader maps that we will save have finished being compiled
		// NIAGARATODO: implement when async compile works
		GNiagaraShaderCompilationManager.FinishCompilation(*GetFriendlyName(), ShaderMapIdsToFinish);

		// Shouldn't have anything left to do...
		TArray<int32> ShaderMapIdsToFinish2;
		GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToFinish2);
		if (ShaderMapIdsToFinish2.Num() != 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("Skipped multiple Niagara shader maps for compilation! May be indicative of no support for a given platform. Count: %d"), ShaderMapIdsToFinish2.Num());
		}
	}
}

#endif

void FNiagaraShaderScript::BuildScriptParametersMetadata(const FNiagaraShaderScriptParametersMetadata& InScriptParametersMetadata)
{
	TSharedRef<FNiagaraShaderScriptParametersMetadata> NewMetadata = MakeShared<FNiagaraShaderScriptParametersMetadata>();
	NewMetadata->DataInterfaceParamInfo = InScriptParametersMetadata.DataInterfaceParamInfo;

	FShaderParametersMetadataBuilder ShaderMetadataBuilder(TShaderParameterStructTypeInfo<FNiagaraShader::FParameters>::GetStructMetadata());

	// Build meta data for each data interface
	INiagaraShaderModule* ShaderModule = INiagaraShaderModule::Get();
	for (FNiagaraDataInterfaceGPUParamInfo& DataInterfaceParamInfo : NewMetadata->DataInterfaceParamInfo)
	{
		UNiagaraDataInterfaceBase* CDODataInterface = ShaderModule->RequestDefaultDataInterface(*DataInterfaceParamInfo.DIClassName);
		if (CDODataInterface == nullptr)
		{
			Invalidate();
			continue;
		}

		if (CDODataInterface->UseLegacyShaderBindings() == false)
		{
			const uint32 NextMemberOffset = ShaderMetadataBuilder.GetNextMemberOffset();
			FNiagaraShaderParametersBuilder ShaderParametersBuilder(DataInterfaceParamInfo, NewMetadata->LooseMetadataNames, NewMetadata->StructIncludeInfos, ShaderMetadataBuilder);
			CDODataInterface->BuildShaderParameters(ShaderParametersBuilder);
			DataInterfaceParamInfo.ShaderParametersOffset = NextMemberOffset;
		}
	}

	NewMetadata->ShaderParametersMetadata = MakeShareable<FShaderParametersMetadata>(ShaderMetadataBuilder.Build(FShaderParametersMetadata::EUseCase::ShaderParameterStruct, TEXT("FNiagaraShaderScript")));

	// There are paths in the editor / uncooked game where we will rebuild metadata while the system is running.
	// We don't pause the systems in those instances and nothing will actually change in the generated metadata,
	// therefore we can enqueue the release to the render thread.
	// Note: If we ever have different shaders at different quality levels we will need to replicate to RT
	if (ScriptParametersMetadata->ShaderParametersMetadata.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(ReleaseShaderMetadata)([MetaDataToRelease_RT = ScriptParametersMetadata](FRHICommandListImmediate&) {});
	}
	ScriptParametersMetadata = NewMetadata;
}

FNiagaraShaderRef FNiagaraShaderScript::GetShader(int32 PermutationId) const
{
	check(!GIsThreadedRendering || !IsInGameThread());
	if (!GIsEditor || RenderingThreadShaderMap /*&& RenderingThreadShaderMap->IsComplete(this, true)*/)
	{
		return RenderingThreadShaderMap->GetShader<FNiagaraShader>(PermutationId);
	}
	return FNiagaraShaderRef();
};

FNiagaraShaderRef FNiagaraShaderScript::GetShaderGameThread(int32 PermutationId) const
{
	if (GameThreadShaderMap && GameThreadShaderMap->IsValid())
	{
		return GameThreadShaderMap->GetShader<FNiagaraShader>(PermutationId);
	}

	return FNiagaraShaderRef();
};


void FNiagaraShaderScript::GetShaderMapIDsWithUnfinishedCompilation(TArray<int32>& ShaderMapIds)
{
	// Build an array of the shader map Id's are not finished compiling.
	if (GameThreadShaderMap && GameThreadShaderMap.IsValid() && !GameThreadShaderMap->IsCompilationFinalized())
	{
		ShaderMapIds.Add(GameThreadShaderMap->GetCompilingId());
	}
	else if (OutstandingCompileShaderMapIds.Num() != 0)
	{
		ShaderMapIds.Append(OutstandingCompileShaderMapIds);
	}
}

#if WITH_EDITOR

/**
* Compiles this script for Platform, storing the result in OutShaderMap
*
* @param ShaderMapId - the set of static parameters to compile
* @param Platform - the platform to compile for
* @param OutShaderMap - the shader map to compile
* @return - true if compile succeeded or was not necessary (shader map for ShaderMapId was found and was complete)
*/
bool FNiagaraShaderScript::BeginCompileShaderMap(
	const FNiagaraShaderMapId& ShaderMapId,
	TRefCountPtr<FNiagaraShaderMap>& OutShaderMap,
	bool bApplyCompletedShaderMapForRendering,
	bool bSynchronous)
{
	check(IsInGameThread());
#if WITH_EDITORONLY_DATA
	bool bSuccess = false;

	STAT(double NiagaraCompileTime = 0);


	SCOPE_SECONDS_COUNTER(NiagaraCompileTime);

	// Queue hlsl generation and shader compilation - Unlike materials, we queue this here, and compilation happens from the editor module
	TRefCountPtr<FNiagaraShaderMap> NewShaderMap = new FNiagaraShaderMap();
	OutstandingCompileShaderMapIds.AddUnique(NewShaderMap->GetCompilingId());		
	UE_LOG(LogShaders, Log, TEXT("BeginCompileShaderMap AddUnique %p %d"), this, NewShaderMap->GetCompilingId());

	FNiagaraCompilationQueue::Get()->Queue(this, NewShaderMap, ShaderMapId, ShaderPlatform, bApplyCompletedShaderMapForRendering);
	if (bSynchronous)
	{
		INiagaraShaderModule NiagaraShaderModule = FModuleManager::GetModuleChecked<INiagaraShaderModule>(TEXT("NiagaraShader"));
		NiagaraShaderModule.ProcessShaderCompilationQueue();
		OutShaderMap = NewShaderMap;
	}
	else
	{
		// For async compile, set to nullptr so that we fall back to CPU side simulation until shader compile is finished
		OutShaderMap = nullptr;
	}

	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_NiagaraShaders, (float)NiagaraCompileTime);

	return true;
#else
	UE_LOG(LogShaders, Fatal, TEXT("Compiling of shaders in a build without editordata is not supported."));
	return false;
#endif
}

FNiagaraCompileEventSeverity FNiagaraCVarUtilities::GetCompileEventSeverityForFailIfNotSet()
{
	switch (GNiagaraTranslatorFailIfNotSetSeverity) {
	case 3:
		return FNiagaraCompileEventSeverity::Error;
	case 2:
		return FNiagaraCompileEventSeverity::Warning;
	case 1:
		return FNiagaraCompileEventSeverity::Log;
	default:
		return FNiagaraCompileEventSeverity::Log;
	};
}

bool FNiagaraCVarUtilities::GetShouldEmitMessagesForFailIfNotSet()
{
	return GNiagaraTranslatorFailIfNotSetSeverity != 0;
}

#endif

