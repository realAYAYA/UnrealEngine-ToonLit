// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRCControllerId.h"
#include "AvaTransitionEnums.h"
#include "Conditions/AvaTransitionCondition.h"
#include "AvaTransitionRCControllerMatchCondition.generated.h"

class UAvaSceneSubsystem;
class URCVirtualPropertyBase;
struct FAvaTransitionScene;

USTRUCT()
struct FAvaTransitionRCControllerMatchConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Compare RC Controller Values", Category="Remote Control")
struct AVALANCHE_API FAvaTransitionRCControllerMatchCondition : public FAvaTransitionCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaTransitionRCControllerMatchConditionInstanceData;

	//~ Begin FAvaTransitionCondition
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const override;
	//~ End FAvaTransitionCondition

	//~ Begin FStateTreeNodeBase
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& InLinker) override;
	//~ End FStateTreeNodeBase

	//~ Begin FStateTreeConditionBase
	virtual bool TestCondition(FStateTreeExecutionContext& InContext) const override;
	//~ End FStateTreeConditionBase

	URCVirtualPropertyBase* GetController(const UAvaSceneSubsystem& InSceneSubsystem, const FAvaTransitionScene* InTransitionScene) const;

	UPROPERTY(EditAnywhere, Category="Parameter")
	FAvaRCControllerId ControllerId;

	UPROPERTY(EditAnywhere, Category="Parameter")
	EAvaTransitionComparisonResult ValueComparisonType = EAvaTransitionComparisonResult::None;

	TStateTreeExternalDataHandle<UAvaSceneSubsystem> SceneSubsystemHandle;
};
