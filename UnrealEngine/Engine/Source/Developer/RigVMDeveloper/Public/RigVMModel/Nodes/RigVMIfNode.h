// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "RigVMTemplateNode.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "RigVMIfNode.generated.h"

class UObject;
struct FRigVMTemplate;

/**
 * A if node is used to pick between two values
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMIfNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:

	// Override from URigVMTemplateNode
	virtual FName GetNotation() const override;
	virtual const FRigVMTemplate* GetTemplate() const override;
	virtual bool IsSingleton() const override { return false; }

	// Override from URigVMNode
	virtual FString GetNodeTitle() const override { return IfName; }
	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Black; }
private:

	static const FString IfName;
	static const FString ConditionName;
	static const FString TrueName;
	static const FString FalseName;
	static const FString ResultName;

	friend class URigVMController;
	friend class URigVMCompiler;
	friend struct FRigVMAddIfNodeAction;
	friend class UControlRigIfNodeSpawner;
	friend class FRigVMParserAST;
	friend struct FRigVMRemoveNodeAction;
};

