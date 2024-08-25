// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/ObjectSource/ActorSelectionSourceModel.h"

#include "Replication/Editor/Model/ObjectSource/SelectedActorsSource.h"
#include "Replication/Editor/Model/ObjectSource/WorldActorSource.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "FEditorObjectSelectionSourceModel"

namespace UE::ConcertClientSharedSlate
{
	FActorSelectionSourceModel::FActorSelectionSourceModel()
		: BaseActorCategory([this]()
		{
			return ConcertSharedSlate::FObjectSourceCategory
			{
				ConcertSharedSlate::FBaseDisplayInfo
				{
					LOCTEXT("ObjectsCategory.Label", "Add Objects"),
					LOCTEXT("ObjectsCategory.Tooltip", "Options for adding objects from the open editor world")
				},
				{
					MakeShared<FSelectedActorsSource>(),
					MakeShared<FWorldActorSource>()
				}
			};
		}())
	{}

	TArray<ConcertSharedSlate::FObjectSourceCategory> FActorSelectionSourceModel::GetRootSources() const
	{
		ConcertSharedSlate::FObjectSourceCategory ActorCategory = BaseActorCategory;
		return { ActorCategory };
	}

	TArray<TSharedRef<ConcertSharedSlate::IObjectSourceModel>> FActorSelectionSourceModel::GetContextMenuOptions(const FSoftObjectPath& Item)
	{
		return {};
	}
}

#undef LOCTEXT_NAMESPACE