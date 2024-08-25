// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MovieGraphCommon.h"
#include "MovieGraphPin.generated.h"

// Forward Declares
class UMovieGraphNode;
class UMovieGraphEdge;

struct FPinConnectionResponse;

USTRUCT(BlueprintType)
struct FMovieGraphPinProperties
{
	GENERATED_BODY()

	FMovieGraphPinProperties() = default;
	explicit FMovieGraphPinProperties(const FName& InLabel, const EMovieGraphValueType PinType, const TObjectPtr<const UObject>& TypeObject, bool bInAllowMultipleConnections)
		: Label(InLabel)
		, Type(PinType)
		, TypeObject(TypeObject)
		, bAllowMultipleConnections(bInAllowMultipleConnections)
	{}

	static FMovieGraphPinProperties MakeBranchProperties(const FName& InLabel = NAME_None)
	{
		FMovieGraphPinProperties Properties(InLabel, EMovieGraphValueType::None, nullptr, false);
		Properties.bIsBranch = true;
		return MoveTemp(Properties);
	}

	/** The name assigned to the pin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FName Label = NAME_None;

	/** The type of the pin. If the pin represents a branch, this type is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EMovieGraphValueType Type = EMovieGraphValueType::Float;

	/** The value type of the pin, if the type is an enum, struct, class, or object. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TObjectPtr<const UObject> TypeObject;

	/** Whether this pin can accept multiple connections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bAllowMultipleConnections = true;

	/** Whether this pin represents a branch. If it does not represent a branch, then it is a value-providing pin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bIsBranch = false;

	/**
	 * Whether this pin is built-in (ie, the pin ships with the node and cannot be removed). Option pins on the Select
	 * node would be an example of pins which are not built-in (they can be added and removed dynamically).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bIsBuiltIn = true;

	bool operator==(const FMovieGraphPinProperties& Other) const
	{
		return Label == Other.Label
			&& Type == Other.Type
			&& TypeObject == Other.TypeObject
			&& bAllowMultipleConnections == Other.bAllowMultipleConnections
			&& bIsBranch == Other.bIsBranch
			&& bIsBuiltIn == Other.bIsBuiltIn;
	}

	bool operator !=(const FMovieGraphPinProperties& Other) const
	{
		return !(*this == Other);
	}
};

/** Specifies a restriction on pin properties when searching for a pin on a node. */
UENUM(BlueprintType)
enum class EMovieGraphPinQueryRequirement : uint8
{
	/** The pin must be built-in, meaning that it is always present on the node. */
	BuiltIn,

	/** The pin must be dynamic, meaning that it may not always exist on the node. These are typically user-created pins. */
	Dynamic,

	/** The pin can be either built-in or dynamic. */
	BuiltInOrDynamic
};


UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphPin : public UObject
{
	GENERATED_BODY()

public:
	// UObject Interface
#if WITH_EDITOR
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
#endif
	// End UObject Interface
	
	bool AddEdgeTo(UMovieGraphPin* InOtherPin);
	bool BreakEdgeTo(UMovieGraphPin* InOtherPin);
	bool BreakAllEdges();
	FPinConnectionResponse CanCreateConnection_PinConnectionResponse(const UMovieGraphPin* InOtherPin) const;
	bool CanCreateConnection(const UMovieGraphPin* InOtherPin) const;
	bool IsConnected() const;
	bool IsInputPin() const;
	bool IsOutputPin() const;
	int32 EdgeCount() const;
	bool AllowsMultipleConnections() const;

	/** Gets the first pin connected to this pin. Returns nullptr if no valid connection exists. */
	UMovieGraphPin* GetFirstConnectedPin() const;

	/** Gets all connected pins. */
	TArray<UMovieGraphPin*> GetAllConnectedPins() const;

	/**
	* Utility function for scripting which gathers all of the nodes connected
	* to this particular pin. Equivalent to looping through all of the edges,
	* getting the connected pin, and then getting the node associated with that pin.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	TArray<UMovieGraphNode*> GetConnectedNodes() const;

	/**
	 * Determines if the connection between this pin and OtherPin follows branch restriction rules. OutError is populated
	 * with an error if the connection should be rejected and the function will return false.
	 */
	bool IsConnectionToBranchAllowed(const UMovieGraphPin* OtherPin, FText& OutError) const;
	bool IsPinDirectionCompatibleWith(const UMovieGraphPin* OtherPin) const;

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