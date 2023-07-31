// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorFactoryMediaPlate.h"

#include "Async/Async.h"
#include "MediaSource.h"
#include "MediaPlate.h"
#include "MediaPlateComponent.h"
#include "MediaPlateEditorModule.h"
#include "MediaPlaylist.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorFactoryMediaPlate)

#define LOCTEXT_NAMESPACE "ActorFactoryMediaPlate"

UActorFactoryMediaPlate::UActorFactoryMediaPlate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("MediaPlateDisplayName", "Media Plate");
	NewActorClass = AMediaPlate::StaticClass();
}

bool UActorFactoryMediaPlate::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid())
	{
		UClass* AssetClass = AssetData.GetClass();
		if ((AssetClass != nullptr) && (AssetClass->IsChildOf(UMediaSource::StaticClass())))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return true;
	}
}

void UActorFactoryMediaPlate::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	SetUpActor(Asset, NewActor);
}

void UActorFactoryMediaPlate::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	SetUpActor(Asset, CDO);
}

void UActorFactoryMediaPlate::SetUpActor(UObject* Asset, AActor* Actor)
{
	if (Actor != nullptr)
	{
		AMediaPlate* MediaPlate = CastChecked<AMediaPlate>(Actor);

		// Hook up media source.
		UMediaSource* MediaSource = Cast<UMediaSource>(Asset);
		if ((MediaSource != nullptr) && (MediaPlate->MediaPlateComponent != nullptr))
		{
			UMediaPlaylist* Playlist = MediaPlate->MediaPlateComponent->MediaPlaylist;
			if (Playlist != nullptr)
			{
				// This gets called twice so only add the media source once.
				if (Playlist->Num() == 0)
				{
					Playlist->Add(MediaSource);

					// Is this media source from a drag and drop?
					FMediaPlateEditorModule* EditorModule = FModuleManager::LoadModulePtr<FMediaPlateEditorModule>("MediaPlateEditor");
					if (EditorModule != nullptr)
					{
						bool bIsInDragDropCache = EditorModule->RemoveMediaSourceFromDragDropCache(MediaSource);
						if (bIsInDragDropCache)
						{
							// Yes. Move this out of transient.
							// Can't do it here as the asset is still being used.
							TWeakObjectPtr<UMediaPlateComponent> MediaPlateComponentPtr(MediaPlate->MediaPlateComponent);
							AsyncTask(ENamedThreads::GameThread, [MediaPlateComponentPtr]()
							{
								UMediaPlateComponent* MediaPlateComponent = MediaPlateComponentPtr.Get();
								if (MediaPlateComponent != nullptr)
								{
									UMediaPlaylist* Playlist = MediaPlateComponent->MediaPlaylist;
									if ((Playlist != nullptr) && (Playlist->Num() > 0))
									{
										UMediaSource* MediaSource = Playlist->Get(0);
										if (MediaSource != nullptr)
										{
											MediaSource->Rename(nullptr, MediaPlateComponent);
										}
									}
								}
							});
						}
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

