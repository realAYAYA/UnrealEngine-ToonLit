// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundModulationPatch.h"

#include "AudioDefines.h"
#include "AudioDevice.h"
#include "AudioModulation.h"
#include "AudioModulationStatics.h"
#include "AudioModulationSystem.h"
#include "SoundControlBus.h"
#include "SoundModulationParameter.h"
#include "SoundModulationPatchProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundModulationPatch)


#define LOCTEXT_NAMESPACE "SoundModulationPatch"


USoundModulationPatch::USoundModulationPatch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TUniquePtr<Audio::IProxyData> USoundModulationPatch::CreateNewProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	using namespace AudioModulation;
	return MakeUnique<FSoundModulatorAssetProxy>(*this);
}

TUniquePtr<Audio::IModulatorSettings> USoundModulationPatch::CreateProxySettings() const
{
	using namespace AudioModulation;
	return TUniquePtr<Audio::IModulatorSettings>(new FModulationPatchSettings(*this));
}

const Audio::FModulationParameter& USoundModulationPatch::GetOutputParameter() const
{
	const FString Breadcrumb = FString::Format(TEXT("{0} '{1}'"), { *GetClass()->GetName(), *GetName() });
	return AudioModulation::GetOrRegisterParameter(PatchSettings.OutputParameter, Breadcrumb);
}

#if WITH_EDITOR
void USoundModulationPatch::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	AudioModulation::IterateModulationManagers([this](AudioModulation::FAudioModulationManager& OutModulation)
	{
		OutModulation.UpdateModulator(*this);
	});

	Super::PostEditChangeProperty(InPropertyChangedEvent);
}

void USoundModulationPatch::PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	AudioModulation::IterateModulationManagers([this](AudioModulation::FAudioModulationManager& OutModulation)
	{
		OutModulation.UpdateModulator(*this);
	});

	Super::PostEditChangeChainProperty(InPropertyChangedEvent);
}

void USoundModulationPatch::PreSave(FObjectPreSaveContext InSaveContext)
{
	Super::PreSave(InSaveContext);
}
#endif // WITH_EDITOR

FSoundControlModulationInput::FSoundControlModulationInput()
	: bSampleAndHold(0)
{
}

const USoundControlBus* FSoundControlModulationInput::GetBus() const
{
	return Bus;
}

const USoundControlBus& FSoundControlModulationInput::GetBusChecked() const
{
	check(Bus);
	return *Bus;
}
#undef LOCTEXT_NAMESPACE // SoundModulationPatch

