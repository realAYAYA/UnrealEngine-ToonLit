// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationTokenizedMessageErrorHandler.h"

#include "Misc/PackageName.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/WorldPartitionActorDescView.h"

#define LOCTEXT_NAMESPACE "WorldPartition"

void ITokenizedMessageErrorHandler::OnInvalidRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, FName GridName)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_HaveInvalidRuntimeGrid", "has an invalid runtime grid")))
		->AddToken(FTextToken::Create(FText::FromName(GridName)))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_InvalidRuntimeGrid_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid, FWorldPartitionActorDescView* ReferenceActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_HaveMissingRefsTo", "has an invalid reference to")));

	if (ReferenceActorDescView)
	{
		Message->AddToken(FActorToken::Create(ReferenceActorDescView->GetActorSoftPath().ToString(), ReferenceActorDescView->GetGuid(), FText::FromString(GetActorName(*ReferenceActorDescView))));
	}
	else
	{
		Message->AddToken(FTextToken::Create(FText::FromString(ReferenceGuid.ToString())));
	}

	Message->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_MissingActorReference_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceGridPlacement(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	const FText SpatiallyLoadedActor(LOCTEXT("TokenMessage_WorldPartition_SpatiallyLoadedActor", "Spatially loaded actor"));
	const FText NonSpatiallyLoadedActor(LOCTEXT("TokenMessage_WorldPartition_NonSpatiallyLoadedActor", "Non-spatially loaded actor"));
	const EMessageSeverity::Type MessageSeverity = ActorDescView.GetIsSpatiallyLoaded() ? EMessageSeverity::Warning : EMessageSeverity::Error;

	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(MessageSeverity);
	Message->AddToken(FTextToken::Create(ActorDescView.GetIsSpatiallyLoaded() ? SpatiallyLoadedActor : NonSpatiallyLoadedActor))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_References", "references")))
		->AddToken(FTextToken::Create(ReferenceActorDescView.GetIsSpatiallyLoaded() ? SpatiallyLoadedActor : NonSpatiallyLoadedActor))
		->AddToken(FActorToken::Create(ReferenceActorDescView.GetActorSoftPath().ToString(), ReferenceActorDescView.GetGuid(), FText::FromString(GetActorName(ReferenceActorDescView))))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_StreamedActorReferenceAlwaysLoadedActor_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceDataLayers(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(),  FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_ReferenceActorInOtherDataLayers", "references an actor in a different set of runtime data layers")))
		->AddToken(FActorToken::Create(ReferenceActorDescView.GetActorSoftPath().ToString(), ReferenceActorDescView.GetGuid(), FText::FromString(GetActorName(ReferenceActorDescView))))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_ActorReferenceActorInAnotherDataLayer_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_ReferenceActorInOtherRuntimeGrid", "references an actor in a different runtime grid")))
		->AddToken(FActorToken::Create(ReferenceActorDescView.GetActorSoftPath().ToString(), ReferenceActorDescView.GetGuid(), FText::FromString(GetActorName(ReferenceActorDescView))))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_ActorReferenceActorInAnotherRuntimeGrid_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceLevelScriptStreamed(const FWorldPartitionActorDescView& ActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelScriptBlueprintStreamedActorReference", "Level Script Blueprint references streamed actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_LevelScriptBlueprintRefefenceStreamed_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
	
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceLevelScriptDataLayers(const FWorldPartitionActorDescView& ActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelScriptBlueprintActorReference", "Level Script Blueprint references actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelScriptBlueprintDataLayerReference", "with a non empty set of data layers")))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_LevelScriptBlueprintRefefenceDataLayer_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceDataLayerAsset(const UDataLayerInstanceWithAsset* DataLayerInstance)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_DataLayer", "Data layer")))
		->AddToken(FUObjectToken::Create(DataLayerInstance))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_NullAsset", "Does not have Data Layer Asset")))
		->AddToken(FMapErrorToken::Create(TEXT("DataLayers_InvalidAsset_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnDataLayerHierarchyTypeMismatch(const UDataLayerInstance* DataLayerInstance, const UDataLayerInstance* Parent)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_DataLayer", "Data layer")))
		->AddToken(FTextToken::Create(FText::FromString(DataLayerInstance->GetDataLayerShortName())))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_IsRntime", "is Runtime but its parent data layer")))
		->AddToken(FTextToken::Create(FText::FromString(Parent->GetDataLayerShortName())))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_IsNot", "is not")))
		->AddToken(FMapErrorToken::Create(TEXT("DataLayers_HierarchyTypeMisMatch_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnDataLayerAssetConflict(const UDataLayerInstanceWithAsset* DataLayerInstance, const UDataLayerInstanceWithAsset* ConflictingDataLayerInstance)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_DataLayerInstance", "Data layer Instance")))
		->AddToken(FUObjectToken::Create(DataLayerInstance))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_AndDataLayerInstance", "and Data Layer Instance")))
		->AddToken(FUObjectToken::Create(ConflictingDataLayerInstance))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_BothReferencing", "are both referencing Data Layer Asset")))
		->AddToken(FUObjectToken::Create(DataLayerInstance->GetAsset()))
		->AddToken(FMapErrorToken::Create(TEXT("DataLayers_AssetConflict_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnActorNeedsResave(const FWorldPartitionActorDescView& ActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Info);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_ActorNeedsResave", "Actor needs resave")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_ActorNeedsResave_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnLevelInstanceInvalidWorldAsset(const FWorldPartitionActorDescView& ActorDescView, FName WorldAsset, ELevelInstanceInvalidReason Reason)
{
	FSoftObjectPath ActorPath(ActorDescView.GetActorSoftPath());
	EMessageSeverity::Type MessageSeverity = EMessageSeverity::Info;
	FText ReasonText;

	switch (Reason)
	{
	case ELevelInstanceInvalidReason::WorldAssetNotFound:
		MessageSeverity = EMessageSeverity::Error;
		ReasonText = LOCTEXT("TokenMessage_WorldPartition_HasInvalidWorldAsset", "has an invalid world asset");
		break;
	case ELevelInstanceInvalidReason::WorldAssetNotUsingExternalActors:
		MessageSeverity = EMessageSeverity::Error;
		ReasonText = LOCTEXT("TokenMessage_WorldPartition_WorldAssetIsNotUsingExternalActors", "is not using external actors");
		break;
	case ELevelInstanceInvalidReason::WorldAssetImcompatiblePartitioned:
		MessageSeverity = EMessageSeverity::Error;
		ReasonText = LOCTEXT("TokenMessage_WorldPartition_WorldAssetIsPartitionedIncompatible", "is partitioned but not marked as compatible");
		break;
	case ELevelInstanceInvalidReason::WorldAssetHasInvalidContainer:
		MessageSeverity = EMessageSeverity::Error;
		ReasonText = LOCTEXT("TokenMessage_WorldPartition_WorldAssetHasInvalidContainer", "has an invalid container");
		break;
	case ELevelInstanceInvalidReason::CirculalReference:
		MessageSeverity = EMessageSeverity::Error;
		ReasonText = LOCTEXT("TokenMessage_WorldPartition_WorldAssetHasCircularReference", "has a circular reference");
		break;
	};

	const FString WorldAssetStr = WorldAsset.ToString();
	const FString WorldAssetPath = WorldAssetStr + TEXT(".") + FPackageName::GetShortName(WorldAssetStr);

	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(MessageSeverity);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelInstance", "Level Instance")))
		->AddToken(FTextToken::Create(FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FTextToken::Create(ReasonText))
		->AddToken(FAssetNameToken::Create(WorldAssetPath))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_LevelInstanceInvalidWorldAsset_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidActorFilterReference(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ReferenceActorDescView.GetActorSoftPath().ToString(), ReferenceActorDescView.GetGuid(), FText::FromString(GetActorName(ReferenceActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_ActorFilterFailed", "will not be filtered out because it is referenced by actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetActorName(ActorDescView))))
		->AddToken(FMapErrorToken::Create(TEXT("WorldPartition_MissingActorReference_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

#undef LOCTEXT_NAMESPACE

#endif
