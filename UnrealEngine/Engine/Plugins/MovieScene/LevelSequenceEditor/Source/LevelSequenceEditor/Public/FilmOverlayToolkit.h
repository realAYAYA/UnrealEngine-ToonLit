// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FilmOverlayToolkit.generated.h"

struct IFilmOverlay;

/** Tool kit for film overlays */
UCLASS()
class LEVELSEQUENCEEDITOR_API UFilmOverlayToolkit : public UObject
{
public:

	GENERATED_BODY()

	/** Register a primary film overlay */
	static void RegisterPrimaryFilmOverlay(const FName& FilmOverlayName, TSharedPtr<IFilmOverlay> FilmOverlay);
	
	/** Unregister a primary film overlay */
	static void UnregisterPrimaryFilmOverlay(const FName& FilmOverlayName);

	/** Get the primary film overlays */
	static const TMap<FName, TSharedPtr<IFilmOverlay>>& GetPrimaryFilmOverlays();

	/** Register a toggleable film overlay */
	static void RegisterToggleableFilmOverlay(const FName& FilmOverlayName, TSharedPtr<IFilmOverlay> FilmOverlay);

	/** Unregister a toggleable film overlay */
	static void UnregisterToggleableFilmOverlay(const FName& FilmOverlayName);

	/** Get the toggleable film overlays */
	static const TMap<FName, TSharedPtr<IFilmOverlay>>& GetToggleableFilmOverlays();

private:

	/** The primary film overlays (only one can be active at a time) */
	static TMap<FName, TSharedPtr<IFilmOverlay>> PrimaryFilmOverlays;

	/** The toggleable film overlays (any number can be active at a time) */
	static TMap<FName, TSharedPtr<IFilmOverlay>> ToggleableFilmOverlays;
};
