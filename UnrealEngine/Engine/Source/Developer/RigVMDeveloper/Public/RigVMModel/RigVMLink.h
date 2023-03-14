// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RigVMPin.h"
#include "RigVMLink.generated.h"

class URigVMGraph;

/**
 * The Link represents a connection between two Pins
 * within a Graph. The Link can be accessed on the 
 * Graph itself - or through the URigVMPin::GetLinks()
 * method.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMLink : public UObject
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMLink()
	{
		SourcePin = TargetPin = nullptr;
	}

	// Serialization override
	virtual void Serialize(FArchive& Ar) override;

	// Returns the current index of this Link within its owning Graph.
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	int32 GetLinkIndex() const;

	// Returns the Link's owning Graph/
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMGraph* GetGraph() const;

	// Returns the source Pin of this Link (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetSourcePin() const;

	// Returns the target Pin of this Link (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetTargetPin() const;

	// Returns the opposite Pin of this Link given one of its edges (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetOppositePin(const URigVMPin* InPin) const;

	// Returns a string representation of the Link,
	// for example: "NodeA.Color.R -> NodeB.Translation.X"
	// note: can be split again using SplitPinPathRepresentation
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	FString GetPinPathRepresentation();

	// Splits a pin path represenation of a link
	// for example: "NodeA.Color.R -> NodeB.Translation.X"
	// into its two pin paths
	static bool SplitPinPathRepresentation(const FString& InString, FString& OutSource, FString& OutTarget);

private:

	void PrepareForCopy();

	UPROPERTY()
	FString SourcePinPath;

	UPROPERTY()
	FString TargetPinPath;

	mutable URigVMPin* SourcePin;
	mutable URigVMPin* TargetPin;

	friend class URigVMController;
};

