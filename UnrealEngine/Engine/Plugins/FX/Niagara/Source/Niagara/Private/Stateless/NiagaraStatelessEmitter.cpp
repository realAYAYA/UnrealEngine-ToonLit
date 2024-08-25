// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/NiagaraStatelessEmitterData.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessDrawDebugContext.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "NiagaraConstants.h"
#include "NiagaraModule.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSettings.h"
#include "NiagaraSystem.h"

#include "Algo/Copy.h"
#include "Interfaces/ITargetPlatform.h"

namespace NiagaraStatelessInternal
{
	static int32 GetDataSetIntOffset(const FNiagaraDataSetCompiledData& CompiledData, const FNiagaraVariableBase& Variable)
	{
		if (const FNiagaraVariableLayoutInfo* Layout = CompiledData.FindVariableLayoutInfo(Variable))
		{
			if (Layout->GetNumInt32Components() > 0)
			{
				return Layout->GetInt32ComponentStart();
			}
		}
		return INDEX_NONE;
	}

	static int32 GetDataSetFloatOffset(const FNiagaraDataSetCompiledData& CompiledData, const FNiagaraVariableBase& Variable)
	{
		if (const FNiagaraVariableLayoutInfo* Layout = CompiledData.FindVariableLayoutInfo(Variable))
		{
			if (Layout->GetNumFloatComponents() > 0)
			{
				return Layout->GetFloatComponentStart();
			}
		}
		return INDEX_NONE;
	}

	bool IsValid(const FNiagaraStatelessEmitterData& EmitterData)
	{
		// No spawn info / no renderers == no point doing anything
		//-TODO: If we allow particle reads this might be different
		if (!EmitterData.SpawnInfos.Num() || !EmitterData.RendererProperties.Num())
		{
			return false;
		}

		// Validate we have a template assigned
		if (!EmitterData.EmitterTemplate)
		{
			return false;
		}

		// Lifetime is not valid so we have nothing to every render with
		if ((EmitterData.LifetimeRange.Min <= 0.0f) && (EmitterData.LifetimeRange.Max <= 0.0f))
		{
			return false;
		}

		// Validate the shader is correct
		const FShaderParametersMetadata* ShaderParametersMetadata = EmitterData.GetShaderParametersMetadata();
		if (!EmitterData.GetShader().IsValid() || !ShaderParametersMetadata)
		{
			return false;
		}

		if (!ensureMsgf(ShaderParametersMetadata->GetLayout().UniformBuffers.Num() == 0, TEXT("UniformBuffers are not supported in stateless simulations currently")))
		{
			// We don't support this as it would require a pass to clear out the buffer pointers to avoid leaks
			return false;
		}

		// Test the the first member in the shader struct is the common parameter block otherwise the shader is invalid
		{
			const TArray<FShaderParametersMetadata::FMember> ShaderParameterMembers = ShaderParametersMetadata->GetMembers();
			if (ShaderParameterMembers.Num() == 0 ||
				ShaderParameterMembers[0].GetBaseType() != UBMT_INCLUDED_STRUCT ||
				ShaderParameterMembers[0].GetStructMetadata()->GetLayout() != NiagaraStateless::FCommonShaderParameters::FTypeInfo::GetStructMetadata()->GetLayout() )
			{
				ensureMsgf(false, TEXT("NiagaraStateless::FCommonShaderParameters must be included first in your shader parameters structure"));
				return false;
			}
		}

		return true;
	}
}

void UNiagaraStatelessEmitter::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// Ensure our module list is up to date
	OnEmitterTemplateChanged();
#endif
}

bool UNiagaraStatelessEmitter::NeedsLoadForTargetPlatform(const ITargetPlatform* TargetPlatform) const
{
	// Don't load disabled emitters.
	// Awkwardly, this requires us to look for ourselves in the owning system.
	const UNiagaraSystem* OwnerSystem = GetTypedOuter<const UNiagaraSystem>();
	if (OwnerSystem)
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : OwnerSystem->GetEmitterHandles())
		{
			if (EmitterHandle.GetStatelessEmitter() == this)
			{
				if (!EmitterHandle.GetIsEnabled())
				{
					return false;
				}
				break;
			}
		}
	}

	if (!FNiagaraPlatformSet::ShouldPruneEmittersOnCook(TargetPlatform->IniPlatformName()))
	{
		return true;
	}

	if (OwnerSystem && !OwnerSystem->GetScalabilityPlatformSet().IsEnabledForPlatform(TargetPlatform->IniPlatformName()))
	{
		UE_LOG(LogNiagara, Verbose, TEXT("Pruned emitter %s for platform %s from system scalability"), *GetFullName(), *TargetPlatform->DisplayName().ToString());
		return false;
	}

	const bool bStatelessEnabled = GetDefault<UNiagaraSettings>()->bStatelessEmittersEnabled;
	const bool bIsEnabled = Platforms.IsEnabledForPlatform(TargetPlatform->IniPlatformName()) && bStatelessEnabled;
	if (!bIsEnabled)
	{
		UE_LOG(LogNiagara, Verbose, TEXT("Pruned emitter %s for platform %s"), *GetFullName(), *TargetPlatform->DisplayName().ToString())
	}
	return bIsEnabled;
}

#if WITH_EDITOR
void UNiagaraStatelessEmitter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FNiagaraDistributionBase::PostEditChangeProperty(this, PropertyChangedEvent);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	//-TODO: This should be done differently
	if (UNiagaraSystem* NiagaraSystem = GetTypedOuter<UNiagaraSystem>())
	{
		FNiagaraSystemUpdateContext UpdateContext(NiagaraSystem, true);

		// Ensure our template is up to date, right not it's easy to remove things by accident and cause a crash
		//if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UNiagaraStatelessEmitter, EmitterTemplateClass))
		{
			OnEmitterTemplateChanged();
		}
	}
}

void UNiagaraStatelessEmitter::OnEmitterTemplateChanged()
{
	// No template nothing to do
	const UNiagaraStatelessEmitterTemplate* EmitterTemplate = GetEmitterTemplate();
	if (EmitterTemplate == nullptr)
	{
		Modules.Empty();
		return;
	}

	// Null modules exist if the module was removed by accident or the source object was removed
	Modules.RemoveAll([](UNiagaraStatelessModule* Module) { return Module == nullptr; });

	// Order modules according to the new template and switch any existing modules over directly.
	//-Note: We could store old customized modules as editor only data for easy switching between templates
	TArray<TObjectPtr<UNiagaraStatelessModule>> NewModules;
	NewModules.Reserve(EmitterTemplate->GetModules().Num());
	for (UClass* ModuleClass : EmitterTemplate->GetModules())
	{
		const int32 ExistingIndex = Modules.IndexOfByPredicate([ModuleClass](UNiagaraStatelessModule* Existing) { return Existing->GetClass() == ModuleClass; });
		if (ExistingIndex == INDEX_NONE)
		{
			UNiagaraStatelessModule* NewModule = NewObject<UNiagaraStatelessModule>(this, ModuleClass, NAME_None, RF_Transactional);
			if (NewModule->CanDisableModule())
			{
				NewModule->SetIsModuleEnabled(false);
			}
			NewModules.Add(NewModule);
		}
		else
		{
			NewModules.Add(Modules[ExistingIndex]);
			Modules.RemoveAtSwap(ExistingIndex, 1, EAllowShrinking::No);
		}
	}
	Modules = MoveTemp(NewModules);
}
#endif //WITH_EDITOR

const UNiagaraStatelessEmitterTemplate* UNiagaraStatelessEmitter::GetEmitterTemplate() const
{
	return EmitterTemplateClass ? EmitterTemplateClass->GetDefaultObject<UNiagaraStatelessEmitterTemplate>() : nullptr;
}

void UNiagaraStatelessEmitter::CacheFromCompiledData()
{
	StatelessEmitterData = MakeShareable(new FNiagaraStatelessEmitterData(), FNiagaraStatelessEmitterData::FDeleter());
	StatelessEmitterData->EmitterTemplate = GetEmitterTemplate();

	// Build data set
	BuildCompiledDataSet();

	// Setup emitter state
	StatelessEmitterData->EmitterState = EmitterState;

	// Find lifetime values
	//-Note: We could abstact this out to be a more general modules implements Lifetime but this mirrors core Niagara where you always have Initialize Particle in 99% of cases
	if (StatelessEmitterData->EmitterTemplate)
	{
		UNiagaraStatelessModule_InitializeParticle* InitializeParticleModule = nullptr;
		Modules.FindItemByClass(&InitializeParticleModule);
		if (ensure(InitializeParticleModule))
		{
			StatelessEmitterData->LifetimeRange = InitializeParticleModule->LifetimeDistribution.CalculateRange();
		}
	}

	// Fill in common data, removing any spawn infos that are invalid to the simulation
	StatelessEmitterData->bDeterministic = bDeterministic;
	StatelessEmitterData->RandomSeed = RandomSeed ^ 0xdefa081;
	StatelessEmitterData->FixedBounds = FixedBounds;

	StatelessEmitterData->SpawnInfos.Reserve(SpawnInfos.Num());
	Algo::CopyIf(
		SpawnInfos,
		StatelessEmitterData->SpawnInfos,
		[this](const FNiagaraStatelessSpawnInfo& SpawnInfo)
		{
			return SpawnInfo.IsValid(EmitterState.LoopDuration.Max);
		}
	);

	// Fill in renderers
	StatelessEmitterData->RendererProperties = RendererProperties;
	for (UNiagaraRendererProperties* Renderer : RendererProperties)
	{
		if (Renderer && Renderer->GetIsEnabled())
		{
			Renderer->CacheFromCompiledData(&StatelessEmitterData->ParticleDataSetCompiledData);
		}
	}

	StatelessEmitterData->bCanEverExecute = NiagaraStatelessInternal::IsValid(*StatelessEmitterData);
	StatelessEmitterData->bCanEverExecute &= Platforms.IsActive();
	StatelessEmitterData->bCanEverExecute &= GetDefault<UNiagaraSettings>()->bStatelessEmittersEnabled;

	// Build buffers that are shared across all instances
	//-OPT: We should be able to build and serialize this data as part of the UNiagaraStatelessEmitter, potentially all of this data even since it's immutable and does not change at runtime
	if (StatelessEmitterData->bCanEverExecute)
	{
		FNiagaraStatelessEmitterDataBuildContext EmitterBuildContext(
			StatelessEmitterData->RendererBindings,
			StatelessEmitterData->BuiltData,
			StatelessEmitterData->StaticFloatData
		);

		for (const UNiagaraStatelessModule* Module : Modules)
		{
			Module->BuildEmitterData(EmitterBuildContext);
		}
		StatelessEmitterData->bModulesHaveRendererBindings = StatelessEmitterData->RendererBindings.Num() > 0;

		if (StatelessEmitterData->StaticFloatData.Num() == 0)
		{
			StatelessEmitterData->StaticFloatData.Emplace(0.0f);
		}
		StatelessEmitterData->InitRenderResources();

		// Prepare renderer bindings this avoid having to do this per instance spawned
		// Note: Order is important here we detect if we need to update shader parameters on binding changes above by looking to see if we had any renderer bindings so this must be below
		for (UNiagaraRendererProperties* Renderer : RendererProperties )
		{
			if (Renderer && Renderer->GetIsEnabled())
			{
				Renderer->PopulateRequiredBindings(StatelessEmitterData->RendererBindings);
			}
		}
	}
}

void UNiagaraStatelessEmitter::BuildCompiledDataSet()
{
#if WITH_EDITORONLY_DATA
	ParticleDataSetCompiledData.Empty();
	ParticleDataSetCompiledData.SimTarget = ENiagaraSimTarget::GPUComputeSim;

	if (const UNiagaraStatelessEmitterTemplate* EmitterTemplate = StatelessEmitterData->EmitterTemplate)
	{
		// Gather a list of all the output variables from the modules, this can change based on what is enabled / disabled
		TArray<FNiagaraVariableBase> AvailableVariables;
		for (UNiagaraStatelessModule* Module : Modules)
		{
			if (Module->IsModuleEnabled())
			{
				Module->GetOutputVariables(AvailableVariables);
			}
		}

		// Remove any variables we don't output
		TConstArrayView<FNiagaraVariableBase> OutputComponents = EmitterTemplate->GetOututputComponents();
		for (auto it=AvailableVariables.CreateIterator(); it; ++it)
		{
			if (!OutputComponents.Contains(*it))
			{
				it.RemoveCurrent();
			}
		}

		// Build data set from variables that are used
		ForEachEnabledRenderer(
			[this, &AvailableVariables](UNiagaraRendererProperties* RendererProps)
			{
				if (AvailableVariables.Num() == 0)
				{
					return;
				}

				for (FNiagaraVariableBase BoundAttribute : RendererProps->GetBoundAttributes())
				{
					// Edge condition with UniqueID which does not contain the Particle namespace from Ribbon Renderer
					BoundAttribute.RemoveRootNamespace(FNiagaraConstants::ParticleAttributeNamespaceString);

					const int32 Index = AvailableVariables.IndexOfByKey(BoundAttribute);
					if (Index != INDEX_NONE)
					{
						AvailableVariables.RemoveAtSwap(Index, 1, EAllowShrinking::No);
						ParticleDataSetCompiledData.Variables.Emplace(BoundAttribute);
						if (AvailableVariables.Num() == 0)
						{
							return;
						}
					}
				}
			}
		);

		//-TODO: We can alias variables in the data set, for example PreviousSpriteFacing could be SpriteFacing in some cases
		ParticleDataSetCompiledData.BuildLayout();

		// Create mapping from output components to data set for the shader to output
		ComponentOffsets.Empty(OutputComponents.Num());
		for (const FNiagaraVariableBase& OutputComponent : OutputComponents)
		{
			if (OutputComponent.GetType() == FNiagaraTypeDefinition::GetIntDef())
			{
				ComponentOffsets.Add(NiagaraStatelessInternal::GetDataSetIntOffset(ParticleDataSetCompiledData, OutputComponent));
			}
			else
			{
				ComponentOffsets.Add(NiagaraStatelessInternal::GetDataSetFloatOffset(ParticleDataSetCompiledData, OutputComponent));
			}
		}
		ComponentOffsets.Shrink();
	}
#endif
	StatelessEmitterData->ParticleDataSetCompiledData = ParticleDataSetCompiledData;
	StatelessEmitterData->ComponentOffsets = ComponentOffsets;
}

bool UNiagaraStatelessEmitter::SetUniqueEmitterName(const FString& InName)
{
	if (InName != UniqueEmitterName)
	{
		Modify();
		FString OldName = UniqueEmitterName;
		UniqueEmitterName = InName;

		if (GetName() != InName)
		{
			// Also rename the underlying uobject to keep things consistent.
			FName UniqueObjectName = MakeUniqueObjectName(GetOuter(), StaticClass(), *InName);
			Rename(*UniqueObjectName.ToString(), GetOuter(), REN_ForceNoResetLoaders);
		}

//#if WITH_EDITORONLY_DATA
//		for (FVersionedNiagaraEmitterData& EmitterData : VersionData)
//		{
//			EmitterData.SyncEmitterAlias(OldName, *this);
//		}
//#endif
		return true;
	}

	return false;
}

NiagaraStateless::FCommonShaderParameters* UNiagaraStatelessEmitter::AllocateShaderParameters(const FNiagaraParameterStore& RendererBindings) const
{
	// Allocate parameters
	const FShaderParametersMetadata* ShaderParametersMetadata = StatelessEmitterData->GetShaderParametersMetadata();
	const int32 ShaderParametersSize = ShaderParametersMetadata->GetSize();
	void* UntypedShaderParameters = FMemory::Malloc(ShaderParametersSize, SHADER_PARAMETER_STRUCT_ALIGNMENT);
	FMemory::Memset(UntypedShaderParameters, 0, ShaderParametersSize);

	// Fill in all of the shader parameters
	FNiagaraStatelessSetShaderParameterContext SetShaderParametersContext(
		RendererBindings.GetParameterDataArray(),
		StatelessEmitterData->BuiltData,
		ShaderParametersMetadata,
		static_cast<uint8*>(UntypedShaderParameters)
	);

	NiagaraStateless::FCommonShaderParameters* CommonParameters = SetShaderParametersContext.GetParameterNestedStruct<NiagaraStateless::FCommonShaderParameters>();
	CommonParameters->Common_LifetimeScaleBias	= FVector2f(StatelessEmitterData->LifetimeRange.Max - StatelessEmitterData->LifetimeRange.Min, StatelessEmitterData->LifetimeRange.Min);

	for (UNiagaraStatelessModule* Module : Modules)
	{
		Module->SetShaderParameters(SetShaderParametersContext);
	}

	//-TODO: Add a way to set them directly, we should know that the final struct is a series of ints in the order of the provided variables
	GetEmitterTemplate()->SetShaderParameters(static_cast<uint8*>(UntypedShaderParameters), StatelessEmitterData->ComponentOffsets);

	return CommonParameters;
}

#if WITH_EDITOR
void UNiagaraStatelessEmitter::SetEmitterTemplateClass(UClass* TemplateClass)
{
	if (TemplateClass && !TemplateClass->IsChildOf(UNiagaraStatelessEmitterTemplate::StaticClass()))
	{
		TemplateClass = nullptr;
	}

	EmitterTemplateClass = TemplateClass;
	OnEmitterTemplateChanged();
}

void UNiagaraStatelessEmitter::AddRenderer(UNiagaraRendererProperties* Renderer, FGuid EmitterVersion)
{
	FNiagaraSystemUpdateContext UpdateContext;
	UpdateContext.SetDestroyOnAdd(true);
	if (UNiagaraSystem* Owner = GetTypedOuter<UNiagaraSystem>())
	{
		UpdateContext.Add(Owner, true);
	}

	Modify();
	Renderer->OuterEmitterVersion = EmitterVersion;
//	FVersionedNiagaraEmitterData* EmitterData = GetEmitterData(EmitterVersion);
	RendererProperties.Add(Renderer);
#if WITH_EDITOR
//	Renderer->OnChanged().AddUObject(this, &UNiagaraEmitter::RendererChanged);
//	UpdateChangeId(TEXT("Renderer added"));
	OnRenderersChangedDelegate.Broadcast();
#endif
//	EmitterData->RebuildRendererBindings(*this);
}

void UNiagaraStatelessEmitter::RemoveRenderer(UNiagaraRendererProperties* Renderer, FGuid EmitterVersion)
{
	FNiagaraSystemUpdateContext UpdateContext;
	UpdateContext.SetDestroyOnAdd(true);
	if (UNiagaraSystem* Owner = GetTypedOuter<UNiagaraSystem>())
	{
		UpdateContext.Add(Owner, true);
	}

	Modify();
//	FVersionedNiagaraEmitterData* EmitterData = GetEmitterData(EmitterVersion);
	RendererProperties.Remove(Renderer);
#if WITH_EDITOR
//	Renderer->OnChanged().RemoveAll(this);
//	UpdateChangeId(TEXT("Renderer removed"));
	OnRenderersChangedDelegate.Broadcast();
#endif
//	EmitterData->RebuildRendererBindings(*this);
}

void UNiagaraStatelessEmitter::MoveRenderer(UNiagaraRendererProperties* Renderer, int32 NewIndex, FGuid EmitterVersion)
{
//	FVersionedNiagaraEmitterData* EmitterData = GetEmitterData(EmitterVersion);
	int32 CurrentIndex = RendererProperties.IndexOfByKey(Renderer);
	if (CurrentIndex == INDEX_NONE || CurrentIndex == NewIndex || !RendererProperties.IsValidIndex(NewIndex))
	{
		return;
	}

	FNiagaraSystemUpdateContext UpdateContext;
	UpdateContext.SetDestroyOnAdd(true);
	if (UNiagaraSystem* Owner = GetTypedOuter<UNiagaraSystem>())
	{
		UpdateContext.Add(Owner, true);
	}

	Modify();
	RendererProperties.RemoveAt(CurrentIndex);
	RendererProperties.Insert(Renderer, NewIndex);
#if WITH_EDITOR
//	UpdateChangeId(TEXT("Renderer moved"));
	OnRenderersChangedDelegate.Broadcast();
#endif
//	EmitterData->RebuildRendererBindings(*this);
}

FNiagaraStatelessSpawnInfo& UNiagaraStatelessEmitter::AddSpawnInfo()
{
	FNiagaraSystemUpdateContext UpdateContext;
	UpdateContext.SetDestroyOnAdd(true);
	if (UNiagaraSystem* Owner = GetTypedOuter<UNiagaraSystem>())
	{
		UpdateContext.Add(Owner, true);
	}

	return SpawnInfos.AddDefaulted_GetRef();
}

void UNiagaraStatelessEmitter::RemoveSpawnInfoBySourceId(FGuid& InSourceIdToRemove)
{
	FNiagaraSystemUpdateContext UpdateContext;
	UpdateContext.SetDestroyOnAdd(true);
	if (UNiagaraSystem* Owner = GetTypedOuter<UNiagaraSystem>())
	{
		UpdateContext.Add(Owner, true);
	}

	SpawnInfos.RemoveAll([InSourceIdToRemove](const FNiagaraStatelessSpawnInfo& SpawnInfo) { return SpawnInfo.SourceId == InSourceIdToRemove; });
}

int32 UNiagaraStatelessEmitter::IndexOfSpawnInfoBySourceId(const FGuid& InSourceId)
{
	return SpawnInfos.IndexOfByPredicate([InSourceId](const FNiagaraStatelessSpawnInfo& SpawnInfo) { return SpawnInfo.SourceId == InSourceId; });
}

FNiagaraStatelessSpawnInfo* UNiagaraStatelessEmitter::FindSpawnInfoBySourceId(const FGuid& InSourceId)
{
	return SpawnInfos.FindByPredicate([InSourceId](const FNiagaraStatelessSpawnInfo& SpawnInfo) { return SpawnInfo.SourceId == InSourceId; });
}

FNiagaraStatelessSpawnInfo* UNiagaraStatelessEmitter::GetSpawnInfoByIndex(int32 Index)
{
	FNiagaraStatelessSpawnInfo* SpawnInfo = nullptr;
	if (Index >= 0 && Index < SpawnInfos.Num())
	{
		SpawnInfo = &SpawnInfos[Index];
	}
	return SpawnInfo;
}

UNiagaraStatelessEmitter* UNiagaraStatelessEmitter::CreateAsDuplicate(FName InDuplicateName, UNiagaraSystem& InDuplicateOwnerSystem) const
{
	UNiagaraStatelessEmitter* NewEmitter = Cast<UNiagaraStatelessEmitter>(StaticDuplicateObject(this, &InDuplicateOwnerSystem));
	NewEmitter->ClearFlags(RF_Standalone | RF_Public);
	NewEmitter->SetUniqueEmitterName(InDuplicateName.GetPlainNameString());

	return NewEmitter;
}

void UNiagaraStatelessEmitter::DrawModuleDebug(UWorld* World, const FTransform& LocalToWorld) const
{
	FNiagaraStatelessDrawDebugContext DrawDebugContext;
	DrawDebugContext.World					= World;
	DrawDebugContext.LocalToWorldTransform	= LocalToWorld;
	DrawDebugContext.WorldTransform			= FTransform::Identity;
	for (const UNiagaraStatelessModule* Module : Modules)
	{
		if (Module->IsDebugDrawEnabled())
		{
			Module->DrawDebug(DrawDebugContext);
		}
	}
}

#endif //WITH_EDITOR
