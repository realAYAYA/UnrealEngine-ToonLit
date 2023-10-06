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

	// Returns the graph nesting depth of this link
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	int32 GetGraphDepth() const;

	// Returns the source Pin of this Link (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetSourcePin() const;

	// Returns the target Pin of this Link (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetTargetPin() const;

	// Returns the source Node of this Link (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMNode* GetSourceNode() const;

	// Returns the target Node of this Link (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMNode* GetTargetNode() const;

	// Returns the source pin's path pin of this Link
	FString GetSourcePinPath() const;

	// Returns the target pin's path pin of this Link
	FString GetTargetPinPath() const;

	// Sets the source pin's path pin of this Link
	bool SetSourcePinPath(const FString& InPinPath);

	// Sets the target pin's path pin of this Link
	bool SetTargetPinPath(const FString& InPinPath);

	// Sets the target pin's path pin of this Link
	bool SetSourceAndTargetPinPaths(const FString& InSourcePinPath, const FString& InTargetPinPath);

	// Returns the opposite Pin of this Link given one of its edges (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	URigVMPin* GetOppositePin(const URigVMPin* InPin) const;

	// Returns a string representation of the Link,
	// for example: "NodeA.Color.R -> NodeB.Translation.X"
	// note: can be split again using SplitPinPathRepresentation
	UFUNCTION(BlueprintCallable, Category = RigVMLink)
	FString GetPinPathRepresentation() const;

	// Returns a string representation of the Link given the two pin paths
	// for example: "NodeA.Color.R -> NodeB.Translation.X"
	// note: can be split again using SplitPinPathRepresentation
	static FString GetPinPathRepresentation(const FString& InSourcePinPath, const FString& InTargetPinPath);

	// Splits a pin path representation of a link
	// for example: "NodeA.Color.R -> NodeB.Translation.X"
	// into its two pin paths
	static bool SplitPinPathRepresentation(const FString& InString, FString& OutSource, FString& OutTarget);

	void UpdatePinPaths();
	void UpdatePinPointers() const;
	bool Detach();
	
private:

	// Returns true if the link is attached.
	// Attached links rely on the pin pointers first and the pin path second.
	// Deattached links never rely on the pin pointers and always try to resolve from string.
	bool IsAttached() const;
	bool Attach(FString* OutFailureReason = nullptr);

	UPROPERTY()
	FString SourcePinPath;

	UPROPERTY()
	FString TargetPinPath;

	mutable URigVMPin* SourcePin;
	mutable URigVMPin* TargetPin;
};

