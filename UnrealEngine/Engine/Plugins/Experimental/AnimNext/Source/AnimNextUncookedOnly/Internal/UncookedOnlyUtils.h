// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamTypeHandle.h"
#include "RigVMCore/RigVMTemplate.h"

class UAnimNextGraph;
class UAnimNextGraph_EditorData;
class UAnimNextGraph_EdGraph;
class URigVMController;
class URigVMGraph;
class UAnimNextParameter;
class UAnimNextParameterBlock;
class UAnimNextParameterBlock_EditorData;
class UAnimNextGraph_EdGraph;
struct FEdGraphPinType;

namespace UE::AnimNext::UncookedOnly
{

struct ANIMNEXTUNCOOKEDONLY_API FUtils
{
	static void Compile(UAnimNextGraph* InGraph);
	
	static UAnimNextGraph_EditorData* GetEditorData(const UAnimNextGraph* InAnimNextGraph);
	
	static void RecreateVM(UAnimNextGraph* InGraph);

	/**
	 * Get an AnimNext parameter type handle from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FParamTypeHandle GetParameterHandleFromPin(const FEdGraphPinType& InPinType);

	static void Compile(UAnimNextParameterBlock* InParameterBlock);

	static void CompileVM(UAnimNextParameterBlock* InParameterBlock);

	static void CompileStruct(UAnimNextParameterBlock* InParameterBlock);
	
	static UAnimNextParameterBlock_EditorData* GetEditorData(const UAnimNextParameterBlock* InParameterBlock);

	static UAnimNextParameterBlock* GetBlock(const UAnimNextParameterBlock_EditorData* InEditorData);

	static void RecreateVM(UAnimNextParameterBlock* InParameterBlock);

	/**
	 * Get an AnimNext parameter type from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FParamTypeHandle GetParamTypeHandleFromPinType(const FEdGraphPinType& InPinType);
	static FAnimNextParamType GetParamTypeFromPinType(const FEdGraphPinType& InPinType);

	/**
	 * Get an FEdGraphPinType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FEdGraphPinType GetPinTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle);
	static FEdGraphPinType GetPinTypeFromParamType(const FAnimNextParamType& InParamType);

	/**
	 * Get an FRigVMTemplateArgumentType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FRigVMTemplateArgumentType GetRigVMArgTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle);
	static FRigVMTemplateArgumentType GetRigVMArgTypeFromParamType(const FAnimNextParamType& InParamType);

	/** Set up a binding graph given the type to set */
	static void SetupBindingGraphForLiteral(URigVMController* InController, const FAnimNextParamType& InParamType);
};

}