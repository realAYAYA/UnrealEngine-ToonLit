// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if SOURCE_CONTROL_WITH_SLATE

#include "Styling/SlateStyle.h"

/**
 * The style manager that is used to access the currently active revision control style.
 * Use FRevisionControlStyleManager::Get() to access and use any revision control icons/styles
 */
class SOURCECONTROL_API FRevisionControlStyleManager
{
public:

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The current revision control style being used */
	static const ISlateStyle& Get();

	/** @return The name of the current revision control style being used */
	static FName GetStyleSetName();

	/** Set the active revision control style to the input style name */
	static void SetActiveRevisionControlStyle(FName InNewActiveRevisionControlStyleName);

	/** Set the active revision control style to the default style */
	static void ResetToDefaultRevisionControlStyle();

private:

	// The default revision control style instance
	static TSharedPtr< class ISlateStyle > DefaultRevisionControlStyleInstance;

	// The currently active revision control style
	static FName CurrentRevisionControlStyleName;
};

/**
 * The default revision control style the editor ships with. Inherit from this to create a custom revision controls style
 * Use FRevisionControlStyleManager::SetActiveRevisionControlStyle to change the currently active revision control style
 * Edit the defaults in the constructor here to change any revision control icons in the editor
 */
class FDefaultRevisionControlStyle : public FSlateStyleSet
{
public:
	
	FDefaultRevisionControlStyle();
	virtual ~FDefaultRevisionControlStyle() override;
	
	virtual const FName& GetStyleSetName() const override;

protected:

	/** The specific color we use for all the "Branched" icons */
	FLinearColor BranchedColor;
	
private:
	static FName StyleName;
};

#endif //SOURCE_CONTROL_WITH_SLATE
