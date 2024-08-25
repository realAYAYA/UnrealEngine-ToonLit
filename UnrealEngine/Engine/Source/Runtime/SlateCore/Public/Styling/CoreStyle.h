// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Styling/AppStyle.h"

struct FSlateDynamicImageBrush;

/**
 * Core slate style
 */
class FCoreStyle 
{
public:

	static SLATECORE_API TSharedRef<class ISlateStyle> Create( const FName& InStyleSetName = "CoreStyle" );

	/** 
	* @return the Application Style 
	*
	* NOTE: Until the Editor can be fully updated, calling FCoreStyle::Get() will
	* return the AppStyle instead of the style definied in this class.  
	*
	* Using the AppStyle is preferred in most cases as it allows the style to be changed 
	* and restyled more easily.
	*
	* In cases requiring explicit use of the CoreStyle where a Slate Widget should not take on
	* the appearance of the rest of the application, use FCoreStyle::GetCoreStyle().
	*
	*/
	static const ISlateStyle& Get( )
	{
		return FAppStyle::Get();
	}

	/** @return the singleton instance of the style created in . */
	static const ISlateStyle& GetCoreStyle()
	{
		return *(Instance.Get());
	}

	/** Get the default font for Slate */
	static SLATECORE_API TSharedRef<const FCompositeFont> GetDefaultFont();

	/** Get a font style using the default for for Slate */
	static SLATECORE_API FSlateFontInfo GetDefaultFontStyle(const FName InTypefaceFontName, const float InSize, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());

	static SLATECORE_API void ResetToDefault( );

	/** Used to override the default selection colors */
	static SLATECORE_API void SetSelectorColor( const FLinearColor& NewColor );
	static SLATECORE_API void SetSelectionColor( const FLinearColor& NewColor );
	static SLATECORE_API void SetInactiveSelectionColor( const FLinearColor& NewColor );
	static SLATECORE_API void SetPressedSelectionColor( const FLinearColor& NewColor );
	static SLATECORE_API void SetFocusBrush(FSlateBrush* NewBrush);

	// todo: jdale - These are only here because of UTouchInterface::Activate and the fact that GetDynamicImageBrush is non-const
	static SLATECORE_API const TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush( FName BrushTemplate, FName TextureName, const ANSICHAR* Specifier = nullptr );
	static SLATECORE_API const TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush( FName BrushTemplate, const ANSICHAR* Specifier, class UTexture2D* TextureResource, FName TextureName );
	static SLATECORE_API const TSharedPtr<FSlateDynamicImageBrush> GetDynamicImageBrush( FName BrushTemplate, class UTexture2D* TextureResource, FName TextureName );

	static const int32 RegularTextSize = 9;
	static const int32 SmallTextSize = 8;

	static SLATECORE_API bool IsStarshipStyle();

	static bool IsInitialized() { return Instance.IsValid(); }

private:

	static SLATECORE_API void SetStyle( const TSharedRef< class ISlateStyle >& NewStyle );

private:

	/** Singleton instances of this style. */
	static SLATECORE_API TSharedPtr< class ISlateStyle > Instance;
};

namespace CoreStyleConstants
{
	// Note, these sizes are in Slate Units.
	// Slate Units do NOT have to map to pixels.
	inline const UE::Slate::FDeprecateVector2DResult Icon5x16(5.0f, 16.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon6x8(6.0f, 8.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon8x4(8.0f, 4.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon16x4(16.0f, 4.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon8x8(8.0f, 8.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon10x10(10.0f, 10.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon12x12(12.0f, 12.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon12x16(12.0f, 16.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon14x14(14.0f, 14.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon16x16(16.0f, 16.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon18x18(18.0f, 18.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon20x20(20.0f, 20.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon22x22(22.0f, 22.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon24x24(24.0f, 24.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon25x25(25.0f, 25.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon26x26(26.0f, 26.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon32x32(32.0f, 32.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon40x40(40.0f, 40.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon64x64(64.0f, 64.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon36x24(36.0f, 24.0f);
	inline const UE::Slate::FDeprecateVector2DResult Icon128x128(128.0f, 128.0f);

	// Common Margins
	inline const FMargin DefaultMargins(8.f, 4.f);
	// Buttons already have a built in (4., 2.) padding - adding to that a little
	inline const FMargin ButtonMargins(12.f, 1.5f, 12.f, 1.5f);

	inline const FMargin PressedButtonMargins(12.f, 2.5f, 12.f, 0.5f);
	inline const FMargin ToggleButtonMargins(16.0f, 2.0f);
	inline const FMargin ComboButtonMargin(8.f, 1.f, 8.f, 1.f);
	inline const FMargin PressedComboButtonMargin(8.f, 2.f, 8.f, 0.f);

	inline const float InputFocusRadius = 4.f;
	inline const float InputFocusThickness = 1.0f;
}
