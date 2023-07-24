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
		const FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());
		const FString DefaultGamePath   = FString::Printf(TEXT("%sDefaultGame.ini"),   *FPaths::SourceConfigDir());
		const FString DefaultInputPath  = FString::Printf(TEXT("%sDefaultInput.ini"),  *FPaths::SourceConfigDir());

		FName PropertyName(PropertyChangedEvent.Property->GetFName());

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UDisplayClusterEditorSettings, bEnabled))
		{
			// Reset read-only flag from the config files
			{
				const FString* ConfigFiles[] = {
					&DefaultEnginePath,
					&DefaultGamePath,
					&DefaultInputPath
				};

				for (const FString* ConfigFile : ConfigFiles)
				{
					if (FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(**ConfigFile))
					{
						FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(**ConfigFile, false);
					}
				}
			}

			// Process nDisplay 'enable' command
			if (bEnabled)
			{
				// DefaultEngine.ini
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), TEXT("/Script/DisplayCluster.DisplayClusterGameEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), TEXT("/Script/DisplayClusterEditor.DisplayClusterEditorEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameViewportClientClassName"), TEXT("/Script/DisplayCluster.DisplayClusterViewportClient"), DefaultEnginePath);

				// DefaultGame.ini
				GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("bUseBorderlessWindow"), TEXT("True"), DefaultGamePath);

				// DefaultInput.ini
				GConfig->SetString(TEXT("/Script/Engine.InputSettings"), TEXT("DefaultPlayerInputClass"), TEXT("/Script/DisplayCluster.DisplayClusterPlayerInput"), DefaultInputPath);
			}
			// Process nDisplay 'disable' command
			else
			{
				// DefaultEngine.ini
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), TEXT("/Script/Engine.GameEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), TEXT("/Script/UnrealEd.UnrealEdEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameViewportClientClassName"), TEXT("/Script/Engine.GameViewportClient"), DefaultEnginePath);

				// DefaultGame.ini
				GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("bUseBorderlessWindow"), TEXT("False"), DefaultGamePath);

				// DefaultInput.ini
				GConfig->SetString(TEXT("/Script/Engine.InputSettings"), TEXT("DefaultPlayerInputClass"), TEXT("/Script/EnhancedInput.EnhancedPlayerInput"), DefaultInputPath);
			}

			// Save changes to the files
			GConfig->Flush(false, DefaultEnginePath);
			GConfig->Flush(false, DefaultGamePath);
			GConfig->Flush(false, DefaultInputPath);
		}
	}
}
