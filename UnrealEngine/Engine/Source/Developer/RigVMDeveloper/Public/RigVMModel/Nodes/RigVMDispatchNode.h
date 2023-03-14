// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMCore/RigVMStructUpgradeInfo.h"
#include "RigVMCore/RigVMTemplate.h"
#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "RigVMDispatchNode.generated.h"

class UObject;
class URigVMPin;
class UScriptStruct;
struct FRigVMDispatchFactory;

/**
 * The Struct Node represents a Function Invocation of a RIGVM_METHOD
 * declared on a USTRUCT. Struct Nodes have input / output pins for all
 * struct UPROPERTY members.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMDispatchNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:

	// Override node functions
	virtual FString GetNodeTitle() const override;
	virtual FText GetToolTipText() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual bool IsDefinedAsConstant() const override;
	virtual bool IsDefinedAsVarying() const override;
	virtual TArray<URigVMPin*> GetAggregateInputs() const override;
	virtual TArray<URigVMPin*> GetAggregateOutputs() const override;
	virtual FName GetNextAggregateName(const FName& InLastAggregatePinName) const override;
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;

	const FRigVMDispatchFactory* GetFactory() const;
	bool IsDeprecated() const;
	FString GetDeprecatedMetadata() const;

protected:

	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;
	const UScriptStruct* GetFactoryStruct() const;
	const FRigVMTemplateTypeMap& GetFilteredTypes() const;
	virtual void InvalidateCache() override;

private:

	mutable FRigVMTemplateTypeMap TypesFromPins;
	mutable const FRigVMDispatchFactory* CachedFactory = nullptr;

	friend class URigVMController;
};

