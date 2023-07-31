// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "KismetNodes/SGraphNodeK2Base.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

class SToolTip;
class SWidget;
class UEdGraph;
class UK2Node_Composite;

class GRAPHEDITOR_API SGraphNodeK2Composite : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Composite){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_Composite* InNode);

	// SGraphNode interface
	 virtual void UpdateGraphNode() override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	// End of SGraphNode interface
protected:
	virtual UEdGraph* GetInnerGraph() const;

	FText GetPreviewCornerText() const;
	FText GetTooltipTextForNode() const;

	virtual TSharedRef<SWidget> CreateNodeBody();
};
