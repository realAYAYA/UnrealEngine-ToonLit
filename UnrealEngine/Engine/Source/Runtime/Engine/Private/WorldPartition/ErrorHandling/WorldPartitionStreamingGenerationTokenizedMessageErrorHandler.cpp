// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationTokenizedMessageErrorHandler.h"

#include "GameFramework/Actor.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"

#define LOCTEXT_NAMESPACE "WorldPartition"

void ITokenizedMessageErrorHandler::OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetFullActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_HaveMissingRefsTo", "have missing references to")))
		->AddToken(FTextToken::Create(FText::FromString(ReferenceGuid.ToString())));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_MissingActorReference_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceGridPlacement(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	const FText SpatiallyLoadedActor(LOCTEXT("TokenMessage_WorldPartition_SpatiallyLoadedActor", "Spatially loaded actor"));
	const FText NonSpatiallyLoadedActor(LOCTEXT("TokenMessage_WorldPartition_NonSpatiallyLoadedActor", "Non-spatially loaded actor"));
	const EMessageSeverity::Type MessageSeverity = ActorDescView.GetIsSpatiallyLoaded() ? EMessageSeverity::Warning : EMessageSeverity::Error;

	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(MessageSeverity);
	Message->AddToken(FTextToken::Create(ActorDescView.GetIsSpatiallyLoaded() ? SpatiallyLoadedActor : NonSpatiallyLoadedActor))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetFullActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_References", "references")))
		->AddToken(FTextToken::Create(ReferenceActorDescView.GetIsSpatiallyLoaded() ? SpatiallyLoadedActor : NonSpatiallyLoadedActor))
		->AddToken(FActorToken::Create(ReferenceActorDescView.GetActorSoftPath().ToString(), ReferenceActorDescView.GetGuid(), FText::FromName(ReferenceActorDescView.GetActorLabelOrName())));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_StreamedActorReferenceAlwaysLoadedActor_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceDataLayers(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(),  FText::FromString(GetFullActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_ReferenceActorInOtherDataLayers", "references an actor in a different set of runtime data layers")))
		->AddToken(FActorToken::Create(ReferenceActorDescView.GetActorSoftPath().ToString(), ReferenceActorDescView.GetGuid(), FText::FromName(ReferenceActorDescView.GetActorLabelOrName())));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_ActorReferenceActorInAnotherDataLayer_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_Actor", "Actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetFullActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_ReferenceActorInOtherRuntimeGrid", "references an actor in a different runtime grid")))
		->AddToken(FActorToken::Create(ReferenceActorDescView.GetActorSoftPath().ToString(), ReferenceActorDescView.GetGuid(), FText::FromName(ReferenceActorDescView.GetActorLabelOrName())));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_ActorReferenceActorInAnotherRuntimeGrid_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceLevelScriptStreamed(const FWorldPartitionActorDescView& ActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelScriptBlueprintStreamedActorReference", "Level Script Blueprint references streamed actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetFullActorName(ActorDescView))));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_LevelScriptBlueprintRefefenceStreamed_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
	
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceLevelScriptDataLayers(const FWorldPartitionActorDescView& ActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelScriptBlueprintActorReference", "Level Script Blueprint references actor")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetFullActorName(ActorDescView))))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelScriptBlueprintDataLayerReference", "with a non empty set of data layers")));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_LevelScriptBlueprintRefefenceDataLayer_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnInvalidReferenceDataLayerAsset(const UDataLayerInstanceWithAsset* DataLayerInstance)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_DataLayer", "Data layer")))
		->AddToken(FUObjectToken::Create(DataLayerInstance))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_NullAsset", "Does not have Data Layer Asset")));

	AddAdditionalNameToken(Message, FName(TEXT("DataLayers_InvalidAsset_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnDataLayerHierarchyTypeMismatch(const UDataLayerInstance* DataLayerInstance, const UDataLayerInstance* Parent)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_DataLayer", "Data layer")))
		->AddToken(FTextToken::Create(FText::FromString(DataLayerInstance->GetDataLayerShortName())))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_IsRntime", "is Runtime but its parent data layer")))
		->AddToken(FTextToken::Create(FText::FromString(Parent->GetDataLayerShortName())))
		->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_DataLayers_IsNot", "is not")));

	AddAdditionalNameToken(Message, FName(TEXT("DataLayers_HierarchyTypeMisMatch_CheckForErrors")));

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
		->AddToken(FUObjectToken::Create(DataLayerInstance->GetAsset()));

	AddAdditionalNameToken(Message, FName(TEXT("DataLayers_AssetConflict_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

void ITokenizedMessageErrorHandler::OnActorNeedsResave(const FWorldPartitionActorDescView& ActorDescView)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_ActorNeedsResave", "Actor needs resave")))
		->AddToken(FActorToken::Create(ActorDescView.GetActorSoftPath().ToString(), ActorDescView.GetGuid(), FText::FromString(GetFullActorName(ActorDescView))));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_ActorNeedsResave_CheckForErrors")));

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
	};

	const FString WorldAssetStr = WorldAsset.ToString();
	const FString WorldAssetPath = WorldAssetStr + TEXT(".") + FPackageName::GetShortName(WorldAssetStr);

	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(MessageSeverity);
	Message->AddToken(FTextToken::Create(LOCTEXT("TokenMessage_WorldPartition_LevelInstance", "Level Instance")))
		->AddToken(FTextToken::Create(FText::FromString(GetFullActorName(ActorDescView))))
		->AddToken(FTextToken::Create(ReasonText))
		->AddToken(FAssetNameToken::Create(WorldAssetPath));

	AddAdditionalNameToken(Message, FName(TEXT("WorldPartition_LevelInstanceInvalidWorldAsset_CheckForErrors")));

	HandleTokenizedMessage(MoveTemp(Message));
}

#undef LOCTEXT_NAMESPACE

#endif
