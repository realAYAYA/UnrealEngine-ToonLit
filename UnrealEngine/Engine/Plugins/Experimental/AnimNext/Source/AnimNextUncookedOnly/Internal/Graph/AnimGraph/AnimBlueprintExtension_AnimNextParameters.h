// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimBlueprintExtension.h"
#include "Graph/AnimGraph/AnimSubsystem_AnimNextParameters.h"
#include "PropertyAccess.h"
#include "IPropertyAccessCompiler.h"
#include "AnimBlueprintExtension_AnimNextParameters.generated.h"

struct FAnimNextAnimGraphNodeParameterBinding;

UCLASS(MinimalAPI)
class UAnimBlueprintExtension_AnimNextParameters : public UAnimBlueprintExtension
{
	GENERATED_BODY()

private:
	friend class UAnimGraphNodeBinding_AnimNextParameters;

	void ProcessNodePins(UAnimGraphNode_Base* InNode, IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData);

	// UAnimBlueprintExtension interface
	virtual void HandleStartCompilingClass(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;

	void PatchEvaluationHandlers(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData);

	void RedirectPropertyPaths(IAnimBlueprintCompilationContext& InCompilationContext, UAnimGraphNode_Base* InNode);

	struct FSinglePropertyRecord
	{
		/** Parameter name to use for binding */
		FName ParameterName = NAME_None;

		/** The destination property we are bound to (on an animation node) */
		FProperty* Property = nullptr;

		/** Index into array if this is an array property element */
		int32 ArrayIndex = INDEX_NONE;

		/** The property path relative to the class */
		TArray<FString> PropertyPath;

		/** The handle of the copy in the property access compiler */
		FPropertyAccessHandle LibraryHandle;

		/** The handle of the copy in the property access library */
		FCompiledPropertyAccessHandle LibraryCompiledHandle;

		/** The type of the access in the property access library */
		EPropertyAccessCopyType LibraryCompiledAccessType = EPropertyAccessCopyType::None;
	};

	/** All the data needed for a single anim graph node's bindings */
	struct FBindingRecord
	{
		// The Node this record came from
		UAnimGraphNode_Base* AnimGraphNode = nullptr;

		// The node variable that the handler is in
		FStructProperty* NodeVariableProperty = nullptr;

		// Whether or not our serviced properties are actually on the anim node 
		bool bServicesNodeProperties = false;

		// Whether or not our serviced properties are actually on the instance instead of the node
		bool bServicesInstanceProperties = false;

		// Set of properties serviced by this handler (Map from property name to the record for that property)
		TMap<FName, FSinglePropertyRecord> ServicedProperties;

		bool IsValid() const
		{
			return NodeVariableProperty != nullptr;
		}

		void RegisterPropertyBinding(FProperty* InProperty, const FAnimNextAnimGraphNodeParameterBinding& InBinding);
	};

	UPROPERTY()
	FAnimSubsystem_AnimNextParameters Subsystem;

	// All the binding records we process during compilation
	TArray<FBindingRecord> BindingRecords;
	TMap<UAnimGraphNode_Base*, int32> BindingRecordIndexMap;

	// Delegate handle for registering against library pre/post-compilation
	FDelegateHandle PreLibraryCompiledDelegateHandle;
	FDelegateHandle PostLibraryCompiledDelegateHandle;
};