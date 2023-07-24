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
#include "PropertyEditorModule.h"
#include "Settings/ProjectPackagingSettings.h"
#endif // WITH_EDITOR

class FUsdStageModule : public IUsdStageModule
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		LLM_SCOPE_BYTAG(Usd);

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( TEXT( "PropertyEditor" ) );
		PropertyModule.RegisterCustomClassLayout( TEXT( "UsdStageActor" ), FOnGetDetailCustomizationInstance::CreateStatic( &FUsdStageActorCustomization::MakeInstance ) );
#endif // WITH_EDITOR
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
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
