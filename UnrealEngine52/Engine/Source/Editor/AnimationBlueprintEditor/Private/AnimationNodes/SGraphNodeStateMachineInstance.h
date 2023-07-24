// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "KismetNodes/SGraphNodeK2Composite.h"
#include "Math/Vector2D.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SPoseWatchOverlay;
class UEdGraph;
struct FOverlayWidgetInfo;

class SGraphNodeStateMachineInstance : public SGraphNodeK2Composite
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeStateMachineInstance){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UAnimGraphNode_StateMachineBase* InNode);

protected:
	// SGraphNodeK2Composite interface
	virtual UEdGraph* GetInnerGraph() const override;
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	virtual TSharedRef<SWidget> CreateNodeBody() override;
	// End of SGraphNodeK2Composite interface

private:
	TSharedPtr<SPoseWatchOverlay> PoseWatchWidget;
};
