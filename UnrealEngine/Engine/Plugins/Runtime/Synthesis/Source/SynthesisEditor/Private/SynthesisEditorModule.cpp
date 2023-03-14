// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynthesisEditorModule.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "SynthComponents/EpicSynth1Component.h"
#include "Factories/Factory.h"
#include "AssetTypeActions_Base.h"
#include "AudioEditorModule.h"
#include "EpicSynth1PresetBank.h"
#include "MonoWaveTablePresetBank.h"
#include "AudioImpulseResponseAsset.h"
#include "ToolMenus.h"
#include "Misc/AssertionMacros.h"
#include "SourceEffects/SourceEffectBitCrusher.h"
#include "SourceEffects/SourceEffectChorus.h"
#include "SourceEffects/SourceEffectDynamicsProcessor.h"
#include "SourceEffects/SourceEffectEnvelopeFollower.h"
#include "SourceEffects/SourceEffectEQ.h"
#include "SourceEffects/SourceEffectFilter.h"
#include "SourceEffects/SourceEffectFoldbackDistortion.h"
#include "SourceEffects/SourceEffectMidSideSpreader.h"
#include "SourceEffects/SourceEffectPanner.h"
#include "SourceEffects/SourceEffectPhaser.h"
#include "SourceEffects/SourceEffectRingModulation.h"
#include "SourceEffects/SourceEffectSimpleDelay.h"
#include "SourceEffects/SourceEffectStereoDelay.h"
#include "SourceEffects/SourceEffectWaveShaper.h"
#include "SubmixEffects/SubmixEffectConvolutionReverb.h"
#include "SubmixEffects/SubmixEffectDelay.h"
#include "SubmixEffects/SubmixEffectFilter.h"
#include "SubmixEffects/SubmixEffectFlexiverb.h"
#include "SubmixEffects/SubmixEffectStereoDelay.h"
#include "SubmixEffects/SubmixEffectTapDelay.h"
#include "SynthesisEditorSettings.h"
#include "HAL/LowLevelMemTracker.h"


DEFINE_LOG_CATEGORY(LogSynthesisEditor);

IMPLEMENT_MODULE(FSynthesisEditorModule, SynthesisEditor)


void FSynthesisEditorModule::StartupModule()
{
	LLM_SCOPE(ELLMTag::AudioSynthesis);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_ModularSynthPresetBank>());
	AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_MonoWaveTableSynthPreset>());
	AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_AudioImpulseResponse>());

	// Now that we've loaded this module, we need to register our effect preset actions
	IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
	AudioEditorModule->RegisterEffectPresetAssetActions();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSynthesisEditorModule::RegisterMenus));
}

void FSynthesisEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	UToolMenus::UnregisterOwner(UE_MODULE_NAME);
}

void FSynthesisEditorModule::RegisterMenus()
{
	LLM_SCOPE(ELLMTag::AudioSynthesis);
	FToolMenuOwnerScoped MenuOwner(this);
	FAudioImpulseResponseExtension::RegisterMenus();
}
