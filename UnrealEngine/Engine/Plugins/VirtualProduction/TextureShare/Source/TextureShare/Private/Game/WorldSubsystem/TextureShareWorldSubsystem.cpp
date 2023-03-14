// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/WorldSubsystem/TextureShareWorldSubsystem.h"
#include "Game/WorldSubsystem/TextureShareWorldSubsystemObject.h"
#include "Game/Settings/TextureShareSettings.h"

#include "Module/TextureShareLog.h"

#include "ITextureShare.h"
#include "ITextureShareAPI.h"

#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Misc/ConfigCacheIni.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareWorldSubsystemHelpers
{
	static ITextureShareAPI& TextureShareAPI()
	{
		static ITextureShareAPI& TextureShareAPISingletone = ITextureShare::Get().GetTextureShareAPI();
		return TextureShareAPISingletone;
	}
};
using namespace TextureShareWorldSubsystemHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// UTextureShareWorldSubsystem
//////////////////////////////////////////////////////////////////////////////////////////////
UTextureShareWorldSubsystem::UTextureShareWorldSubsystem()
{
	FTextureShareSettings PluginSettings = FTextureShareSettings::GetSettings();
	if(PluginSettings.bCreateDefaults)
	{
		// Create default
		TextureShare = NewObject<UTextureShare>(GetTransientPackage(), NAME_None, RF_Transient | RF_ArchetypeObject | RF_Public | RF_Transactional);
	}
}

UTextureShareWorldSubsystem::~UTextureShareWorldSubsystem()
{
	TextureShare = nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////
TStatId UTextureShareWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTextureShareWorldSubsystem, STATGROUP_Tickables);
}

void UTextureShareWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTextureShareWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		OnWorldEndPlay(*World);
	}

	Super::Deinitialize();
}

//////////////////////////////////////////////////////////////////////////////////////////////
void UTextureShareWorldSubsystem::Release()
{
	// Remove all created TextureShare objects
	for (const FString& TextureShareNameIt : NamesOfExistingObjects)
	{
		if (!TextureShareNameIt.IsEmpty())
		{
			TextureShareAPI().RemoveObject(TextureShareNameIt);
		}
	}

	NamesOfExistingObjects.Empty();
}

//////////////////////////////////////////////////////////////////////////////////////////////
void UTextureShareWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	bWorldPlay = true;

	UE_LOG(LogTextureShareWorldSubsystem, Verbose, TEXT("OnWorldBeginPlay"));

	FTextureShareSettings PluginSettings = FTextureShareSettings::GetSettings();
	if (!PluginSettings.ProcessName.IsEmpty())
	{
		TextureShareAPI().SetProcessName(PluginSettings.ProcessName);
	}

	TextureShareAPI().OnWorldBeginPlay(InWorld);

	Super::OnWorldBeginPlay(InWorld);
}

void UTextureShareWorldSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	if (bWorldPlay)
	{
		UE_LOG(LogTextureShareWorldSubsystem, Verbose, TEXT("OnWorldEndPlay"));

		TextureShareAPI().OnWorldEndPlay(InWorld);

		bWorldPlay = false;
		Release();
		
	}
}

void UTextureShareWorldSubsystem::Tick(float DeltaTime)
{
	if (bWorldPlay)
	{
		if (TextureShare && TextureShare->IsEnabled())
		{
			// Update the list of used object names for the current frame
			// and remove unused objects
			const TSet<FString> PrevFrameObjectNames(NamesOfExistingObjects);
			NamesOfExistingObjects = TextureShare->GetTextureShareObjectNames();


			for (const FString& ShareNameIt : PrevFrameObjectNames)
			{
				if (NamesOfExistingObjects.Contains(ShareNameIt) == false)
				{
					if (!ShareNameIt.IsEmpty())
					{
						TextureShareAPI().RemoveObject(ShareNameIt);
					}
				}
			}
		}

		// GetOrCreate and Tick existing objects
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* GameViewportClient = World->GetGameViewport())
			{
				if (FViewport* DstViewport = GameViewportClient->Viewport)
				{
					// Update existing objects
					for (const FString& ShareNameIt : NamesOfExistingObjects)
					{
						if (UTextureShareObject* TextureShareObject = TextureShare->GetTextureShareObject(ShareNameIt))
						{
							FTextureShareWorldSubsystemObject ObjectAPI(TextureShare->ProcessName, TextureShareObject);

							// tick game->render thread
							ObjectAPI.Tick(DstViewport);
						}
					}
				}
			}
		}
	}

	Super::Tick(DeltaTime);
}
