// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorObjectNameModel.h"

#include "Internationalization/Text.h"
#include "Replication/ObjectUtils.h"

#include "GameFramework/Actor.h"
#include "SubobjectDataSubsystem.h"

namespace UE::ConcertClientSharedSlate
{
	namespace Private
	{
		static FText FindSubobjectDisplayName(const UObject& Subbject, AActor& OwningActor)
		{
			USubobjectDataSubsystem* SubobjectDataSubsystem = USubobjectDataSubsystem::Get();
			TArray<FSubobjectDataHandle> Handles;
			SubobjectDataSubsystem->GatherSubobjectData(&OwningActor, Handles);
				
			for (const FSubobjectDataHandle& Handle : Handles)
			{
				const FSubobjectData* SubobjectData = Handle.GetData();
				const UObject* Object = SubobjectData->FindComponentInstanceInActor(&OwningActor);
				if (Object == &Subbject)
				{
					return FText::FromString(SubobjectData->GetDisplayString());
				}
			}

			return FText::GetEmpty();
		}
	}
	
	FText FEditorObjectNameModel::GetObjectDisplayName(const FSoftObjectPath& ObjectPath) const
	{
		if (UObject* ResolvedObject = ObjectPath.ResolveObject())
		{
			// Display actor just like the outliner does
			if (const AActor* AsActor = Cast<AActor>(ResolvedObject))
			{
				return FText::FromString(AsActor->GetActorLabel());
			}

			// Display the same component name as the SSubobjectEditor widget does, i.e. component hierarchy in the details panel or Blueprint editor.
			AActor* OwningActor = ResolvedObject->GetTypedOuter<AActor>();
			const FText FoundSubobjectName = OwningActor ? Private::FindSubobjectDisplayName(*ResolvedObject, *OwningActor) : FText::GetEmpty();
			if (!FoundSubobjectName.IsEmpty())
			{
				return FoundSubobjectName;
			}

			return FText::FromString(ResolvedObject->GetName());
		}
		
		return FText::FromString(ConcertSharedSlate::ObjectUtils::ExtractObjectDisplayStringFromPath(ObjectPath));
	}
}
