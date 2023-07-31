// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/MultiBox/MultiBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"


/**
 * Arbitrary Widget MultiBlock
 */
class FWidgetBlock
	: public FMultiBlock
{

public:

	/**
	 * Constructor
	 *
	 * @param	InContent	The widget to place in the block
	 * @param	InLabel		Optional label text to be added to the left of the content
	 * @param	bInNoIndent	If true, removes the padding from the left of the widget that lines it up with other menu items
	 */
	FWidgetBlock(TSharedRef<SWidget> InContent, const FText& InLabel, bool bInNoIndent, EHorizontalAlignment InHorizontalAlignment = HAlign_Fill);

	/** FMultiBlock interface */
	virtual void CreateMenuEntry(class FMenuBuilder& MenuBuilder) const override;

	/** Set optional delegate to customize when a menu appears instead of the widget, such as in toolbars */
	void SetCustomMenuDelegate( FNewMenuDelegate& InOnFillMenuDelegate);

private:

	/** FMultiBlock private interface */
	virtual TSharedRef< class IMultiBlockBaseWidget > ConstructWidget() const override;
	virtual bool GetAlignmentOverrides(EHorizontalAlignment& OutHorizontalAlignment, EVerticalAlignment& OutVerticalAlignment, bool& bOutAutoWidth) const;
private:

	// Friend our corresponding widget class
	friend class SWidgetBlock;

	/** Content widget */
	TSharedRef<SWidget> ContentWidget;

	/** Optional label text */
	FText Label;

	/** Remove the padding from the left of the widget that lines it up with other menu items? */
	bool bNoIndent;

	/** Hortizontal aligment for this widget in its parent container. Note: only applies to toolbars */
	EHorizontalAlignment HorizontalAlignment;

	/** Optional delegate to customize when a menu appears instead of the widget, such as in toolbars */
	FNewMenuDelegate CustomMenuDelegate;
};




/**
 * Arbitrary Widget MultiBlock widget
 */
class SLATE_API SWidgetBlock
	: public SMultiBlockBaseWidget
{

public:

	SLATE_BEGIN_ARGS( SWidgetBlock ){}
	SLATE_END_ARGS()


	/**
	 * Builds this MultiBlock widget up from the MultiBlock associated with it
	 */
	virtual void BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName) override;

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

protected:

	/**
	* Finds the STextBlock that gets displayed in the UI
	*
	* @param Content	Widget to check for an STextBlock
	* @return	The STextBlock widget found
	*/
	TSharedRef<SWidget> FindTextBlockWidget(TSharedRef<SWidget> Content);
};
