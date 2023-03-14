// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameFeatureStateChangeObserver.h"
#include "GameplayTagContainer.h"
#include "ConversationDatabase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include "ConversationRegistry.generated.h"

class UGameFeatureData;
class UConversationDatabase;
class UWorld;
struct FStreamableHandle;

//  Container for safely replicating  script struct references (constrained to a specified parent struct)
USTRUCT()
struct COMMONCONVERSATIONRUNTIME_API FNetSerializeScriptStructCache_ConvVersion
{
	GENERATED_BODY()

	void InitForType(UScriptStruct* InScriptStruct);

	// Serializes reference to given script struct (must be in the cache)
	bool NetSerialize(FArchive& Ar, UScriptStruct*& Struct);

	UPROPERTY()
	TMap<TObjectPtr<UScriptStruct>, int32> ScriptStructsToIndex;

	UPROPERTY()
	TArray<TObjectPtr<UScriptStruct>> IndexToScriptStructs;
};

/**
 * These handles are issued when someone requests a conversation entry point be streamed in.
 * As long as this handle remains active, we were continue to keep it keep those elements streamed
 * in, as well as if new game feature plugins activate, we will stream in additional assets
 * or let previous ones expire.
 */
struct COMMONCONVERSATIONRUNTIME_API FConversationsHandle : public TSharedFromThis<FConversationsHandle>
{
	static TSharedPtr<FConversationsHandle> Create(const UConversationRegistry* InOwningRegistry, const TSharedPtr<FStreamableHandle>& InStreamableHandle, const TArray<FGameplayTag>& InEntryTags);

private:
	// Private token only allows members or friends to call MakeShared
	struct FPrivateToken { explicit FPrivateToken() = default; };

public:
	FConversationsHandle(FPrivateToken, UConversationRegistry* InOwningRegistry, const TSharedPtr<FStreamableHandle>& InStreamableHandle, const TArray<FGameplayTag>& InEntryTags);

private:
	void Initialize();
	void HandleAvailableConversationsChanged();

private:
	TSharedPtr<FStreamableHandle> StreamableHandle;
	TArray<FGameplayTag> ConversationEntryTags;
	TWeakObjectPtr<UConversationRegistry> OwningRegistryPtr;
};


DECLARE_MULTICAST_DELEGATE(FAvailableConversationsChangedEvent);

/**
 * A registry that can answer questions about all available dialogue assets
 */
UCLASS()
class COMMONCONVERSATIONRUNTIME_API UConversationRegistry : public UWorldSubsystem, public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

public:
	UConversationRegistry();

	static UConversationRegistry* GetFromWorld(const UWorld* World);

	/** UWorldSubsystem Begin */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** UWorldSubsystem End */

	UConversationNode* GetRuntimeNodeFromGUID(const FGuid& NodeGUID) const;
	TArray<FGuid> GetEntryPointGUIDs(FGameplayTag EntryPoint) const;

	TArray<FGuid> GetOutputLinkGUIDs(FGameplayTag EntryPoint) const;
	TArray<FGuid> GetOutputLinkGUIDs(const FGuid& SourceGUID) const;
	TArray<FGuid> GetOutputLinkGUIDs(const TArray<FGuid>& SourceGUIDs) const;

	TSharedPtr<FConversationsHandle> LoadConversationsFor(const FGameplayTag& ConversationEntryTag) const;
	TSharedPtr<FConversationsHandle> LoadConversationsFor(const TArray<FGameplayTag>& ConversationEntryTags) const;

	TArray<FPrimaryAssetId> GetPrimaryAssetIdsForEntryPoint(FGameplayTag EntryPoint) const;
	
	UPROPERTY(Transient)
	FNetSerializeScriptStructCache_ConvVersion ConversationChoiceDataStructCache;

	FAvailableConversationsChangedEvent AvailableConversationsChanged;

private:
	UConversationDatabase* GetConversationFromNodeGUID(const FGuid& NodeGUID) const;

	void BuildDependenciesGraph();
	void GetAllDependenciesForConversation(const FSoftObjectPath& Parent, TSet<FSoftObjectPath>& OutConversationsToLoad) const;

	void GameFeatureStateModified();

	virtual void OnGameFeatureActivating(const UGameFeatureData* GameFeatureData, const FString& PluginURL) override;

	virtual void OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, FGameFeatureDeactivatingContext& Context, const FString& PluginURL) override;

private:
	TMap<FSoftObjectPath, TArray<FSoftObjectPath>> RuntimeDependencyGraph;
	TMap<FGameplayTag, TArray<FSoftObjectPath>> EntryTagToConversations;
	TMap<FGameplayTag, TArray<FGuid>> EntryTagToEntryList;
	TMap<FGuid, FSoftObjectPath> NodeGuidToConversation;

private:
	bool bDependenciesBuilt = false;
};