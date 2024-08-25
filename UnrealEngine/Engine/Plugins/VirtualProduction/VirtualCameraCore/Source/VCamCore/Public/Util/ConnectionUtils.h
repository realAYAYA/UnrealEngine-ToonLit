// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamComponent.h"

class UVCamModifier;
struct FVCamConnection;
struct FVCamModifierConnectionPoint;

namespace UE::VCamCore::ConnectionUtils
{
	/** Gets all modifiers that satisfy the connection's filter requirements and have at least one connection point available. */
	VCAMCORE_API TArray<FModifierStackEntry> FindCompatibleModifiers(const FVCamConnection& Connection, UVCamComponent& Component);

	FORCEINLINE TArray<FName> FindCompatibleModifierNames(const FVCamConnection& Connection, UVCamComponent& Component)
	{
		TArray<FName> Result;
		Algo::Transform(FindCompatibleModifiers(Connection, Component), Result, [](const FModifierStackEntry& Entry){ return Entry.Name; });
		return Result;
	}

	/** Gets all connection points that satisfy the connection's filter requirements. */
	VCAMCORE_API TArray<FName> FindCompatibleConnectionPoints(const FVCamConnection& Connection, UVCamModifier& Modifier);

	enum class EBreakBehavior
	{
		/** Continue with the next connection point or modifier */
		Continue,
		/** Continue with the next connection point on the next modifier  */
		SkipCurrentModifier,
		/** Stop */
		Break
	};
	using FCompatibleModifierCallback = TFunctionRef<EBreakBehavior(const FModifierStackEntry& Modifier, FName ConnectionPoint)>;

	VCAMCORE_API void ForEachCompatibleConnectionPoint(const FVCamConnection& Connection, const TArray<FModifierStackEntry>& Modifiers, FCompatibleModifierCallback ProcessConnectionPoint);
	
	FORCEINLINE void ForEachCompatibleConnectionPoint(const FVCamConnection& Connection, UVCamComponent& Component, FCompatibleModifierCallback ProcessConnectionPoint)
	{
		ForEachCompatibleConnectionPoint(Connection, Component.GetModifierStack(), ProcessConnectionPoint);
	}
}
