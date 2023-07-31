// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"
#include "RigVMBuildData.generated.h"

USTRUCT(BlueprintType)
struct RIGVMDEVELOPER_API FRigVMFunctionReferenceArray
{
	GENERATED_BODY()

	// Resets the data structure and maintains all storage.
	void Reset() { FunctionReferences.Reset();  }

	// Returns true if a given function reference index is valid.
	bool IsValidIndex(int32 InIndex) const { return FunctionReferences.IsValidIndex(InIndex); }

	// Returns the number of reference functions
	FORCEINLINE int32 Num() const { return FunctionReferences.Num(); }

	// const accessor for an function reference given its index
	FORCEINLINE const TSoftObjectPtr<URigVMFunctionReferenceNode>& operator[](int32 InIndex) const { return FunctionReferences[InIndex]; }

	UPROPERTY(VisibleAnywhere, Category = "BuildData")
	TArray< TSoftObjectPtr<URigVMFunctionReferenceNode> > FunctionReferences;
};

USTRUCT()
struct RIGVMDEVELOPER_API FRigVMReferenceNodeData
{
	GENERATED_BODY();

public:
	
	FRigVMReferenceNodeData()
		:ReferenceNodePath()
		,ReferencedFunctionPath()
	{}

	FRigVMReferenceNodeData(URigVMFunctionReferenceNode* InReferenceNode);

	UPROPERTY()
	FString ReferenceNodePath;

	UPROPERTY()
	FString ReferencedFunctionPath;

	TSoftObjectPtr<URigVMFunctionReferenceNode> GetReferenceNodeObjectPath();
	TSoftObjectPtr<URigVMLibraryNode> GetReferencedFunctionObjectPath();
	URigVMFunctionReferenceNode* GetReferenceNode();
	URigVMLibraryNode* GetReferencedFunction();

private:

	TSoftObjectPtr<URigVMFunctionReferenceNode> ReferenceNodePtr;
	TSoftObjectPtr<URigVMLibraryNode> LibraryNodePtr;
};

/**
 * The Build Data is used to store transient / intermediate build information
 * for the RigVM graph to improve the user experience.
 * This object is never serialized.
 */
UCLASS()
class RIGVMDEVELOPER_API URigVMBuildData : public UObject
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMBuildData();

	// Returns the list of references for a given function definition
	const FRigVMFunctionReferenceArray* FindFunctionReferences(const URigVMLibraryNode* InFunction) const;

	/**
	 * Iterator function to invoke a lambda / TFunction for each reference of a function
	 * @param InFunction The function to iterate all references for
	 * @param PerReferenceFunction The function to invoke for each reference
	 */
	void ForEachFunctionReference(const URigVMLibraryNode* InFunction, TFunction<void(URigVMFunctionReferenceNode*)> PerReferenceFunction) const;

	/**
	* Iterator function to invoke a lambda / TFunction for each reference of a function
	* @param InFunction The function to iterate all references for
	* @param PerReferenceFunction The function to invoke for each reference
	*/
	void ForEachFunctionReferenceSoftPtr(const URigVMLibraryNode* InFunction, TFunction<void(TSoftObjectPtr<URigVMFunctionReferenceNode>)> PerReferenceFunction) const;

	// Update the references list for a given reference node
	void UpdateReferencesForFunctionReferenceNode(URigVMFunctionReferenceNode* InReferenceNode);

	// registers a new reference node for a given function
	void RegisterFunctionReference(URigVMLibraryNode* InFunction, URigVMFunctionReferenceNode* InReference);

	// registers a new reference node for a given function
	void RegisterFunctionReference(TSoftObjectPtr<URigVMLibraryNode> InFunction, TSoftObjectPtr<URigVMFunctionReferenceNode> InReference);

	// registers a new reference node for a given function
	void RegisterFunctionReference(FRigVMReferenceNodeData InReferenceNodeData);

	// unregisters a new reference node for a given function
	void UnregisterFunctionReference(URigVMLibraryNode* InFunction, URigVMFunctionReferenceNode* InReference);

	// unregisters a new reference node for a given function
	void UnregisterFunctionReference(TSoftObjectPtr<URigVMLibraryNode> InFunction, TSoftObjectPtr<URigVMFunctionReferenceNode> InReference);

	// Clear references to temp assets
	void ClearInvalidReferences();

	// Helper function to disable clearing transient package references
	void SetIsRunningUnitTest(bool bIsRunning) { bIsRunningUnitTest = bIsRunning; }

private:

	UPROPERTY(VisibleAnywhere, Category = "BuildData")
	TMap< TSoftObjectPtr<URigVMLibraryNode>, FRigVMFunctionReferenceArray > FunctionReferences;

	bool bIsRunningUnitTest;

	friend class URigVMController;
	friend class URigVMCompiler;
};

