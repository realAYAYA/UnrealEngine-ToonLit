// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MovieGraphCommon.h"
#include "MovieGraphPin.generated.h"

// Forward Declares
class UMovieGraphNode;
class UMovieGraphEdge;

USTRUCT(BlueprintType)
struct FMovieGraphPinProperties
{
	GENERATED_BODY()

	FMovieGraphPinProperties() = default;
	explicit FMovieGraphPinProperties(const FName& InLabel, const EMovieGraphValueType PinType, bool bInAllowMultipleConnections)
		: Label(InLabel)
		, Type(PinType)
		, bAllowMultipleConnections(bInAllowMultipleConnections)
	{}

	static FMovieGraphPinProperties MakeBranchProperties(const FName& InLabel = NAME_None)
	{
		FMovieGraphPinProperties Properties(InLabel, EMovieGraphValueType::None, false);
		Properties.bIsBranch = true;
		return MoveTemp(Properties);
	}

	/** The name assigned to the pin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FName Label = NAME_None;

	/** The type of the pin. If the pin represents a branch, this type is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EMovieGraphValueType Type = EMovieGraphValueType::Float;

	/** Whether this pin can accept multiple connections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bAllowMultipleConnections = true;

	/** Whether this pin represents a branch. If it does not represent a branch, then it is a value-providing pin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bIsBranch = false;

	bool operator==(const FMovieGraphPinProperties& Other) const
	{
		return Label == Other.Label
			&& Type == Other.Type
			&& bAllowMultipleConnections == Other.bAllowMultipleConnections
			&& bIsBranch == Other.bIsBranch;
	}

	bool operator !=(const FMovieGraphPinProperties& Other) const
	{
		return !(*this == Other);
	}
};


UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphPin : public UObject
{
	GENERATED_BODY()

public:
	bool AddEdgeTo(UMovieGraphPin* InOtherPin);
	bool BreakEdgeTo(UMovieGraphPin* InOtherPin);
	bool BreakAllEdges();
	bool IsConnected() const;
	bool IsOutputPin() const;
	int32 EdgeCount() const;
	bool AllowsMultipleConnections() const;

	/** Gets the first pin connected to this pin. Returns nullptr if no valid connection exists. */
	UMovieGraphPin* GetFirstConnectedPin() const;

	/** Gets all connected pins. */
	TArray<UMovieGraphPin*> GetAllConnectedPins() const;

public:
	// The node that this pin belongs to.
	UPROPERTY(BlueprintReadOnly, Category = "Properties")
	TObjectPtr<UMovieGraphNode> Node = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Properties", meta = (ShowOnlyInnerProperties))
	FMovieGraphPinProperties Properties;

	/** 
	* A list of edges between pins. This is marked as TextExportTransient so that when we copy/paste nodes,
	* we don't copy the edges, as they are rebuilt after paste based on the editor graph connections.
	*/
	UPROPERTY(BlueprintReadOnly, TextExportTransient, Category = "Properties")
	TArray<TObjectPtr<UMovieGraphEdge>> Edges;
};