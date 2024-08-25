// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "RigVMModel/RigVMPin.h"
#include "SGraphPin.h"

class RIGVMEDITOR_API SRigVMGraphPinQuat : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SRigVMGraphPinQuat)
		: _ModelPin(nullptr)
	{}
		SLATE_ARGUMENT(URigVMPin*, ModelPin)
	SLATE_END_ARGS()


	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	TOptional<FRotator> GetRotator() const;
	void OnRotatorCommitted(FRotator InRotator, ETextCommit::Type InCommitType, bool bUndoRedo);
	TOptional<float> GetRotatorComponent(int32 InComponent) const;
	void OnRotatorComponentChanged(float InValue, int32 InComponent);
	void OnRotatorComponentCommitted(float InValue, ETextCommit::Type InCommitType, int32 InComponent, bool bUndoRedo);

	URigVMPin* ModelPin;
};
