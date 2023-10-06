//  Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ToolElementRegistry.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

/** A class that provides the FToolbarRegistrationArgs for a UE toolbar  */
class WIDGETREGISTRATION_API FToolbarRegistrationArgs : public FToolElementRegistrationArgs
{
public:

	/** 
	 * the constructor for FToolbarRegistrationArgs
	 *
	 * @param InToolBarBuilder a TSharedRef to the FToolBarBuilder  which will be used by
	 * this to build up a toolbar SWidget
	 */
	FToolbarRegistrationArgs(TSharedRef<FToolBarBuilder> InToolBarBuilder);

	/**
	 * Generates the TSharedPtr<SWidget> for a toolbar
	 *
	 * @return the the TSharedPtr<SWidget> for a toolbar
	 */
	virtual TSharedPtr<SWidget> GenerateWidget() override;

	/** The FToolBarBuilder object used to build up a toolbar widget */
	TSharedRef<FToolBarBuilder> ToolBarBuilder;
	
};
