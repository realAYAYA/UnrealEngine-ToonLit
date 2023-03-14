// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemInstanceController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

FNiagaraSystemInstanceController::FNiagaraSystemInstanceController()
	: bNeedsRendererRecache(false)
	, bNeedsOverrideParametersTicked(false)
	, bNeedsUpdateEmitterMaterials(false)
{
}

void FNiagaraSystemInstanceController::Initialize(UWorld& World, UNiagaraSystem& System, FNiagaraUserRedirectionParameterStore* InOverrideParameters,
	USceneComponent* AttachComponent, ENiagaraTickBehavior TickBehavior, bool bPooled, int32 RandomSeedOffset, bool bForceSolo)
{
	OverrideParameters = InOverrideParameters;

	FNiagaraSystemInstance::AllocateSystemInstance(SystemInstance, World, System, OverrideParameters, AttachComponent, TickBehavior, bPooled);
	//UE_LOG(LogNiagara, Log, TEXT("Create System: %p | %s\n"), SystemInstance.Get(), *GetAsset()->GetFullName());
	SystemInstance->SetRandomSeedOffset(RandomSeedOffset);
	SystemInstance->Init(bForceSolo);

	WorldManager = FNiagaraWorldManager::Get(&World);
	check(WorldManager);

	UpdateEmitterMaterials(); // On system reset we want to always reinit materials for now. Hopefully we can recycle the already created Mids.
}

void FNiagaraSystemInstanceController::Release()
{
	checkf(!SystemInstance.IsValid() || SystemInstance->IsComplete(), TEXT("NiagraSystemInstance must be complete when releasing the controller.  System(%s) Component(%s)"), *GetNameSafe(SystemInstance->GetSystem()), *GetFullNameSafe(SystemInstance->GetAttachComponent()));

	// Rather than setting the ptr to null here, we allow it to transition ownership to the system's deferred deletion queue. This allows us to safely
	// get rid of the system interface should we be doing this in response to a callback invoked during the system interface's lifetime completion cycle.
	FNiagaraSystemInstance::DeallocateSystemInstance(SystemInstance);
	WorldManager = nullptr;
	OverrideParameters = nullptr;
	OnMaterialsUpdatedDelegate.Unbind();
}

bool FNiagaraSystemInstanceController::HasValidSimulation() const
{
	if (SystemInstance.IsValid())
	{
		return SystemInstance->GetSystemSimulation().IsValid();
	}

	return false;
}

FNiagaraSystemRenderData* FNiagaraSystemInstanceController::CreateSystemRenderData(ERHIFeatureLevel::Type FeatureLevel) const
{
	if (auto SystemInst = SystemInstance.Get())
	{
		// We can't safely update emitter materials here because it can be called from non-game thread
		return new FNiagaraSystemRenderData(*this, *SystemInst, FeatureLevel);
	}
	return nullptr;
}

void FNiagaraSystemInstanceController::GenerateSetDynamicDataCommands(FNiagaraSystemRenderData::FSetDynamicDataCommandList& Commands, FNiagaraSystemRenderData& RenderData, const FNiagaraSceneProxy& SceneProxy)
{
	RenderData.GenerateSetDynamicDataCommands(Commands, SceneProxy, SystemInstance.Get(), EmitterMaterials);
}

void FNiagaraSystemInstanceController::PostTickRenderers(FNiagaraSystemRenderData& RenderData)
{
	if (auto SystemInst = SystemInstance.Get())
	{
		if (bNeedsOverrideParametersTicked)
		{
			bNeedsOverrideParametersTicked = false;
			OverrideParameters->ResolvePositions(SystemInst->GetLWCConverter());
			OverrideParameters->Tick();
		}
		if (bNeedsUpdateEmitterMaterials)
		{
			bNeedsUpdateEmitterMaterials = false;
			UpdateEmitterMaterials();
		}
		if (bNeedsRendererRecache)
		{
			RenderData.RecacheRenderers(*SystemInst, *this);
			bNeedsRendererRecache = false;
		}
		RenderData.PostTickRenderers(*SystemInst);
	}
}

void FNiagaraSystemInstanceController::NotifyRenderersComplete(FNiagaraSystemRenderData& RenderData)
{
	if (auto SystemInst = SystemInstance.Get())
	{
		RenderData.OnSystemComplete(*SystemInst);
	}
}

void FNiagaraSystemInstanceController::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials)
{
	if (!SystemInstance.IsValid())
	{
		return;
	}

	for (TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst : SystemInstance->GetEmitters())
	{
		if (FVersionedNiagaraEmitterData* EmitterData = EmitterInst->GetCachedEmitterData())
		{
			EmitterData->ForEachEnabledRenderer(
				[&](UNiagaraRendererProperties* Properties)
				{
					bool bCreateMidsForUsedMaterials = Properties->NeedsMIDsForMaterials();
					TArray<UMaterialInterface*> Mats;
					Properties->GetUsedMaterials(&EmitterInst.Get(), Mats);

					if (bCreateMidsForUsedMaterials)
					{
						for (const FMaterialOverride& Override : EmitterMaterials)
						{
							if (Override.EmitterRendererProperty == Properties &&
								Mats.IsValidIndex(Override.MaterialSubIndex))
							{
								Mats[Override.MaterialSubIndex] = Override.Material;
							}
						}
					}

					OutMaterials.Append(Mats);
				}
			);
		}
	}
}

void FNiagaraSystemInstanceController::GetStreamingMeshInfo(const FBoxSphereBounds& OwnerBounds, FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const
{
	if (!SystemInstance.IsValid())
	{
		return;
	}

	for (TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst : SystemInstance->GetEmitters())
	{
		if (FVersionedNiagaraEmitterData* EmitterData = EmitterInst->GetCachedEmitter().GetEmitterData())
		{
			EmitterData->ForEachEnabledRenderer([&](UNiagaraRendererProperties* Properties)
			{
				Properties->GetStreamingMeshInfo(OwnerBounds, &EmitterInst.Get(), OutStreamingRenderAssets);
			});
		}
	}
}

UMaterialInterface* FNiagaraSystemInstanceController::GetMaterialOverride(const UNiagaraRendererProperties* InProps, int32 InMaterialSubIndex) const
{
	for (const FMaterialOverride& Override : EmitterMaterials)
	{
		if (Override.EmitterRendererProperty == InProps && InMaterialSubIndex == Override.MaterialSubIndex)
		{
			return Override.Material;
		}
	}

	return nullptr;
}

bool FNiagaraSystemInstanceController::GetParticleValueVec3_DebugOnly(TArray<FVector>& OutValues, FName EmitterName, FName ValueName) const
{
	if (SystemInstance.IsValid())
	{
		for (TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& Sim : SystemInstance->GetEmitters())
		{
			if (Sim->GetEmitterHandle().GetName() == EmitterName)
			{
				FNiagaraDataBuffer& ParticleData = Sim->GetData().GetCurrentDataChecked();
				int32 NumParticles = ParticleData.GetNumInstances();
				OutValues.SetNum(NumParticles);

				const auto Reader = FNiagaraDataSetAccessor<FVector3f>::CreateReader(Sim->GetData(), ValueName);
				if (!Reader.IsValid())
				{
					return false;
				}

				for (int32 i = 0; i < NumParticles; ++i)
				{
					OutValues[i] = (FVector)Reader.GetSafe(i, FVector3f::ZeroVector);
				}

				break;
			}
		}
	}

	return true;
}

bool FNiagaraSystemInstanceController::GetParticleValues_DebugOnly(TArray<float>& OutValues, FName EmitterName, FName ValueName) const
{
	if (SystemInstance.IsValid())
	{
		for (TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& Sim : SystemInstance->GetEmitters())
		{
			if (Sim->GetEmitterHandle().GetName() == EmitterName)
			{
				FNiagaraDataBuffer& ParticleData = Sim->GetData().GetCurrentDataChecked();
				int32 NumParticles = ParticleData.GetNumInstances();
				OutValues.SetNum(NumParticles);

				const auto Reader = FNiagaraDataSetAccessor<float>::CreateReader(Sim->GetData(), ValueName);
				if (!Reader.IsValid())
				{
					return false;
				}

				for (int32 i = 0; i < NumParticles; ++i)
				{
					OutValues[i] = Reader.GetSafe(i, 0.0f);
				}

				break;
			}
		}
	}

	return true;
}

void FNiagaraSystemInstanceController::DebugDump(bool bFullDump)
{
	if (SystemInstance.IsValid())
	{
		const UEnum* ExecutionStateEnum = StaticEnum<ENiagaraExecutionState>();
		UE_LOG(LogNiagara, Log, TEXT("\tSystem ExecutionState(%s) RequestedExecutionState(%s)"), *ExecutionStateEnum->GetNameStringByIndex((int32)SystemInstance->GetActualExecutionState()), *ExecutionStateEnum->GetNameStringByIndex((int32)SystemInstance->GetRequestedExecutionState()));

		if (!SystemInstance->IsComplete())
		{
			for (TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe> Emitter : SystemInstance->GetEmitters())
			{
				if ( Emitter->GetCachedEmitter().Emitter != nullptr )
				{
					UE_LOG(LogNiagara, Log, TEXT("\tEmitter '%s' ExecutionState(%s) NumParticles(%d)"), *Emitter->GetEmitterHandle().GetUniqueInstanceName(), *ExecutionStateEnum->GetNameStringByIndex((int32)Emitter->GetExecutionState()), Emitter->GetNumParticles());
				}
			}
		}

		if (bFullDump)
		{
			UE_LOG(LogNiagara, Log, TEXT("=========================== Begin Full Dump ==========================="));
			SystemInstance->Dump();
			UE_LOG(LogNiagara, Log, TEXT("=========================== End Full Dump ==========================="));
		}
	}
}

void FNiagaraSystemInstanceController::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Override : EmitterMaterials)
	{
		Collector.AddReferencedObject(Override.Material);

		// NOTE: Intentionally *not* adding a reference to EmitterRendererProperty because it's only used as an identifier
	}
}

void FNiagaraSystemInstanceController::UpdateEmitterMaterials()
{
	check(IsInRenderingThread() || IsInGameThread() || IsAsyncLoading() || GIsSavingPackage); // Same restrictions as MIDs

	TArray<FMaterialOverride> NewEmitterMaterials;
	if (SystemInstance)
	{
		for (int32 i = 0; i < SystemInstance->GetEmitters().Num(); i++)
		{
			FNiagaraEmitterInstance* EmitterInst = &SystemInstance->GetEmitters()[i].Get();
			if (FVersionedNiagaraEmitterData* EmitterData = EmitterInst->GetCachedEmitterData())
			{
				EmitterData->ForEachEnabledRenderer(
					[&](UNiagaraRendererProperties* Properties)
					{
						// Nothing to do if we don't create MIDs for this material
						if (!Properties->NeedsMIDsForMaterials())
						{
							return;
						}

						TArray<UMaterialInterface*> UsedMaterials;
						Properties->GetUsedMaterials(EmitterInst, UsedMaterials);

						uint32 MaterialIndex = 0;
						for (UMaterialInterface*& ExistingMaterial : UsedMaterials)
						{
							if (ExistingMaterial)
							{
								if (auto MID = Cast<UMaterialInstanceDynamic>(ExistingMaterial))
								{
									if (EmitterMaterials.FindByPredicate([&](const FMaterialOverride& ExistingOverride) -> bool { return (ExistingOverride.Material == ExistingMaterial) && (ExistingOverride.EmitterRendererProperty == Properties) && (ExistingOverride.MaterialSubIndex == MaterialIndex); }))
									{
										// It's a MID we've previously created and are managing. Recreate it by grabbing the parent.
										// TODO: Are there cases where we don't always have to recreate it?
										ExistingMaterial = MID->Parent;
									}
								}

								// Create a new MID
								//UE_LOG(LogNiagara, Log, TEXT("Create Dynamic Material for component %s"), *GetPathName());
								ExistingMaterial = UMaterialInstanceDynamic::Create(ExistingMaterial, nullptr);
								FMaterialOverride Override;
								Override.Material = ExistingMaterial;
								Override.EmitterRendererProperty = Properties;
								Override.MaterialSubIndex = MaterialIndex;

								NewEmitterMaterials.Add(Override);
							}
							++MaterialIndex;
						}
					}
				);
			}
		}
	}
	EmitterMaterials = MoveTemp(NewEmitterMaterials);
	OnMaterialsUpdatedDelegate.ExecuteIfBound();
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, bool InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetBoolDef(), InVariableName);
		const FNiagaraBool BoolValue(InValue);
		OverrideParameters->SetParameterValue(BoolValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, int32 InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetIntDef(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, float InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetFloatDef(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, FVector2f InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetVec2Def(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, FVector3f InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetVec3Def(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, FVector InValue)
{
	if (OverrideParameters)
	{
		OverrideParameters->SetPositionParameterValue(InValue, InVariableName, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, FVector4f InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetVec4Def(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, FLinearColor InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetColorDef(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, FQuat4f InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetQuatDef(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, const FMatrix44f& InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetMatrix4Def(), InVariableName);
		OverrideParameters->SetParameterValue(InValue, VariableDesc, true);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, TWeakObjectPtr<UObject> InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetUObjectDef(), InVariableName);
		OverrideParameters->SetUObject(InValue.Get(), VariableDesc);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, TWeakObjectPtr<UMaterialInterface> InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetUMaterialDef(), InVariableName);
		UObject* CurrentValue = OverrideParameters->GetUObject(VariableDesc);
		UMaterialInterface* NewValue = InValue.Get();
		OverrideParameters->SetUObject(NewValue, VariableDesc);
		if (CurrentValue != NewValue)
		{
			bNeedsUpdateEmitterMaterials = true; // Will need to update our internal tables. Maybe need a new MID.
		}
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, TWeakObjectPtr<UStaticMesh> InValue)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetUStaticMeshDef(), InVariableName);
		UObject* CurrentValue = OverrideParameters->GetUObject(VariableDesc);
		UStaticMesh* NewValue = InValue.Get();
		OverrideParameters->SetUObject(NewValue, VariableDesc);
		if (CurrentValue != NewValue)
		{
			bNeedsOverrideParametersTicked = true;
			bNeedsUpdateEmitterMaterials = true;
			OnNeedsRendererRecache();
		}
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, TWeakObjectPtr<UTexture> Texture)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetUTextureDef(), InVariableName);
		OverrideParameters->SetUObject(Texture.Get(), VariableDesc);
	}
}

void FNiagaraSystemInstanceController::SetVariable(FName InVariableName, TWeakObjectPtr<UTextureRenderTarget> TextureRenderTarget)
{
	if (OverrideParameters)
	{
		const FNiagaraVariable VariableDesc(FNiagaraTypeDefinition::GetUTextureRenderTargetDef(), InVariableName);
		OverrideParameters->SetUObject(TextureRenderTarget.Get(), VariableDesc);
	}
}