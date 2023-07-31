// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "RigVMModel/RigVMPin.h"
#include "ControlRigBlueprint.h"

class UControlRigGraphNode;
class STableViewBase;
class SOverlay;
class SGraphPin;
class UEdGraphPin;

class SControlRigGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SControlRigGraphNode)
		: _GraphNodeObj(nullptr)
		{}

	SLATE_ARGUMENT(UControlRigGraphNode*, GraphNodeObj)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SGraphNode interface
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> InNodeTitle) override;

	virtual void EndUserInteraction() const override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	virtual void AddPin( const TSharedRef<SGraphPin>& PinToAdd ) override;
	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override
	{
		TitleAreaWidget = DefaultTitleAreaWidget;
	}
	virtual const FSlateBrush * GetNodeBodyBrush() const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;

	virtual void RefreshErrorInfo() override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool IsHidingPinWidgets() const override { return UseLowDetailNodeContent(); }
	virtual bool UseLowDetailPinNames() const override;

	void CreateAggregateAddPinButton();

	/** Callback function executed when Add pin button is clicked */
	virtual FReply OnAddPin() override;

private:

	bool UseLowDetailNodeContent() const;

	FVector2D GetLowDetailDesiredSize() const;

	EVisibility GetTitleVisibility() const;
	EVisibility GetArrayPlusButtonVisibility(URigVMPin* InModelPin) const;

	FText GetPinLabel(TWeakPtr<SGraphPin> GraphPin) const;

	FSlateColor GetPinTextColor(TWeakPtr<SGraphPin> GraphPin) const;
	FSlateColor GetVariableLabelTextColor(TWeakObjectPtr<URigVMFunctionReferenceNode> FunctionReferenceNode, FName InVariableName) const;
	FText GetVariableLabelTooltipText(TWeakObjectPtr<UControlRigBlueprint> InBlueprint, FName InVariableName) const;

	FReply HandleAddArrayElement(FString InModelPinPath);

	void HandleNodeTitleDirtied();

	FText GetInstructionCountText() const;
	FText GetInstructionDurationText() const;

private:

	int32 GetNodeTopologyVersion() const;
	EVisibility GetPinVisibility(int32 InPinInfoIndex) const;
	const FSlateBrush * GetExpanderImage(int32 InPinInfoIndex, bool bLeft, bool bHovered) const;
	FReply OnExpanderArrowClicked(int32 InPinInfoIndex);
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	/** Cached widget title area */
	TSharedPtr<SOverlay> TitleAreaWidget;

	int32 NodeErrorType;

	TSharedPtr<SImage> VisualDebugIndicatorWidget;
	TSharedPtr<STextBlock> InstructionCountTextBlockWidget;
	TSharedPtr<STextBlock> InstructionDurationTextBlockWidget;

	static const FSlateBrush* CachedImg_CR_Pin_Connected;
	static const FSlateBrush* CachedImg_CR_Pin_Disconnected;

	/** Cache the node title so we can invalidate it */
	TSharedPtr<SNodeTitle> NodeTitle;

	TWeakObjectPtr<UControlRigBlueprint> Blueprint;

	FVector2D LastHighDetailSize;

	struct FPinInfo
	{
		int32 Index;
		int32 ParentIndex;
		bool bHasChildren;
		bool bHideInputWidget;
		bool bIsContainer;
		int32 Depth;
		FString ModelPinPath;
		TSharedPtr<SGraphPin> InputPinWidget;
		TSharedPtr<SGraphPin> OutputPinWidget;
		bool bExpanded;
		bool bAutoHeight;
	};
	
	TArray<FPinInfo> PinInfos;
	TWeakObjectPtr<URigVMNode> ModelNode;
};
