// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/SlateRect.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "SGraphNodeResizable.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SWidget;
class UEdGraphNode;
struct FGeometry;
struct FPointerEvent;

class GRAPHEDITOR_API SGraphNodeDocumentation : public SGraphNodeResizable
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeDocumentation){}
	SLATE_END_ARGS()

	//~ Begin SPanel Interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	//~ End SPanel Interface

	//~ Begin SWidget Interface
	void Construct( const FArguments& InArgs, UEdGraphNode* InNode );
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	//~ End SWidget Interface

	//~ Begin SGraphNodeResizable Interface
	virtual FVector2D GetNodeMinimumSize() const override;
	virtual FVector2D GetNodeMaximumSize() const override;
	virtual float GetTitleBarHeight() const override;
	virtual FSlateRect GetHitTestingBorder() const override;
	//~ End SGraphNodeResizable Interface

protected:

	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;
	virtual bool IsNameReadOnly () const override { return false; }
	//~ End SGraphNode Interface

	/** Retrives the current documentation title based on the chosen excerpt */
	FText GetDocumentationTitle() const;
	/** Create documentation page from link and excerpt */
	TSharedPtr<SWidget> CreateDocumentationPage();
	/** Returns the width the documentation content must adhere to, used as a delegate in child widgets */
	FOptionalSize GetContentWidth() const;
	/** Returns the height the documentation content must adhere to, used as a delegate in child widgets */
	FOptionalSize GetContentHeight() const;
	/** Returns the child widget text wrapat size, used as a delegate during creation of documentation page */
	float GetDocumentationWrapWidth() const;
	/** Returns the current child widgets visibility for hit testing */
	EVisibility GetWidgetVisibility() const;
	/** Color of the page gradient start */
	FLinearColor GetPageGradientStartColor() const;
	/** Color of the page gradient end */
	FLinearColor GetPageGradientEndColor() const;

private:

	/** Node Title Bar */
	TSharedPtr<SWidget> TitleBar;
	/** Documentation excerpt maximum page size */
	FVector2D DocumentationSize;
	/** Content Widget with the desired size being managed */
	TSharedPtr<SWidget> ContentWidget;
	/** Tracks if child widgets are availble to hit test against */
	EVisibility ChildWidgetVisibility;
	/** Cached Documentation Link */
	FString CachedDocumentationLink;
	/** Cached Documentation Excerpt */
	FString CachedDocumentationExcerpt;

};
