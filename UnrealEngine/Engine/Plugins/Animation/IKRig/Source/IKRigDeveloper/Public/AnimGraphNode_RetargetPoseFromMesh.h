// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_Base.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "AnimGraphNode_RetargetPoseFromMesh.generated.h"

class FPrimitiveDrawInterface;
class USkeletalMeshComponent;

// Editor node for IKRig 
UCLASS()
class IKRIGDEVELOPER_API UAnimGraphNode_RetargetPoseFromMesh : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_RetargetPoseFromMesh Node;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog);
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	virtual void PreloadRequiredAssets() override;
	virtual void CopyNodeDataToPreviewNode(FAnimNode_Base* AnimNode) override;
	virtual FEditorModeID GetEditorMode() const override;
	virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const override;
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
	virtual bool UsingCopyPoseFromMesh() const override { return true; };
	// End of UAnimGraphNode_Base interface

	// UK2Node interface
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	// End of UK2Node interface

	static const FName AnimModeName;
};
