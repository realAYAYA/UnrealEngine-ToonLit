// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIODisplayExtensionWrapper.h"
#include "OpenColorIOModule.h"
#include "Engine/GameEngine.h"
#include "Slate/SceneViewport.h"

#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenColorIODisplayExtensionWrapper)

void UOpenColorIODisplayExtensionWrapper::CreateDisplayExtensionIfNotExists()
{
	if (!DisplayExtension.IsValid())
	{
		// Null viewport should ensure it doesn't run anywhere yet (unless explicitly gathered)
		DisplayExtension = FSceneViewExtensions::NewExtension<FOpenColorIODisplayExtension>(nullptr);
	}
	check(DisplayExtension.IsValid());
}

void UOpenColorIODisplayExtensionWrapper::SetOpenColorIOConfiguration(FOpenColorIODisplayConfiguration InDisplayConfiguration)
{
	if (!DisplayExtension.IsValid())
	{
		return;
	}

	DisplayExtension->SetDisplayConfiguration(InDisplayConfiguration);
}

void UOpenColorIODisplayExtensionWrapper::SetSceneExtensionIsActiveFunction(const FSceneViewExtensionIsActiveFunctor& IsActiveFunction)
{
	if (!DisplayExtension.IsValid())
	{
		return;
	}

	DisplayExtension->IsActiveThisFrameFunctions.Reset(1);
	DisplayExtension->IsActiveThisFrameFunctions.Add(IsActiveFunction);
}

void UOpenColorIODisplayExtensionWrapper::SetSceneExtensionIsActiveFunctions(const TArray<FSceneViewExtensionIsActiveFunctor>& IsActiveFunctions)
{
	if (!DisplayExtension.IsValid())
	{
		return;
	}

	DisplayExtension->IsActiveThisFrameFunctions = IsActiveFunctions;
}

void UOpenColorIODisplayExtensionWrapper::RemoveSceneExtension()
{
	DisplayExtension.Reset();
}

UOpenColorIODisplayExtensionWrapper* UOpenColorIODisplayExtensionWrapper::CreateOpenColorIODisplayExtension(
	FOpenColorIODisplayConfiguration InDisplayConfiguration,
	const FSceneViewExtensionIsActiveFunctor& IsActiveFunction)
{
	if (!InDisplayConfiguration.ColorConfiguration.IsValid())
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("%s, no display extension was created."), *InDisplayConfiguration.ColorConfiguration.ToString());
		return nullptr;
	}

	// Create OCIO Scene View Extension and configure it.

	UOpenColorIODisplayExtensionWrapper* OutExtension = NewObject<UOpenColorIODisplayExtensionWrapper>();

	OutExtension->CreateDisplayExtensionIfNotExists();
	OutExtension->SetOpenColorIOConfiguration(InDisplayConfiguration);
	OutExtension->SetSceneExtensionIsActiveFunction(IsActiveFunction);

	return OutExtension;
}

UOpenColorIODisplayExtensionWrapper* UOpenColorIODisplayExtensionWrapper::CreateInGameOpenColorIODisplayExtension(FOpenColorIODisplayConfiguration InDisplayConfiguration)
{
	FSceneViewExtensionIsActiveFunctor IsActiveFunctor;
	IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
	{
		if (!Context.Viewport)
		{
			return TOptional<bool>();
		}

#if WITH_EDITOR
		if (GIsEditor && GEditor)
		{
			// Activate the SVE if it is a PIE viewport.
			if (Context.Viewport->IsPlayInEditorViewport())
			{
				return TOptional<bool>(true);
			}
		}
#endif // WITH_EDITOR

		// Activate the SVE if it is the game's primary viewport.
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			if (GameEngine->SceneViewport.IsValid() && (GameEngine->SceneViewport->GetViewport() == Context.Viewport))
			{
				return TOptional<bool>(true);
			}
		}

		// If our viewport did not meet any of the criteria to activate the SVE, emit no opinion.
		return TOptional<bool>();
	};

	return CreateOpenColorIODisplayExtension(InDisplayConfiguration, IsActiveFunctor);
}
