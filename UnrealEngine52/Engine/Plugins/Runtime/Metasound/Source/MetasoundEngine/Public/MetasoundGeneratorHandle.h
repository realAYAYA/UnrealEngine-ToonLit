// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/SharedPointer.h"
#include "MetasoundParameterPack.h"

#include "MetasoundGeneratorHandle.generated.h"

class UAudioComponent;
class UMetaSoundSource;
class UMetasoundParameterPack;
namespace Metasound
{
	class FMetasoundGenerator;
}

UCLASS(BlueprintType,Category="MetaSound")
class METASOUNDENGINE_API UMetasoundGeneratorHandle : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="MetaSound")
	static UMetasoundGeneratorHandle* CreateMetaSoundGeneratorHandle(UAudioComponent* OnComponent);

	/**
	 * Makes a copy of the supplied parameter pack and passes it to the MetaSoundGenerator
	 * for asynchronous processing. IT ALSO caches this copy so that if the AudioComponent
	 * is virtualized the parameter pack will be sent again when/if the AudioComponent is 
	 * "unvirtualized".
	 */
	UFUNCTION(BlueprintCallable, Category="MetaSoundParameterPack")
	bool ApplyParameterPack(UMetasoundParameterPack* Pack);

private:

	void SetAudioComponent(UAudioComponent* InAudioComponent);
	void CacheMetasoundSource();
	void ClearCachedData();

	/**
	 * Attempts to pin the weak generator pointer. If the first attempt fails it checks to see
	 * if it can "recapture" a pointer to a generator for the current AudioComponent/MetaSoundSource
	 * combination. 
	 */
	TSharedPtr<Metasound::FMetasoundGenerator> PinGenerator();

	/**
	 * Functions for adding and removing our MetaSoundGenerator lifecycle delegates
	 */
	void AttachGeneratorDelegates();
	void DetachGeneratorDelegates();

	/**
	 * Generator creation and destruction delegates we register with the UMetaSoundSource
	 */
	void OnSourceCreatedAGenerator(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator);
	void OnSourceDestroyedAGenerator(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AudioComponent;
	uint64 AudioComponentId;
	UPROPERTY(Transient)
	TObjectPtr<UMetaSoundSource> CachedMetasoundSource;
	TWeakPtr<Metasound::FMetasoundGenerator> CachedGeneratorPtr;
	FSharedMetasoundParameterStoragePtr CachedParameterPack;

	FDelegateHandle GeneratorCreatedDelegateHandle;
	FDelegateHandle GeneratorDestroyedDelegateHandle;
};