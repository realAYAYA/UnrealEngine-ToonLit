// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditorAssetEditorImpl.h"

#include "CoreMinimal.h"
#include "UVEditorSubsystem.h"
#include "Editor.h"
#include "GameFramework/Actor.h"

namespace UE
{
namespace Geometry
{
	void FUVEditorAssetEditorImpl::LaunchUVEditor(const TArray<TObjectPtr<UObject>>& ObjectsIn)
	{
		UUVEditorSubsystem* UVSubsystem = GEditor->GetEditorSubsystem<UUVEditorSubsystem>();
		check(UVSubsystem);
		TArray<TObjectPtr<UObject>> ProcessedObjects;
		ConvertInputArgsToValidTargets(ObjectsIn, ProcessedObjects);
		UVSubsystem->StartUVEditor(ProcessedObjects);
	}

	bool FUVEditorAssetEditorImpl::CanLaunchUVEditor(const TArray<TObjectPtr<UObject>>& ObjectsIn)
	{
		UUVEditorSubsystem* UVSubsystem = GEditor->GetEditorSubsystem<UUVEditorSubsystem>();
		check(UVSubsystem);
		TArray<TObjectPtr<UObject>> ProcessedObjects;
		ConvertInputArgsToValidTargets(ObjectsIn, ProcessedObjects);
		return UVSubsystem->AreObjectsValidTargets(ProcessedObjects);
	}

	void FUVEditorAssetEditorImpl::ConvertInputArgsToValidTargets(const TArray<TObjectPtr<UObject>>& ObjectsIn, TArray<TObjectPtr<UObject>>& ObjectsOut) const
	{
		for (const TObjectPtr<UObject>& Object : ObjectsIn)
		{
			const AActor* Actor = Cast<const AActor>(Object);
			if (Actor)
			{
				TArray<UObject*> ActorAssets;
				Actor->GetReferencedContentObjects(ActorAssets);

				if (ActorAssets.Num() > 0)
				{
					for (UObject* Asset : ActorAssets)
					{
						ObjectsOut.AddUnique(Asset);
					}
				}
				else {
					// Need to transform actors to components here because that's what the UVEditor expects to have
					TInlineComponentArray<UActorComponent*> ActorComponents;
					Actor->GetComponents(ActorComponents);
					ObjectsOut.Append(ActorComponents);
				}
			}
			else
			{
				ObjectsOut.AddUnique(Object);
			}
		}
	}
}
}
