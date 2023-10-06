// Copyright Epic Games, Inc. All Rights Reserved.
/*=============================================================================================
	MacPlatformSplash.h: Mac platform splash screen...
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSplash.h"

/**
* Mac splash implementation 
**/
struct APPLICATIONCORE_API FMacPlatformSplash : public FGenericPlatformSplash
{
	/**
	* Show the splash screen
	*/
	static void Show();
	/**
	* Hide the splash screen
	*/
	static void Hide();

	/**
	 * Sets the text displayed on the splash screen (for startup/loading progress)
	 *
	 * @param	InType		Type of text to change
	 * @param	InText		Text to display
	 */
	static void SetSplashText( const SplashTextType::Type InType, const TCHAR* InText );

	/**
	 * Return whether the splash screen is being shown or not
	 */
	static bool IsShown();
};

typedef FMacPlatformSplash FPlatformSplash;

