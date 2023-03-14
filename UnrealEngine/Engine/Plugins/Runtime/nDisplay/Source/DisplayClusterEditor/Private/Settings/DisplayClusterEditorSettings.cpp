// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterEditorSettings.h"
#include "DisplayClusterEditorEngine.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"


const FName UDisplayClusterEditorSettings::Container = TEXT("Project");
const FName UDisplayClusterEditorSettings::Category  = TEXT("Plugins");
const FName UDisplayClusterEditorSettings::Section   = TEXT("nDisplay");


UDisplayClusterEditorSettings::UDisplayClusterEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) 
{
	GET_MEMBER_NAME_CHECKED(UDisplayClusterEditorSettings, bEnabled);
}

void UDisplayClusterEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property != nullptr)
	{
		// Since nDisplay is Windows only, save configs that depends on nDisplay plugin assets in Windows specific config file
		static const FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());
		static const FString DefaultGamePath   = FString::Printf(TEXT("%sDefaultGame.ini"),   *FPaths::SourceConfigDir());

		FName PropertyName(PropertyChangedEvent.Property->GetFName());

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UDisplayClusterEditorSettings, bEnabled))
		{
			if (FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*DefaultEnginePath))
			{
				FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*DefaultEnginePath, false);
			}

			if (FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*DefaultGamePath))
			{
				FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*DefaultGamePath, false);
			}

			if (bEnabled)
			{
				// DefaultEngine.ini
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), TEXT("/Script/DisplayCluster.DisplayClusterGameEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), TEXT("/Script/DisplayClusterEditor.DisplayClusterEditorEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameViewportClientClassName"), TEXT("/Script/DisplayCluster.DisplayClusterViewportClient"), DefaultEnginePath);

				// DefaultGame.ini
				GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("bUseBorderlessWindow"), TEXT("True"), DefaultGamePath);
			}
			else
			{
				// DefaultEngine.ini
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), TEXT("/Script/Engine.GameEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), TEXT("/Script/UnrealEd.UnrealEdEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameViewportClientClassName"), TEXT("/Script/Engine.GameViewportClient"), DefaultEnginePath);

				// DefaultGame.ini
				GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("bUseBorderlessWindow"), TEXT("False"), DefaultGamePath);
			}

			GConfig->Flush(false, DefaultEnginePath);
			GConfig->Flush(false, DefaultGamePath);
		}
	}
}
