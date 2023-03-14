// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UsdWrappers/ForwardDeclarations.h"

namespace UE
{
	class FUsdAttribute;
}

namespace UsdUtils
{
	/** Adds a "Muted" CustomData entry to the attribute at the stage's UE state sublayer, which will prevent it from being animated when loaded into UE */
	USDUTILITIES_API bool MuteAttribute( UE::FUsdAttribute& Attribute, const UE::FUsdStage& Stage );

	/** Removes the "Muted" CustomData entry from the attribute at the stage's UE state sublayer, letting it be animated when loaded into UE */
	USDUTILITIES_API bool UnmuteAttribute( UE::FUsdAttribute& Attribute, const UE::FUsdStage& Stage );

	/** Returns whether this attribute has the "Muted" CustomData on the stage's UE state sublayer, meaning it shouldn't be animated when loaded into UE */
	USDUTILITIES_API bool IsAttributeMuted( const UE::FUsdAttribute& Attribute, const UE::FUsdStage& Stage );

	/**
	 * If Attribute has opinions authored on layers stronger than the current edit target this will emit a warning to
	 * the user, indicating that the new value may not be visible on the USD Stage, and may cease to be visible on
	 * the UE level after a reload.
	 *
	 * Use this after setting any attribute: This function does nothing in case the Stage's current edit target has
	 * the strongest opinion for the attribute already
	 */
	USDUTILITIES_API void NotifyIfOverriddenOpinion( const UE::FUsdAttribute& Attribute );
}

