// Copyright Epic Games, Inc. All Rights Reserved.

/* <<<< THIS IS TEMPORARY PROTOTYPE CODE >>>>
 *
 * This is essentially a copy + paste of the
 * equivalent file in engine code and is
 * intended for prototyping work as part of
 * the Ch5 locomotion initiative.
 *
 */

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "AnimNode_RigidBodyWithControl.h"
#include "Engine/PoseWatch.h"
#include "AnimGraphNode_RigidBodyWithControl.generated.h"

UCLASS(meta=(Keywords = "Simulate Rigid Body Physics Ragdoll"))
class PHYSICSCONTROLUNCOOKEDONLY_API UAnimGraphNode_RigidBodyWithControl : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_RigidBodyWithControl Node;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base
	virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp, const bool bIsSelected, const bool bIsPoseWatchEnabled) const override;
	virtual void OnPoseWatchChanged(const bool IsPoseWatchActive, TObjectPtr<UPoseWatch> InPoseWatch, FEditorModeTools& InModeTools, FAnimNode_Base* InRuntimeNode) override;

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;

	void ToggleBodyVisibility();
	void ToggleConstraintVisibility();
	void ToggleControlEditorTab();

	TArray<TPair<FName, TArray<FName>>> GenerateControlsAndBodyModifierNames() const;

	USkeleton* GetSkeleton() const;

protected:
	// UAnimGraphNode_SkeletalControlBase interface
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	// End of UAnimGraphNode_SkeletalControlBase interface

	bool AreAnyBodiesHidden() const;
	bool AreAnyConstraintsHidden() const;
	bool IsControlEditorTabOpen() const;

	void PostChange();

private:
	TWeakObjectPtr<UPoseWatchElement> PoseWatchElementBodies;
	TWeakObjectPtr<UPoseWatchElement> PoseWatchElementConstraints;
	TWeakObjectPtr<UPoseWatchElement> PoseWatchElementParentSpaceControls;
	TWeakObjectPtr<UPoseWatchElement> PoseWatchElementWorldSpaceControls;
};
