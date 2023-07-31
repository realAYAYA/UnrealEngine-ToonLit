// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDeviceConsole.h"
#include "UObject/NameTypes.h"

/**
 * Windows implementation of console log window, utilizing the Win32 console API
 */
class APPLICATIONCORE_API FWindowsConsoleOutputDevice2 final : public FOutputDeviceConsole
{
private:
	uint16 TextAttribute;

	/** true if the color is currently set by the caller */
	bool OverrideColorSet;

	class FConsoleWindow;
	FConsoleWindow* Window;
	FRWLock WindowRWLock;

#if !UE_BUILD_SHIPPING
	/** An entry for log category highlighting */
	struct FLogHighlight
	{
		/** The category to highlight */
		FName Category;

		/** The 4 digit color code to highlight with */
		const TCHAR* Color = nullptr;

		bool operator == (FName InCategory) const
		{
			return Category == InCategory;
		}
	};

	/** Log categories to be highlighted */
	TArray<FLogHighlight, TInlineAllocator<8>> LogHighlights;

	/** An entry for log string highlighting */
	struct FLogStringHighlight
	{
		/** The string to search for and highlight */
		TStringBuilder<128> SearchString;

		/** The 4 digit color code to highlight with */
		const TCHAR* Color = nullptr;
	};

	/** Log strings to be highlighted */
	TArray<FLogStringHighlight, TInlineAllocator<8>> LogStringHighlights;
#endif

	/**
	 * Saves the console window's position and size to the game .ini
	 */
	void SaveToINI();

	static const FString& GetConfigFilename();

public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FWindowsConsoleOutputDevice2();
	~FWindowsConsoleOutputDevice2();

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (true) or hide (false) the console window.
	 */
	virtual void Show( bool ShowWindow );

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return true if console is shown, false otherwise
	 */
	virtual bool IsShown();

	virtual bool IsAttached();

	virtual bool CanBeUsedOnAnyThread() const override;

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time ) override;
	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

	void SetColor( const TCHAR* Color );

private:
	FWindowsConsoleOutputDevice2(const FWindowsConsoleOutputDevice2&) = delete;
	FWindowsConsoleOutputDevice2& operator=(const FWindowsConsoleOutputDevice2&) = delete;
};
