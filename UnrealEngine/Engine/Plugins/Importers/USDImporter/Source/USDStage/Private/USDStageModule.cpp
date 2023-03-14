// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDStageModule.h"

#include "USDMemory.h"
#include "USDStageActor.h"
#include "USDStageActorCustomization.h"

#include "Modules/ModuleManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"

#if WITH_EDITOR
#include "Settings/ProjectPackagingSettings.h"
#endif // WITH_EDITOR

class FUsdStageModule : public IUsdStageModule
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		LLM_SCOPE_BYTAG(Usd);

		// If don't have any active references to our materials they won't be packaged into monolithic builds, and we wouldn't
		// be able to create dynamic material instances at runtime.
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin( TEXT( "USDImporter" ) );
		if ( Plugin.IsValid() )
		{
			FString MaterialsPath = TEXT( "/USDImporter/Materials" );

			UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>( UProjectPackagingSettings::StaticClass()->GetDefaultObject() );
			if ( PackagingSettings )
			{
				bool bAlreadyInPath = false;

				TArray<FDirectoryPath>& DirectoriesToCook = PackagingSettings->DirectoriesToAlwaysCook;
				for ( int32 Index = DirectoriesToCook.Num() - 1; Index >= 0; --Index )
				{
					if ( FPaths::IsSamePath( DirectoriesToCook[ Index ].Path, MaterialsPath ) )
					{
						bAlreadyInPath = true;
						break;
					}
				}

				if ( !bAlreadyInPath )
				{
					FDirectoryPath MaterialsDirectory;
					MaterialsDirectory.Path = MaterialsPath;

					PackagingSettings->DirectoriesToAlwaysCook.Add( MaterialsDirectory );
				}
			}
		}

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( TEXT( "PropertyEditor" ) );
		PropertyModule.RegisterCustomClassLayout( TEXT( "UsdStageActor" ), FOnGetDetailCustomizationInstance::CreateStatic( &FUsdStageActorCustomization::MakeInstance ) );
#endif // WITH_EDITOR
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin( TEXT( "USDImporter" ) );
		if ( Plugin.IsValid() && !IsEngineExitRequested() ) // If we're shutting down because the engine is exiting then this may crash, and there's no point anyway
		{
			FString Path = TEXT( "/USDImporter/Materials" );

			UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>( UProjectPackagingSettings::StaticClass()->GetDefaultObject() );
			if ( PackagingSettings )
			{
				TArray<FDirectoryPath>& DirectoriesToCook = PackagingSettings->DirectoriesToAlwaysCook;
				for ( int32 Index = DirectoriesToCook.Num() - 1; Index >= 0; --Index )
				{
					if ( FPaths::IsSamePath( DirectoriesToCook[ Index ].Path, Path ) )
					{
						DirectoriesToCook.RemoveAt(Index);
					}
				}
			}
		}

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >( TEXT( "PropertyEditor" ) );
		PropertyModule.UnregisterCustomClassLayout( TEXT( "UsdStageActor" ) );
#endif // WITH_EDITOR
	}

	virtual AUsdStageActor& GetUsdStageActor( UWorld* World ) override
	{
		if ( AUsdStageActor* UsdStageActor = FindUsdStageActor( World ) )
		{
			return *UsdStageActor;
		}
		else
		{
			return *( World->SpawnActor< AUsdStageActor >() );
		}
	}

	virtual AUsdStageActor* FindUsdStageActor( UWorld* World ) override
	{
		for ( FActorIterator ActorIterator( World ); ActorIterator; ++ActorIterator )
		{
			if ( AUsdStageActor* UsdStageActor = Cast< AUsdStageActor >( *ActorIterator ) )
			{
				return UsdStageActor;
			}
		}

		return nullptr;
	}
};

IMPLEMENT_MODULE_USD( FUsdStageModule, USDStage );
