// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "MetasoundSource.h"

#include "MetasoundOfflinePlayerComponent.generated.h"

namespace Metasound
{
	class FMetasoundGeneratorHandle;
}
class UAudioComponent;
class UMetasoundGeneratorHandle;

UCLASS()
class HARMONIXMETASOUND_API UMetasoundOfflinePlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UMetasoundOfflinePlayerComponent();

	// To deprecate, only required to keep engine-only integrated backends happy
	UMetasoundGeneratorHandle* CreateGeneratorBasedOnAudioComponent(UAudioComponent* AudioComponent, int32 InSampleRate = 12000, int32 InBlockSize = 256);

	TSharedPtr<Metasound::FMetasoundGeneratorHandle> CreateSharedGeneratorBasedOnAudioComponent(UAudioComponent* AudioComponent, int32 InSampleRate = 12000, int32 InBlockSize = 256);
	void ReleaseGenerator();

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	TSharedPtr<Metasound::FMetasoundGenerator> GetGenerator();

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	ISoundGeneratorPtr Generator;

	UPROPERTY(Transient)
	TObjectPtr<UMetaSoundSource> MetasoundSource;

	int32 SampleRate;
	int32 BlockSize;
	int32 GeneratorBlockSize;

	double StartSecond = 0.0;
	int64  RenderedSamples = 0;
	TArray<float> ScratchBuffer;
};
