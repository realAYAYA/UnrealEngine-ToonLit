// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayBehaviorsEditorModule.h"

#include "GameplayBehaviorsEditorStyle.h"

#define LOCTEXT_NAMESPACE "GameplayBehaviors"

class FGameplayBehaviorsEditorModule : public IGameplayBehaviorsEditorModule
{
protected:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FGameplayBehaviorsEditorModule::StartupModule()
{
	FGameplayBehaviorsEditorStyle::Get();
}

void FGameplayBehaviorsEditorModule::ShutdownModule()
{
	FGameplayBehaviorsEditorStyle::Shutdown();
}

IMPLEMENT_MODULE(FGameplayBehaviorsEditorModule, GameplayBehaviorsEditorModule)

#undef LOCTEXT_NAMESPACE