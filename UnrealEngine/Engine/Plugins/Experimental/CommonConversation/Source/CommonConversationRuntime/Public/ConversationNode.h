// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"

#include "ConversationNode.generated.h"

class UWorld;

class UConversationNode;
class UConversationDatabase;
class UCommonDialogueConversation;
struct FConversationContext;

/**
 * 
 */
enum class EConversationNodeDescriptionVerbosity : uint8 
{
	Basic,
	Detailed,
};

//////////////////////////////////////////////////////////////////////

/**
 * 
 */
USTRUCT(BlueprintType)
struct COMMONCONVERSATIONRUNTIME_API FConversationNodeHandle
{
	GENERATED_BODY()

public:
	FConversationNodeHandle() : NodeGUID(0, 0, 0, 0) {}
	FConversationNodeHandle(const FGuid& InNodeGUID) : NodeGUID(InNodeGUID) { }

	UPROPERTY(EditAnywhere, Category=Conversation)
	FGuid NodeGUID;

	bool IsValid() const { return NodeGUID.IsValid(); }
	FString ToString() const { return NodeGUID.ToString(); }
	void Invalidate() { NodeGUID.Invalidate(); }

	FConversationNodeHandle& operator = (const FGuid& InNodeGUID)
	{
		NodeGUID = InNodeGUID;
		return *this;
	}

	operator FGuid& () { return NodeGUID; }
	operator FGuid() const { return NodeGUID; }

	/** 
	 * Tries to resolve the node, this may fail, the guid might be bogus, or the node might not be
	 * in memory.
	 */
	const UConversationNode* TryToResolve(const FConversationContext& Context) const;
	const UConversationNode* TryToResolve_Slow(UWorld* InWorld) const;
};

inline bool operator==(const FConversationNodeHandle& Lhs, const FConversationNodeHandle& Rhs) { return Lhs.NodeGUID == Rhs.NodeGUID; }
inline bool operator!=(const FConversationNodeHandle& Lhs, const FConversationNodeHandle& Rhs) { return !(Lhs == Rhs); }

//////////////////////////////////////////////////////////////////////

// Represents a single runtime node in the conversation database.
UCLASS(Abstract, Const)
class COMMONCONVERSATIONRUNTIME_API UConversationNode : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual UWorld* GetWorld() const override;

	/** fill in data about tree structure */
	void InitializeNode(UConversationNode* InParentNode);

	/** initialize any asset related data */
	virtual void InitializeFromAsset(UConversationDatabase& Asset);

	/** gathers description of all runtime parameters */
	virtual void DescribeRuntimeValues(const UCommonDialogueConversation& OwnerComp, EConversationNodeDescriptionVerbosity Verbosity, TArray<FString>& Values) const;

	/** @return parent node */
	UConversationNode* GetParentNode() const { return ParentNode; }

	/** @return name of node */
	FText GetDisplayNameText() const;

	/** @return string containing description of this node instance with all relevant runtime values */
	FText GetRuntimeDescription(const UCommonDialogueConversation& OwnerComp, EConversationNodeDescriptionVerbosity Verbosity) const;

	/** @return string containing description of this node with all setup values */
	virtual FText GetStaticDescription() const;

	/** The node's unique ID. */
	FGuid GetNodeGuid() const { return Compiled_NodeGUID; }

#if WITH_EDITOR
	/** Get the name of the icon used to display this node in the editor */
	virtual FName GetNodeIconName() const;

	/** Called after creating new node in behavior tree editor, use for versioning */
	virtual void OnNodeCreated() {}

	bool ShowPropertyEditors() const
	{
		return bShowPropertyEditors;
	}

	bool ShowPropertyDetails() const
	{
		if (bShowPropertyEditors)
		{
			return false;
		}

		return bShowPropertyDetails;
	}
#endif

protected:
	UFUNCTION(BlueprintCallable, Category=Conversation)
	FLinearColor GetDebugParticipantColor(FGameplayTag ParticipantID) const;

#if WITH_EDITOR
	/** @return true if this property should be hidden in static description, false if should be shown */
	bool ShouldHideProperty(FProperty* InTestProperty) const;
#endif // #if WITH_EDITOR

public:
	UPROPERTY(Transient, DuplicateTransient)
	mutable TObjectPtr<UObject> EvalWorldContextObj;

protected:
	/** node name */
	UPROPERTY(EditAnywhere, Category=Description, AdvancedDisplay)
	FString NodeName;

	/**
	 * The node's unique ID.  This value is set during compilation.
	 */
	UPROPERTY()
	FGuid Compiled_NodeGUID;

#if WITH_EDITORONLY_DATA
	/** show detailed information about properties */
	UPROPERTY(EditInstanceOnly, Category=Description, AdvancedDisplay)
	uint32 bShowPropertyDetails : 1;

	/** show detailed information about properties */
	UPROPERTY(EditInstanceOnly, Category=Description, AdvancedDisplay)
	uint32 bShowPropertyEditors : 1;
#endif

private:
	/** parent node */
	UPROPERTY()
	TObjectPtr<UConversationNode> ParentNode;

private:
	friend class FConversationCompiler;
};

//////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class COMMONCONVERSATIONRUNTIME_API UConversationNodeWithLinks : public UConversationNode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FGuid> OutputConnections;

	friend class UConversationRegistry;
};
