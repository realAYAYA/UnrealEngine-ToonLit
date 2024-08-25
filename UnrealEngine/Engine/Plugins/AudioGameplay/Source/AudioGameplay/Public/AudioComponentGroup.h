// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioComponentGroupExtension.h"
#include "AudioParameter.h"
#include "AudioParameterControllerInterface.h"
#include "Components/SceneComponent.h"
#include "AudioComponentGroup.generated.h"

template <typename InterfaceType> class TScriptInterface;

class USoundBase;
class UParamCollection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSoundGroupChanged);
DECLARE_DYNAMIC_DELEGATE_OneParam(FSoundCallback, const FName&, EventName);
DECLARE_DYNAMIC_DELEGATE_OneParam(FBoolParamCallback, const bool, ParamValue);
DECLARE_DYNAMIC_DELEGATE_OneParam(FStringParamCallback, const FString&, Value);

/*
 * Automatic Handler for voices and parameters across any number of AudioComponents
 */
UCLASS(BlueprintType, DefaultToInstanced)
class AUDIOGAMEPLAY_API UAudioComponentGroup : public USceneComponent, public IAudioParameterControllerInterface
{
	GENERATED_BODY()

public:

	UAudioComponentGroup(const FObjectInitializer& ObjectInitializer);

	virtual ~UAudioComponentGroup() = default;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	static UAudioComponentGroup* StaticGetOrCreateComponentGroup(AActor* Actor);

	virtual void BeginPlay() override;

	// Stop all instances of this Sound on any internal or external components
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void StopSound(USoundBase* Sound, const float FadeTime = 0.f);

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = AudioComponentGroup)
	bool IsPlayingAny() const;

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = AudioComponentGroup)
	bool IsVirtualized() const { return bIsVirtualized; }

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void BroadcastStopAll();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void BroadcastKill();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void BroadcastEvent(const FName EventName);

	// Component interaction
	UAudioComponent* GetNextAvailableComponent();

	UAudioComponent* AddComponent();
	UAudioComponent* ResetComponent(UAudioComponent* Component) const;
	void RemoveComponent(const UAudioComponent* InComponent);

	//Allow an externally created AudioComponent to share parameters with this SoundGroup
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void AddExternalComponent(UAudioComponent* ComponentToAdd);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void RemoveExternalComponent(UAudioComponent* ComponentToRemove);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void EnableVirtualization();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void DisableVirtualization();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void SetVolumeMultiplier(const float InVolume) { GroupModifier.Volume = InVolume; }
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void SetPitchMultiplier(const float InPitch) { GroupModifier.Pitch = InPitch; }

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void SetLowPassFilter(const float InFrequency) { GroupModifier.LowPassFrequency = InFrequency; }

	const TArray<FAudioParameter>& GetParams() { return PersistentParams; }

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void AddExtension(TScriptInterface<IAudioComponentGroupExtension> NewExtension);
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void RemoveExtension(TScriptInterface<IAudioComponentGroupExtension> NewExtension);

	void UpdateExtensions(const float DeltaTime);

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = AudioComponentGroup)
	float GetFloatParamValue(const FName ParamName) const;

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = AudioComponentGroup)
	bool GetBoolParamValue(const FName ParamName) const;
	 
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = AudioComponentGroup)
	FString GetStringParamValue(const FName ParamName) const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void SubscribeToStringParam(const FName ParamName, FStringParamCallback Delegate);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void SubscribeToEvent(const FName EventName, FSoundCallback Delegate);
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void SubscribeToBool(const FName ParamName, FBoolParamCallback Delegate);

	// remove any string, event, and bool subscriptions that are bound to this object
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = AudioComponentGroup)
	void UnsubscribeObject(const UObject* Object);
	
	//~ Begin IAudioParameterControllerInterface interface 
	virtual void ResetParameters() override;

	virtual void SetTriggerParameter(FName InName) override;
	virtual void SetBoolParameter(FName InName, bool InBool) override;
	virtual void SetBoolArrayParameter(FName InName, const TArray<bool>& InValue) override;
	virtual void SetIntParameter(FName InName, int32 InInt) override;
	virtual void SetIntArrayParameter(FName InName, const TArray<int32>& InValue) override;
	virtual void SetFloatParameter(FName InName, float InFloat) override;
	virtual void SetFloatArrayParameter(FName InName, const TArray<float>& InValue) override;
	virtual void SetStringParameter(FName InName, const FString& InValue) override;
	virtual void SetStringArrayParameter(FName InName, const TArray<FString>& InValue) override;
	virtual void SetObjectParameter(FName InName, UObject* InValue) override;
	virtual void SetObjectArrayParameter(FName InName, const TArray<UObject*>& InValue) override;

	virtual void SetParameter(FAudioParameter&& InValue) override;
	virtual void SetParameters(TArray<FAudioParameter>&& InValues) override;
	virtual void SetParameters_Blueprint(const TArray<FAudioParameter>& InParameters) override;
	//~ End IAudioParameterControllerInterface interface

	UPROPERTY(BlueprintAssignable)
	FSoundGroupChanged OnStopped;

	UPROPERTY(BlueprintAssignable)
	FSoundGroupChanged OnKilled;

	UPROPERTY(BlueprintAssignable)
	FSoundGroupChanged OnVirtualized;

	UPROPERTY(BlueprintAssignable)
	FSoundGroupChanged OnUnvirtualized;

	void IterateComponents(const TFunction<void(UAudioComponent*)> OnIterate);

protected:

	void ApplyParams(UAudioComponent* Component) const;
	void ApplyModifiers(UAudioComponent* Component, const FAudioComponentModifier& Modifier) const;

	void UpdateComponentParameters();

	float GetComponentVolume() const;

	void ExecuteStringParamSubscriptions(const FAudioParameter& StringParam);
	void ExecuteBoolParamSubscriptions(const FAudioParameter& BoolParam);
	void ExecuteEventSubscriptions(const FName EventName);

	const FAudioParameter* GetParamInternal(const FName ParamName) const;

	UPROPERTY(VisibleAnywhere, Category = AudioComponentGroup)
	TArray<TObjectPtr<UAudioComponent>> Components;

	UPROPERTY(Transient)
	TArray<FAudioParameter> ParamsToSet;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = AudioComponentGroup)
	TArray<FAudioParameter> PersistentParams;

	UPROPERTY(Transient)
	TArray<TScriptInterface<IAudioComponentGroupExtension>> Extensions;

	// Modifier set externally via BP functions
	FAudioComponentModifier GroupModifier;

	// final values set last update
	FAudioComponentModifier CachedModifier;

	//Components managed externally that won't be used in the pool, but can still share parameters
	TArray<TWeakObjectPtr<UAudioComponent>> ExternalComponents;

	TMap<FName, TArray<FStringParamCallback>> StringSubscriptions;
	TMap<FName, TArray<FSoundCallback>> EventSubscriptions;
	TMap<FName, TArray<FBoolParamCallback>> BoolSubscriptions;

	bool bIsVirtualized = false;

	friend class FAudioComponentGroupDebug;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Components/AudioComponent.h"
#endif
