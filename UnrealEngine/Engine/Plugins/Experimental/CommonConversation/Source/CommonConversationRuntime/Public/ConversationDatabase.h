// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Engine/Blueprint.h"

#include "ConversationDatabase.generated.h"

class UConversationGraph;
class FConversationCompiler;
class UEdGraph;
class UConversationNode;
class UConversationRegistry;

/**
 * There may be multiple databases with the same entrypoint tag, this struct holds
 * all of those nodes with the same matching tag name, so that the entry point is
 * effectively randomized when there are multiple copies.
 */
USTRUCT()
struct FConversationEntryList
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag EntryTag;

	UPROPERTY()
	TArray<FGuid> DestinationList;
};

//////////////////////////////////////////////////////////////////////
//
// This struct represents a logical participant in a conversation.
//
// In an active conversation, logical participants are mapped to actual participants
// (e.g., mapping a logical Player to the current player pawn)
//

USTRUCT()
struct FCommonDialogueBankParticipant
{
	GENERATED_BODY()

	UPROPERTY()
	FText FallbackName;

	/** Identifier represented by the component */
	UPROPERTY(EditAnywhere, Category=Identification, meta=(Categories="Conversation.Participant"))
	FGameplayTag ParticipantName;

	UPROPERTY(EditAnywhere, Category = Identification)
	FLinearColor NodeTint = FLinearColor::White;

	//UPROPERTY()
	//UCommonDialogueSpeakerInfo* SpeakerInfo;
};

//////////////////////////////////////////////////////////////////////
// This is a database of conversation graphs and participants
// It is an asset and never instanced.  The conversation registry is used
// at runtime to actually run a conversation rather than referencing these
// database fragments directly.

UCLASS()
class COMMONCONVERSATIONRUNTIME_API UConversationDatabase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UConversationDatabase(const FObjectInitializer& ObjectInitializer);

	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

#if WITH_EDITOR
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif

	bool IsNodeReachable(const FGuid& NodeGUID) const { return ReachableNodeMap.Contains(NodeGUID); }

	FLinearColor GetDebugParticipantColor(FGameplayTag ParticipantID) const;

private: // Compiled Data
	
	// Compiled: Entry points
	UPROPERTY(AssetRegistrySearchable)
	int32 CompilerVersion = INDEX_NONE;

	// Compiled: Reachable nodes
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UConversationNode>> ReachableNodeMap;

	// Compiled: Entry points
	UPROPERTY(AssetRegistrySearchable)
	TArray<FConversationEntryList> EntryTags;

	// Compiled: 
	UPROPERTY(AssetRegistrySearchable)
	FGameplayTagContainer ExitTags;
	
	// Compiled: 
	UPROPERTY(AssetRegistrySearchable)
	TArray<FGuid> InternalNodeIds;

	// Compiled: 
	UPROPERTY(AssetRegistrySearchable)
	TArray<FGuid> LinkedToNodeIds;

private:
	// List of participant slots
	UPROPERTY(EditAnywhere, Category=Conversation)
	TArray<FCommonDialogueBankParticipant> Speakers;

private:

#if WITH_EDITORONLY_DATA
	// All nodes
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UConversationNode>> FullNodeMap;

	// 'Source code' graphs (of type UConversationGraph)
	UPROPERTY()
	TArray<TObjectPtr<UEdGraph>> SourceGraphs;

public:
	// Info about the graphs we last edited
	UPROPERTY()
	TArray<FEditedDocumentInfo> LastEditedDocuments;
#endif

private:
	friend FConversationCompiler;
	friend UConversationRegistry;
};
