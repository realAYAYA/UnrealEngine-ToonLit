// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSettings.h"

UNiagaraSettings::UNiagaraSettings(const FObjectInitializer& ObjectInitlaizer)
	: Super(ObjectInitlaizer)
	, NDISkelMesh_GpuMaxInfluences(ENDISkelMesh_GpuMaxInfluences::Unlimited)
	, NDISkelMesh_GpuUniformSamplingFormat(ENDISkelMesh_GpuUniformSamplingFormat::Full)
	, NDISkelMesh_AdjacencyTriangleIndexFormat(ENDISkelMesh_AdjacencyTriangleIndexFormat::Full)
{
	PositionPinTypeColor = FLinearColor(1.0f, 0.3f, 1.0f, 1.0f);

	NDICollisionQuery_AsyncGpuTraceProviderOrder.Add(ENDICollisionQuery_AsyncGpuTraceProvider::Type::HWRT);
	NDICollisionQuery_AsyncGpuTraceProviderOrder.Add(ENDICollisionQuery_AsyncGpuTraceProvider::Type::GSDF);
}

FName UNiagaraSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR

void UNiagaraSettings::AddEnumParameterType(UEnum* Enum)
{
	if(!AdditionalParameterEnums.Contains(Enum))
	{
		AdditionalParameterEnums.Add(Enum);
		FNiagaraTypeDefinition::RecreateUserDefinedTypeRegistry();
	}
}

FText UNiagaraSettings::GetSectionText() const
{
	return NSLOCTEXT("NiagaraPlugin", "NiagaraSettingsSection", "Niagara");
}
#endif

#if WITH_EDITOR
void UNiagaraSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		SettingsChangedDelegate.Broadcast(PropertyChangedEvent.Property->GetFName(), this);
	}
}

UNiagaraSettings::FOnNiagaraSettingsChanged& UNiagaraSettings::OnSettingsChanged()
{
	return SettingsChangedDelegate;
}

UNiagaraSettings::FOnNiagaraSettingsChanged UNiagaraSettings::SettingsChangedDelegate;
#endif

UNiagaraEffectType* UNiagaraSettings::GetDefaultEffectType()const
{
	return Cast<UNiagaraEffectType>(DefaultEffectType.TryLoad());
}

