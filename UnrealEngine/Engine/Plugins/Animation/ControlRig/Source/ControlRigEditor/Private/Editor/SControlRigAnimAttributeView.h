// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FControlRigEditor;
class UControlRigBlueprint;
class UControlRig;
enum class EControlRigState : uint8;

class SAnimAttributeView;


class SControlRigAnimAttributeView : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SControlRigAnimAttributeView )
	{}
	
	SLATE_END_ARGS()

	~SControlRigAnimAttributeView();
	
	void Construct( const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

	void HandleSetObjectBeingDebugged(UObject* InObject);
	
	void StartObservingNewControlRig(UControlRig* InControlRig);
	void StopObservingCurrentControlRig();

	void HandleControlRigPostForwardSolve(UControlRig* InControlRig, const EControlRigState InState, const FName& InEventName) const;
	
	TWeakPtr<FControlRigEditor> ControlRigEditor;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	TWeakObjectPtr<UControlRig> ControlRigBeingDebuggedPtr;
	TSharedPtr<SAnimAttributeView> AttributeView;
};
