// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioParameterComponent.h"
#include "Components/AudioComponent.h"
#include "UObject/UObjectHash.h"


void UAudioParameterComponent::GetActorSoundParams_Implementation(TArray<FAudioParameter>& Params) const
{
	Params.Append(GetParameters());
}

void UAudioParameterComponent::ResetParameters()
{
	int32 CurrSize = Parameters.Num();
	Parameters.Reset(CurrSize);
}

void UAudioParameterComponent::SetTriggerParameter(FName InName)
{
	// todo@sweedin: use "GetAllAudioComponent" and execute this trigger on them
	UE_LOG(LogAudio, Warning, TEXT("Trigger parameter not supported for UAudioParameterComponent"));
}

void UAudioParameterComponent::SetBoolParameter(FName InName, bool InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetBoolArrayParameter(FName InName, const TArray<bool>& InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetIntParameter(FName InName, int32 InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetIntArrayParameter(FName InName, const TArray<int32>& InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetFloatParameter(FName InName, float InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetFloatArrayParameter(FName InName, const TArray<float>& InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetStringParameter(FName InName, const FString& InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetStringArrayParameter(FName InName, const TArray<FString>& InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetObjectParameter(FName InName, UObject* InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetObjectArrayParameter(FName InName, const TArray<UObject*>& InValue)
{
	SetParameterInternal(FAudioParameter(InName, InValue));
}

void UAudioParameterComponent::SetParameters_Blueprint(const TArray<FAudioParameter>& InParameters)
{
	TArray<FAudioParameter> Values = InParameters;
	SetParameters(MoveTemp(Values));
}

void UAudioParameterComponent::SetParameter(FAudioParameter&& InValue)
{
	SetParameterInternal(MoveTemp(InValue));
}

void UAudioParameterComponent::SetParameters(TArray<FAudioParameter>&& InValues)
{
	for (const FAudioParameter& Value : InValues)
	{
		if (FAudioParameter* CurrentParam = FAudioParameter::FindOrAddParam(Parameters, Value.ParamName))
		{
			constexpr bool bInTakeName = false;
			CurrentParam->Merge(Value, bInTakeName);
		}
	}

	// Forward to any AudioComponents currently playing on this actor (if any)
	TArray<TObjectPtr<UAudioComponent>> Components;
	GetAllAudioComponents(Components);

	for (auto& Component : Components)
	{
		if (Component && Component->IsPlaying())
		{
			TArray<FAudioParameter> ValuesTemp = InValues;
			Component->SetParameters(MoveTemp(ValuesTemp));
		}
	}
}

void UAudioParameterComponent::SetParameterInternal(FAudioParameter&& InParam)
{
	if (InParam.ParamName.IsNone())
	{
		return;
	}

	if (FAudioParameter* CurrentParam = FAudioParameter::FindOrAddParam(Parameters, InParam.ParamName))
	{
		constexpr bool bInTakeName = false;
		CurrentParam->Merge(InParam, bInTakeName);
	}

	// Forward to any AudioComponents currently playing on this actor (if any)
	TArray<UAudioComponent*> Components;
	GetAllAudioComponents(Components);

	for (auto& Component : Components)
	{
		if (Component && Component->IsPlaying())
		{
			Component->SetParameter(MoveTemp(InParam));
		}
	}
}

void UAudioParameterComponent::GetAllAudioComponents(TArray<UAudioComponent*>& Components) const
{
	AActor* RootActor = GetOwner();
	if (USceneComponent* RootComp = RootActor->GetRootComponent())
	{
		// Walk up to the top-most attach actor in the hierarchy (will just be the RootActor if no attachment)
		RootActor = RootComp->GetAttachmentRootActor();
	}

	// Helper to collect components from an actor
	auto CollectFromActor = [&Components](const AActor* InActor)
	{
		if (InActor)
		{
			TArray<UAudioComponent*> TempComponents;
			constexpr bool bIncludeChildActors = false;
			InActor->GetComponents<UAudioComponent>(TempComponents, bIncludeChildActors);
			Components.Append(MoveTemp(TempComponents));
		}
	};

	if (RootActor)
	{
		// Grab any AudioComponents from our owner directly
		CollectFromActor(RootActor);

		// Recursively grab AudioComponents from our Attached Actor's
		TArray<AActor*> AttachedActors;
		constexpr bool bResetArray = false;
		constexpr bool bRecursivelyIncludeAttachedActors = true;
		RootActor->GetAttachedActors(AttachedActors, bResetArray, bRecursivelyIncludeAttachedActors);

		for (AActor* Actor : AttachedActors)
		{
			CollectFromActor(Actor);
		}
	}
}
