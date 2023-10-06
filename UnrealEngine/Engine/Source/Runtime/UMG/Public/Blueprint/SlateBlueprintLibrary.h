// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"
#include "Layout/Geometry.h"
#include "Styling/SlateBrush.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlateBlueprintLibrary.generated.h"

UCLASS(meta=(ScriptName="SlateLibrary"), MinimalAPI)
class USlateBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 *
	 * @return true if the provided location in absolute coordinates is within the bounds of this geometry.
	 */
	 UFUNCTION(BlueprintPure, Category="User Interface|Geometry")
	static UMG_API bool IsUnderLocation(const FGeometry& Geometry, const FVector2D& AbsoluteCoordinate);

	/**
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 *
	 * @return Transforms AbsoluteCoordinate into the local space of this Geometry.
	 */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry")
	static UMG_API FVector2D AbsoluteToLocal(const FGeometry& Geometry, FVector2D AbsoluteCoordinate);

	/**
	 * Translates local coordinates into absolute coordinates
	 *
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 *
	 * @return  Absolute coordinates
	 */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry")
	static UMG_API FVector2D LocalToAbsolute(const FGeometry& Geometry, FVector2D LocalCoordinate);

	/** Returns the local top/left of the geometry in local space. */
	UFUNCTION(BlueprintPure, Category = "User Interface|Geometry")
	static UMG_API FVector2D GetLocalTopLeft(const FGeometry& Geometry);

	/** Returns the size of the geometry in local space. */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry")
	static UMG_API FVector2D GetLocalSize(const FGeometry& Geometry);

	/** Returns the size of the geometry in absolute space. */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry")
	static UMG_API FVector2D GetAbsoluteSize(const FGeometry& Geometry);

	/**  */
	UFUNCTION(BlueprintPure, Category = "User Interface|Geometry")
	static UMG_API float TransformScalarAbsoluteToLocal(const FGeometry& Geometry, float AbsoluteScalar);

	/**  */
	UFUNCTION(BlueprintPure, Category = "User Interface|Geometry")
	static UMG_API float TransformScalarLocalToAbsolute(const FGeometry& Geometry, float LocalScalar);

	/**  */
	UFUNCTION(BlueprintPure, Category = "User Interface|Geometry")
	static UMG_API FVector2D TransformVectorAbsoluteToLocal(const FGeometry& Geometry, FVector2D AbsoluteVector);

	/**  */
	UFUNCTION(BlueprintPure, Category = "User Interface|Geometry")
	static UMG_API FVector2D TransformVectorLocalToAbsolute(const FGeometry& Geometry, FVector2D LocalVector);

	/** Returns whether brushes A and B are identical. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (SlateBrush)", CompactNodeTitle = "=="), Category = "SlateBrush")
	static UMG_API bool EqualEqual_SlateBrush(const FSlateBrush& A, const FSlateBrush& B);

	/**
	 * Translates local coordinate of the geometry provided into local viewport coordinates.
	 *
	 * @param PixelPosition The position in the game's viewport, usable for line traces and 
	 * other uses where you need a coordinate in the space of viewport resolution units.
	 * @param ViewportPosition The position in the space of other widgets in the viewport.  Like if you wanted
	 * to add another widget to the viewport at the same position in viewport space as this location, this is
	 * what you would use.
	 */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry", meta=( WorldContext="WorldContextObject" ))
	static UMG_API void LocalToViewport(UObject* WorldContextObject, const FGeometry& Geometry, FVector2D LocalCoordinate, FVector2D& PixelPosition, FVector2D& ViewportPosition);

	/**
	 * Translates absolute coordinate in desktop space of the geometry provided into local viewport coordinates.
	 *
	 * @param PixelPosition The position in the game's viewport, usable for line traces and
	 * other uses where you need a coordinate in the space of viewport resolution units.
	 * @param ViewportPosition The position in the space of other widgets in the viewport.  Like if you wanted
	 * to add another widget to the viewport at the same position in viewport space as this location, this is
	 * what you would use.
	 */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry", meta=( WorldContext="WorldContextObject" ))
	static UMG_API void AbsoluteToViewport(UObject* WorldContextObject, FVector2D AbsoluteDesktopCoordinate, FVector2D& PixelPosition, FVector2D& ViewportPosition);

	/**
	 * Translates a screen position in pixels into the local space of a widget with the given geometry. 
	 * If bIncludeWindowPosition is true, then this method will also remove the game window's position (useful when in windowed mode).
	 */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry", meta=( WorldContext="WorldContextObject", DisplayName="ScreenToLocal" ))
	static UMG_API void ScreenToWidgetLocal(UObject* WorldContextObject, const FGeometry& Geometry, FVector2D ScreenPosition, FVector2D& LocalCoordinate, bool bIncludeWindowPosition = false);

	/**
	 * Translates a screen position in pixels into absolute application coordinates.
	 * If bIncludeWindowPosition is true, then this method will also remove the game window's position (useful when in windowed mode).
	 */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry", meta=( WorldContext="WorldContextObject", DisplayName="ScreenToAbsolute" ))
	static UMG_API void ScreenToWidgetAbsolute(UObject* WorldContextObject, FVector2D ScreenPosition, FVector2D& AbsoluteCoordinate, bool bIncludeWindowPosition = false);

	/**
	 * Translates a screen position in pixels into the local space of the viewport widget.
	 */
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry", meta=( WorldContext="WorldContextObject" ))
	static UMG_API void ScreenToViewport(UObject* WorldContextObject, FVector2D ScreenPosition, FVector2D& ViewportPosition);
};
