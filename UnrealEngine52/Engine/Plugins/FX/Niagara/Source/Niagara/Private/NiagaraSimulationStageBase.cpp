// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimulationStageBase.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystem.h"
#include "NiagaraScriptSourceBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSimulationStageBase)

const FName UNiagaraSimulationStageBase::ParticleSpawnUpdateName("ParticleSpawnUpdate");

namespace NiagaraSimulationStageLocal
{
	static FName NAME_Particles_StateIndex("Particles.StateIndex");
	static FNiagaraVariableBase GetDefaultEnabledBinding() { return FNiagaraVariableBase(FNiagaraTypeDefinition::GetBoolDef(), NAME_None); }
	static FNiagaraVariableBase GetDefaultElementCountBinding() { return FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), NAME_None); }
	static FNiagaraVariableBase GetDefaultNumIterationsBinding() { return FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), NAME_None); }
	static FNiagaraVariableBase GetDefaultParticleStateBinding() { return FNiagaraVariableBase(FNiagaraTypeDefinition::GetIntDef(), NAME_Particles_StateIndex); }
}

bool UNiagaraSimulationStageBase::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
#if WITH_EDITORONLY_DATA
	if (FNiagaraCompileHashVisitorDebugInfo* DebugInfo = InVisitor->AddDebugInfo())
	{
		DebugInfo->Object = FString::Printf(TEXT("Class: \"%s\"  Name: \"%s\""), *GetClass()->GetName(), *GetName());
	}
#endif
	InVisitor->UpdatePOD(TEXT("Enabled"), bEnabled ? 1 : 0);
	return true;
}

#if WITH_EDITOR
void UNiagaraSimulationStageBase::SetEnabled(bool bInEnabled)
{
	if (bEnabled != bInEnabled)
	{
		bEnabled = bInEnabled;
		RequestRecompile();
		OnChanged().Broadcast();
	}
}

void UNiagaraSimulationStageBase::RequestRecompile()
{
	if (!bEnabled)
	{
		Script->InvalidateCompileResults(TEXT("SimulationStage changed."));
	}

	FVersionedNiagaraEmitter OuterEmitter = GetOuterEmitter();
	if (FVersionedNiagaraEmitterData* EmitterData = OuterEmitter.GetEmitterData())
	{
		if (UNiagaraScriptSourceBase* GraphSource = EmitterData->UpdateScriptProps.Script->GetLatestSource())
		{
			GraphSource->MarkNotSynchronized(TEXT("SimulationStage changed."));
		}

		UNiagaraSystem::RequestCompileForEmitter(OuterEmitter);
	}
}

void UNiagaraSimulationStageBase::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageBase, bEnabled))
	{
		RequestRecompile();
	}
}
#endif

bool UNiagaraSimulationStageGeneric::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const 
{
	Super::AppendCompileHash(InVisitor);

	InVisitor->UpdateString(TEXT("EnabledBinding"), EnabledBinding.GetDataSetBindableVariable().GetName().ToString());
	InVisitor->UpdateString(TEXT("ElementCountXBinding"), ElementCountXBinding.GetDataSetBindableVariable().GetName().ToString());
	InVisitor->UpdateString(TEXT("ElementCountYBinding"), ElementCountYBinding.GetDataSetBindableVariable().GetName().ToString());
	InVisitor->UpdateString(TEXT("ElementCountZBinding"), ElementCountZBinding.GetDataSetBindableVariable().GetName().ToString());
	InVisitor->UpdatePOD(TEXT("Iterations"), Iterations);
	InVisitor->UpdateString(TEXT("NumIterationsBinding"), NumIterationsBinding.GetDataSetBindableVariable().GetName().ToString());
	InVisitor->UpdatePOD(TEXT("IterationSource"), (int32)IterationSource);
	InVisitor->UpdatePOD(TEXT("ExecuteBehavior"), (int32)ExecuteBehavior);
	InVisitor->UpdatePOD(TEXT("bDisablePartialParticleUpdate"), bDisablePartialParticleUpdate ? 1 : 0);
	InVisitor->UpdateString(TEXT("DataInterface"), DataInterface.BoundVariable.GetName().ToString());
	InVisitor->UpdateString(TEXT("SimulationStageName"), SimulationStageName.ToString());
	InVisitor->UpdatePOD(TEXT("bParticleIterationStateEnabled"), bParticleIterationStateEnabled ? 1 : 0);
	InVisitor->UpdateString(TEXT("ParticleIterationStateBinding"), ParticleIterationStateBinding.GetDataSetBindableVariable().GetName().ToString());
	InVisitor->UpdateString(TEXT("ParticleIterationStateRange"), FString::Printf(TEXT("%d,%d"), ParticleIterationStateRange.X, ParticleIterationStateRange.Y));
	InVisitor->UpdatePOD(TEXT("bGpuDispatchForceLinear"), bGpuDispatchForceLinear ? 1 : 0);
	InVisitor->UpdatePOD(TEXT("DirectDispatchType"), (int32)DirectDispatchType);
	InVisitor->UpdatePOD(TEXT("DirectDispatchElementType"), int32(DirectDispatchElementType));
	InVisitor->UpdatePOD(TEXT("bOverrideGpuDispatchNumThreads"), bOverrideGpuDispatchNumThreads ? 1 : 0);
	InVisitor->UpdateString(TEXT("OverrideGpuDispatchNumThreads"), FString::Printf(TEXT("%d,%d,%d"), OverrideGpuDispatchNumThreads.X, OverrideGpuDispatchNumThreads.Y, OverrideGpuDispatchNumThreads.Z));

	return true;
}

#if WITH_EDITORONLY_DATA
bool UNiagaraSimulationStageGeneric::FillCompilationData(TArray<FNiagaraSimulationStageCompilationData>& CompilationSimStageData) const
{
	FNiagaraSimulationStageCompilationData& SimStageData = CompilationSimStageData.AddDefaulted_GetRef();
	SimStageData.StageGuid = Script->GetUsageId();
	SimStageData.StageName = SimulationStageName;
	SimStageData.EnabledBinding = EnabledBinding.GetName();
	SimStageData.ElementCountXBinding = ElementCountXBinding.GetName();
	SimStageData.ElementCountYBinding = ElementCountYBinding.GetName();
	SimStageData.ElementCountZBinding = ElementCountZBinding.GetName();
	SimStageData.NumIterations = Iterations;
	SimStageData.NumIterationsBinding = NumIterationsBinding.GetName();
	SimStageData.IterationSourceType = IterationSource;
	SimStageData.IterationDataInterface = IterationSource == ENiagaraIterationSource::DataInterface ? DataInterface.BoundVariable.GetName() : FName();
	SimStageData.ExecuteBehavior = ExecuteBehavior;
	SimStageData.PartialParticleUpdate = bDisablePartialParticleUpdate == false;
	SimStageData.bParticleIterationStateEnabled = bParticleIterationStateEnabled;
	SimStageData.ParticleIterationStateRange = ParticleIterationStateRange;
	SimStageData.bGpuDispatchForceLinear = bGpuDispatchForceLinear;
	SimStageData.DirectDispatchType = DirectDispatchType;
	SimStageData.DirectDispatchElementType = DirectDispatchElementType;
	SimStageData.bOverrideGpuDispatchNumThreads = bOverrideGpuDispatchNumThreads;
	SimStageData.OverrideGpuDispatchNumThreads = OverrideGpuDispatchNumThreads;

	if (SimStageData.bParticleIterationStateEnabled)
	{
		FString AttributeName = ParticleIterationStateBinding.GetName().ToString();
		if (ensureMsgf(AttributeName.RemoveFromStart(TEXT("Particles.")), TEXT("Attribute '%s' is not in particles namespace"), *AttributeName))
		{
			SimStageData.ParticleIterationStateBinding = FName(AttributeName);
		}
	}
	return true;
}
#endif //WITH_EDITORONLY_DATA

void UNiagaraSimulationStageGeneric::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		using namespace NiagaraSimulationStageLocal;
		EnabledBinding.Setup(GetDefaultEnabledBinding(), GetDefaultEnabledBinding(), ENiagaraRendererSourceDataMode::Emitter);
#if WITH_EDITORONLY_DATA
		ElementCountBinding_DEPRECATED.Setup(GetDefaultElementCountBinding(), GetDefaultElementCountBinding(), ENiagaraRendererSourceDataMode::Emitter);
#endif
		ElementCountXBinding.Setup(GetDefaultElementCountBinding(), GetDefaultElementCountBinding(), ENiagaraRendererSourceDataMode::Emitter);
		ElementCountYBinding.Setup(GetDefaultElementCountBinding(), GetDefaultElementCountBinding(), ENiagaraRendererSourceDataMode::Emitter);
		ElementCountZBinding.Setup(GetDefaultElementCountBinding(), GetDefaultElementCountBinding(), ENiagaraRendererSourceDataMode::Emitter);
		NumIterationsBinding.Setup(GetDefaultNumIterationsBinding(), GetDefaultNumIterationsBinding(), ENiagaraRendererSourceDataMode::Emitter);
		ParticleIterationStateBinding.Setup(GetDefaultParticleStateBinding(), GetDefaultParticleStateBinding(), ENiagaraRendererSourceDataMode::Particles);
	}
}

void UNiagaraSimulationStageGeneric::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Ensure data wasn't somehow saved incorrectly
	using namespace NiagaraSimulationStageLocal;
	if (EnabledBinding.GetType() != GetDefaultEnabledBinding().GetType())
	{
		EnabledBinding.Setup(GetDefaultEnabledBinding(), GetDefaultEnabledBinding(), ENiagaraRendererSourceDataMode::Emitter);
	}
	if (ElementCountXBinding.GetType() != GetDefaultElementCountBinding().GetType())
	{
		ElementCountXBinding.Setup(GetDefaultElementCountBinding(), GetDefaultElementCountBinding(), ENiagaraRendererSourceDataMode::Emitter);
	}
	if (ElementCountYBinding.GetType() != GetDefaultElementCountBinding().GetType())
	{
		ElementCountYBinding.Setup(GetDefaultElementCountBinding(), GetDefaultElementCountBinding(), ENiagaraRendererSourceDataMode::Emitter);
	}
	if (ElementCountZBinding.GetType() != GetDefaultElementCountBinding().GetType())
	{
		ElementCountZBinding.Setup(GetDefaultElementCountBinding(), GetDefaultElementCountBinding(), ENiagaraRendererSourceDataMode::Emitter);
	}
	if (NumIterationsBinding.GetType() != GetDefaultNumIterationsBinding().GetType())
	{
		NumIterationsBinding.Setup(GetDefaultNumIterationsBinding(), GetDefaultNumIterationsBinding(), ENiagaraRendererSourceDataMode::Emitter);
	}
	if (ParticleIterationStateBinding.GetType() != GetDefaultParticleStateBinding().GetType())
	{
		ParticleIterationStateBinding.Setup(GetDefaultParticleStateBinding(), GetDefaultParticleStateBinding(), ENiagaraRendererSourceDataMode::Particles);
	}

	// Handle Deprecated Properties
	if (ElementCountBinding_DEPRECATED.IsValid())
	{
		bOverrideGpuDispatchType_DEPRECATED = true;
		OverrideGpuDispatchType_DEPRECATED = ENiagaraGpuDispatchType::OneD;
		ElementCountXBinding = ElementCountBinding_DEPRECATED;
	}

	if (bOverrideGpuDispatchType_DEPRECATED)
	{
		IterationSource = ENiagaraIterationSource::DirectSet;
		DirectDispatchType = OverrideGpuDispatchType_DEPRECATED;
	}
#endif
}

#if WITH_EDITOR
void UNiagaraSimulationStageGeneric::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName;
	FName MemberPropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}
	if (PropertyChangedEvent.MemberProperty)
	{
		MemberPropertyName = PropertyChangedEvent.MemberProperty->GetFName();
	}

	static const TSet<FName> ForceCompilePropertyNames =
	{
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, EnabledBinding),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, ElementCountXBinding),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, ElementCountYBinding),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, ElementCountZBinding),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, Iterations),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, NumIterationsBinding),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, IterationSource),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, ExecuteBehavior),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, bDisablePartialParticleUpdate),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, DataInterface),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, SimulationStageName),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, bParticleIterationStateEnabled),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, ParticleIterationStateBinding),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, bGpuDispatchForceLinear),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, DirectDispatchType),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, DirectDispatchElementType),
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, bOverrideGpuDispatchNumThreads),
	};

	static const TSet<FName> ForceCompileMemberPropertyNamess =
	{
		GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, ParticleIterationStateRange),
	};

	bool bNeedsRecompile = ForceCompilePropertyNames.Contains(PropertyName) || ForceCompileMemberPropertyNamess.Contains(MemberPropertyName);

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraSimulationStageGeneric, OverrideGpuDispatchNumThreads))
	{
		OverrideGpuDispatchNumThreads.X = FMath::Max(OverrideGpuDispatchNumThreads.X, 1);
		OverrideGpuDispatchNumThreads.Y = FMath::Max(OverrideGpuDispatchNumThreads.Y, 1);
		OverrideGpuDispatchNumThreads.Z = FMath::Max(OverrideGpuDispatchNumThreads.Z, 1);
		bNeedsRecompile = true;
	}

	if (bNeedsRecompile)
	{
		RequestRecompile();
	}

	OnChanged().Broadcast();
}

FVersionedNiagaraEmitterData* UNiagaraSimulationStageBase::GetEmitterData() const
{
	if (UNiagaraEmitter* SrcEmitter = GetTypedOuter<UNiagaraEmitter>())
	{
		return SrcEmitter->GetEmitterData(OuterEmitterVersion);
	}
	return nullptr;
}

FVersionedNiagaraEmitter UNiagaraSimulationStageBase::GetOuterEmitter() const
{
	if (UNiagaraEmitter* SrcEmitter = GetTypedOuter<UNiagaraEmitter>())
	{
		return FVersionedNiagaraEmitter(SrcEmitter, OuterEmitterVersion);
	}
	return FVersionedNiagaraEmitter();
}

FName UNiagaraSimulationStageGeneric::GetStackContextReplacementName() const 
{
	switch (IterationSource)
	{
		case ENiagaraIterationSource::Particles:
		case ENiagaraIterationSource::DirectSet:
			return NAME_None;

		case ENiagaraIterationSource::DataInterface:
			return DataInterface.BoundVariable.GetName();

		default:
			ensureMsgf(false, TEXT("Should not get here! Need to handle unknown case!"));
			break;
	}
	return NAME_None;
}
#endif

