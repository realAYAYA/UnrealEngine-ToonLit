// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationDatabase.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "UObject/ObjectSaveContext.h"
#endif
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConversationDatabase)

#define LOCTEXT_NAMESPACE "ConversationDatabase"

UConversationDatabase::UConversationDatabase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UConversationDatabase::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

#if WITH_EDITOR
void UConversationDatabase::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	{
		// Give all nodes a new Guid
		for (UEdGraph* Graph : SourceGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				Node->CreateNewGuid();
			}
		}
		
		//@TODO: CONVERSATION: Rebuild/recompile now (can't reach that code in the graph module from here...)
	}
}
#endif

FLinearColor UConversationDatabase::GetDebugParticipantColor(FGameplayTag ParticipantID) const
{
	for (const FCommonDialogueBankParticipant& Speaker : Speakers)
	{
		if (Speaker.ParticipantName == ParticipantID)
		{
			return Speaker.NodeTint;
		}
	}

	return FLinearColor(0.15f, 0.15f, 0.15f);
}

#if WITH_EDITOR
void UConversationDatabase::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UConversationDatabase::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
}

EDataValidationResult UConversationDatabase::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult SuperResult = Super::IsDataValid(ValidationErrors);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetRegistrySerializationOptions SaveOptions;
	AssetRegistry.InitializeSerializationOptions(SaveOptions);

	EDataValidationResult Result = EDataValidationResult::Valid;

	if (SaveOptions.bUseAssetRegistryTagsAllowListInsteadOfDenyList)
	{
		const TSet<FName>* CookedAssetTagsForConversations = SaveOptions.CookFilterlistTagsByClass.Find(UConversationDatabase::StaticClass()->GetClassPathName());
		if (CookedAssetTagsForConversations == nullptr || !CookedAssetTagsForConversations->Contains(GET_MEMBER_NAME_CHECKED(UConversationDatabase, EntryTags)))
		{
			ValidationErrors.Add(FText::Format(LOCTEXT("Missing_EntryTags", "Missing from DefaultEngine.ini, {0}"),
				FText::FromString(TEXT("+CookedTagsWhitelist=(Class=ConversationDatabase,Tag=EntryTags)")))
			);

			Result = EDataValidationResult::Invalid;
		}
		if (CookedAssetTagsForConversations == nullptr || !CookedAssetTagsForConversations->Contains(GET_MEMBER_NAME_CHECKED(UConversationDatabase, ExitTags)))
		{
			ValidationErrors.Add(FText::Format(LOCTEXT("Missing_ExitTags", "Missing from DefaultEngine.ini, {0}"),
				FText::FromString(TEXT("+CookedTagsWhitelist=(Class=ConversationDatabase,Tag=ExitTags)")))
			);

			Result = EDataValidationResult::Invalid;
		}
		if (CookedAssetTagsForConversations == nullptr || !CookedAssetTagsForConversations->Contains(GET_MEMBER_NAME_CHECKED(UConversationDatabase, InternalNodeIds)))
		{
			ValidationErrors.Add(FText::Format(LOCTEXT("Missing_InternalNodeIds", "Missing from DefaultEngine.ini, {0}"),
				FText::FromString(TEXT("+CookedTagsWhitelist=(Class=ConversationDatabase,Tag=InternalNodeIds)")))
			);

			Result = EDataValidationResult::Invalid;
		}
		if (CookedAssetTagsForConversations == nullptr || !CookedAssetTagsForConversations->Contains(GET_MEMBER_NAME_CHECKED(UConversationDatabase, LinkedToNodeIds)))
		{
			ValidationErrors.Add(FText::Format(LOCTEXT("Missing_LinkedToNodeIds", "Missing from DefaultEngine.ini, {0}"),
				FText::FromString(TEXT("+CookedTagsWhitelist=(Class=ConversationDatabase,Tag=LinkedToNodeIds)")))
			);

			Result = EDataValidationResult::Invalid;
		}
	}

	return CombineDataValidationResults(SuperResult, Result);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
