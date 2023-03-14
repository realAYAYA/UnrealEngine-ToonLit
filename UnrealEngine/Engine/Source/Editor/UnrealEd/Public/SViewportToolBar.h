// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Animation/CurveSequence.h"
#include "CoreMinimal.h"
#include "Editor/UnrealEdTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SMenuAnchor;
struct FSlateBrush;

/**
 * A level viewport toolbar widget that is placed in a viewport
 */
class UNREALED_API SViewportToolBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SViewportToolBar ){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/**
	 * @return The currently open pull down menu if there is one
	 */
	TWeakPtr<SMenuAnchor> GetOpenMenu() const;

	/**
	 * Sets the open menu to a new menu and closes any currently opened one
	 *
	 * @param NewMenu The new menu that is opened
	 */
	void SetOpenMenu( TSharedPtr< SMenuAnchor >& NewMenu );


	/** @return Whether the given viewmode is supported. */ 
	virtual bool IsViewModeSupported(EViewModeIndex ViewModeIndex) const;

protected:
	/**
	* Returns the label for the "Camera" tool bar menu based on based viewport type
	*
	* @param ViewportType	The Viewport type we want the label for
	*
	* @return	Label to use for this menu label
	*/
	virtual FText GetCameraMenuLabelFromViewportType(const ELevelViewportType ViewportType) const;

	/**
	* Returns the label icon for the "Camera" tool bar menu, which changes depending on the viewport type
	*
	* @param ViewportType	The Viewport type we want the icon for
	*
	* @return	Label icon to use for this menu label
	*/
	virtual const FSlateBrush* GetCameraMenuLabelIconFromViewportType(const ELevelViewportType ViewportType) const;

private:
	/** The pulldown menu that is open if any */
	TWeakPtr< SMenuAnchor > OpenedMenu;
	/** True if the mouse is inside the toolbar */
	bool bIsHovered;
};

