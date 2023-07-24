// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundGeneratorHandle.h"

#include "Components/AudioComponent.h"
#include "AudioDeviceManager.h"
#include "MetasoundGenerator.h"
#include "MetasoundSource.h"
#include "MetasoundParameterPack.h"

UMetasoundGeneratorHandle* UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle(UAudioComponent* OnComponent)
{
	if (!OnComponent)
	{
		return nullptr;
	}

	UMetasoundGeneratorHandle* Result = NewObject<UMetasoundGeneratorHandle>();
	Result->SetAudioComponent(OnComponent);
	return Result;
}

void UMetasoundGeneratorHandle::ClearCachedData()
{
	DetachGeneratorDelegates();
	AudioComponent        = nullptr;
	AudioComponentId      = 0;
	CachedMetasoundSource = nullptr;
	CachedGeneratorPtr    = nullptr;
	CachedParameterPack   = nullptr;
}

void UMetasoundGeneratorHandle::SetAudioComponent(UAudioComponent* InAudioComponent)
{
	if (InAudioComponent != AudioComponent)
	{
		ClearCachedData();
		AudioComponent   = InAudioComponent;
		AudioComponentId = InAudioComponent->GetAudioComponentID();
	}
}

void UMetasoundGeneratorHandle::CacheMetasoundSource()
{
	if (!AudioComponent)
	{
		return;
	}

	UMetaSoundSource* CurrentMetasoundSource = Cast<UMetaSoundSource>(AudioComponent->GetSound());
	if (CachedMetasoundSource == CurrentMetasoundSource)
	{
		return;
	}

	DetachGeneratorDelegates();
	CachedGeneratorPtr    = nullptr;
	CachedMetasoundSource = CurrentMetasoundSource;

	if (CachedMetasoundSource)
	{
		AttachGeneratorDelegates();
	}
}

void UMetasoundGeneratorHandle::AttachGeneratorDelegates()
{
	GeneratorCreatedDelegateHandle = CachedMetasoundSource->OnGeneratorInstanceCreated.AddLambda(
		[=](uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
		{
			OnSourceCreatedAGenerator(InAudioComponentId, InGenerator);
		});
	GeneratorDestroyedDelegateHandle = CachedMetasoundSource->OnGeneratorInstanceDestroyed.AddLambda(
		[=](uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
		{
			OnSourceDestroyedAGenerator(InAudioComponentId, InGenerator);
		});
}

void UMetasoundGeneratorHandle::DetachGeneratorDelegates()
{
	CachedMetasoundSource->OnGeneratorInstanceCreated.Remove(GeneratorCreatedDelegateHandle);
	GeneratorCreatedDelegateHandle.Reset();
	CachedMetasoundSource->OnGeneratorInstanceDestroyed.Remove(GeneratorDestroyedDelegateHandle);
	GeneratorDestroyedDelegateHandle.Reset();
}

TSharedPtr<Metasound::FMetasoundGenerator> UMetasoundGeneratorHandle::PinGenerator()
{
	TSharedPtr<Metasound::FMetasoundGenerator> PinnedGenerator = CachedGeneratorPtr.Pin();
	if (PinnedGenerator.IsValid() || !CachedMetasoundSource)
	{
		return PinnedGenerator;
	}

	// The first attempt to pin failed, so reach out to the MetaSoundSource and see if it has a 
	// generator for our AudioComponent...
	CachedGeneratorPtr = CachedMetasoundSource->GetGeneratorForAudioComponent(AudioComponentId);
	PinnedGenerator    = CachedGeneratorPtr.Pin();
	return PinnedGenerator;
}

bool UMetasoundGeneratorHandle::ApplyParameterPack(UMetasoundParameterPack* Pack)
{
	if (!Pack)
	{
		return false;
	}

	// Create a copy of the parameter pack and cache it.
	CachedParameterPack = Pack->GetCopyOfParameterStorage();

	// No point in continuing if the parameter pack is not valid for any reason.
	if (!CachedParameterPack.IsValid())
	{
		return false;
	}

	// Assure that our MetaSoundSource is up to date. It is possible that this has been 
	// changed via script since we were first created.
	CacheMetasoundSource();

	// Now we can try to pin the generator.
	TSharedPtr<Metasound::FMetasoundGenerator> PinnedGenerator = PinGenerator();

	if (!PinnedGenerator.IsValid())
	{
		// Failed to pin the generator, but we have cached the parameter pack,
		// so if our delegate gets called when a new generator is created we can 
		// apply the cached parameters then.
		return false;
	}

	// Finally... send down the parameter pack.
	PinnedGenerator->QueueParameterPack(CachedParameterPack);
	return true;
}

void UMetasoundGeneratorHandle::OnSourceCreatedAGenerator(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
{
	if (InAudioComponentId == AudioComponentId)
	{
		CachedGeneratorPtr = InGenerator;
		if (InGenerator && CachedParameterPack)
		{
			InGenerator->QueueParameterPack(CachedParameterPack);
		}
	}
}

void UMetasoundGeneratorHandle::OnSourceDestroyedAGenerator(uint64 InAudioComponentId, TSharedPtr<Metasound::FMetasoundGenerator> InGenerator)
{
	if (InAudioComponentId == AudioComponentId)
	{
		CachedGeneratorPtr = nullptr;
	}
}

