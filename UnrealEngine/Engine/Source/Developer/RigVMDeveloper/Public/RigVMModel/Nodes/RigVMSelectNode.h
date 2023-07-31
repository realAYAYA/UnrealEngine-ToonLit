// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "RigVMTemplateNode.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "RigVMSelectNode.generated.h"

class UObject;
class URigVMPin;
struct FRigVMTemplate;

/**
 * A select node is used to select between multiple values
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMSelectNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:

	// Override from URigVMTemplateNode
	virtual FName GetNotation() const override;
	virtual const FRigVMTemplate* GetTemplate() const override;
	virtual bool IsSingleton() const override { return false; }

	// Override from URigVMNode
	virtual FString GetNodeTitle() const override { return SelectName; }
	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Black; }
	
protected:

	virtual bool AllowsLinksOn(const URigVMPin* InPin) const override;

private:

	static const FString SelectName;
	static const FString IndexName;
	static const FString ValueName;
	static const FString ResultName;

	friend class URigVMController;
	friend class URigVMCompiler;
	friend struct FRigVMAddSelectNodeAction;
	friend class UControlRigSelectNodeSpawner;
	friend class FRigVMParserAST;
	friend class FRigVMSelectExprAST;
	friend struct FRigVMRemoveNodeAction;
};

