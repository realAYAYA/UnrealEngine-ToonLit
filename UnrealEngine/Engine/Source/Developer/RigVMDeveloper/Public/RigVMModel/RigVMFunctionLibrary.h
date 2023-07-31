// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMGraph.h"
#include "RigVMBuildData.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"
#include "RigVMFunctionLibrary.generated.h"

/**
 * The Function Library is a graph used only to store
 * the sub graphs used for functions.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMFunctionLibrary : public URigVMGraph
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMFunctionLibrary();

	// URigVMGraph interface
	virtual FString GetNodePath() const override;
	virtual URigVMFunctionLibrary* GetDefaultFunctionLibrary() const override;
	// end URigVMGraph interface

	// Returns all of the stored functions
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray<URigVMLibraryNode*> GetFunctions() const;

	// Finds a function by name
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	URigVMLibraryNode* FindFunction(const FName& InFunctionName) const;

	// Finds a function by a node within a function (or a sub graph of that)
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
    URigVMLibraryNode* FindFunctionForNode(URigVMNode* InNode) const;

	// returns the build data for for this library
	UFUNCTION()
	URigVMBuildData* GetBuildData() const;

	// Returns all references for a given function name
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray< TSoftObjectPtr<URigVMFunctionReferenceNode> > GetReferencesForFunction(const FName& InFunctionName);

	// Returns all references for a given function name
	UFUNCTION(BlueprintCallable, Category = RigVMGraph)
	TArray< FString > GetReferencePathsForFunction(const FName& InFunctionName);

	/**
	* Iterator function to invoke a lambda / TFunction for each reference of a function
	* @param InFunctionName The function name to iterate all references for
	* @param PerReferenceFunction The function to invoke for each reference
	*/
	void ForEachReference(const FName& InFunctionName, TFunction<void(URigVMFunctionReferenceNode*)> PerReferenceFunction) const;

	/**
	* Iterator function to invoke a lambda / TFunction for each reference of a function
	* @param InFunctionName The function name to iterate all references for
	* @param PerReferenceFunction The function to invoke for each reference
	*/
	void ForEachReferenceSoftPtr(const FName& InFunctionName, TFunction<void(TSoftObjectPtr<URigVMFunctionReferenceNode>)> PerReferenceFunction) const;

	// Returns a function that has been previously localized based on the provided function to localize.
	// We maintain meta data on what functions have been created locally based on which other ones,
	// and use this method to avoid redundant localizations.
	URigVMLibraryNode* FindPreviouslyLocalizedFunction(URigVMLibraryNode* InFunctionToLocalize);
	
private:

	UPROPERTY()
	TMap< TObjectPtr<URigVMLibraryNode>, FRigVMFunctionReferenceArray > FunctionReferences_DEPRECATED;

	// A map which stores a library node per original pathname.
	// The source pathname is the full path of the source function that was localized
	// to the local copy stored in the value of the pair.
	UPROPERTY()
	TMap< FString, TObjectPtr<URigVMLibraryNode> > LocalizedFunctions;

	friend class URigVMController;
	friend class URigVMCompiler;
	friend class UControlRigBlueprint;
};

