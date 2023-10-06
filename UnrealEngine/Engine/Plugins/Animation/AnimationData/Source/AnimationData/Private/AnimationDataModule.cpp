// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationDataModule.h"
#include "AnimSequencerDataModel.h"
#include "Features/IModularFeatures.h"
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"

void FAnimationDataModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature( UE::Anim::DataModel::IAnimationDataModels::GetModularFeatureName(),  &SequencerBasedDataModelFeature);
}

void FAnimationDataModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature( UE::Anim::DataModel::IAnimationDataModels::GetModularFeatureName(),  &SequencerBasedDataModelFeature);
}

IMPLEMENT_MODULE(FAnimationDataModule, AnimationData);