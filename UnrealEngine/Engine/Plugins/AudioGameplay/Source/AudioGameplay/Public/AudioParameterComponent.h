// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "Containers/Array.h"
#include "UObject/Object.h"
#include "AudioParameter.h"
#include "AudioParameterControllerInterface.h"
#include "Audio/ActorSoundParameterInterface.h"

#include "AudioParameterComponent.generated.h"


class UAudioComponent;

/**
 *	UAudioParameterComponent - Can be used to set/store audio parameters and automatically dispatch them (through ActorSoundParameterInterface) 
 *  to any sounds played by the component's Owner Actor
 */
UCLASS(BlueprintType, HideCategories=(Object, ActorComponent, Physics, Rendering, Mobility, LOD), meta=(BlueprintSpawnableComponent))
class AUDIOGAMEPLAY_API UAudioParameterComponent : public UActorComponent, 
		  										   public IAudioParameterControllerInterface, 
												   public IActorSoundParameterInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Parameters)
	const TArray<FAudioParameter>& GetParameters() const { return Parameters; }

	/** Begin IActorSoundParameterInterface */
	virtual void GetActorSoundParams_Implementation(TArray<FAudioParameter>& Params) const override;
	/** End IActorSoundParameterInterface */

	/** Begin IAudioParameterControllerInterface */
	virtual void ResetParameters() override;
	virtual void SetTriggerParameter(FName InName) override;
	virtual void SetBoolParameter(FName InName, bool InValue) override;
	virtual void SetBoolArrayParameter(FName InName, const TArray<bool>& InValue) override;
	virtual void SetIntParameter(FName InName, int32 InInt) override;
	virtual void SetIntArrayParameter(FName InName, const TArray<int32>& InValue) override;
	virtual void SetFloatParameter(FName InName, float InValue) override;
	virtual void SetFloatArrayParameter(FName InName, const TArray<float>& InValue) override;
	virtual void SetStringParameter(FName InName, const FString& InValue) override;
	virtual void SetStringArrayParameter(FName InName, const TArray<FString>& InValue) override;
	virtual void SetObjectParameter(FName InName, UObject* InValue) override;
	virtual void SetObjectArrayParameter(FName InName, const TArray<UObject*>& InValue) override;
	virtual void SetParameters_Blueprint(const TArray<FAudioParameter>& InParameters) override;
	virtual void SetParameter(FAudioParameter&& InValue) override;
	virtual void SetParameters(TArray<FAudioParameter>&& InValues) override;
	/** End IAudioParameterControllerInterface */

private:

	void SetParameterInternal(FAudioParameter&& InParam);
	void GetAllAudioComponents(TArray<UAudioComponent*>& Components) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UAudioComponent>> ActiveComponents;

	UPROPERTY(EditDefaultsOnly, Category = "Parameters")
	TArray<FAudioParameter> Parameters;
};
