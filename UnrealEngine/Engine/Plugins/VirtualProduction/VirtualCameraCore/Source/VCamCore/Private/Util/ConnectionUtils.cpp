// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/ConnectionUtils.h"

#include "Modifier/VCamModifier.h"
#include "UI/VCamConnectionStructs.h"
#include "VCamComponent.h"

namespace UE::VCamCore::ConnectionUtils
{
	TArray<FModifierStackEntry> FindCompatibleModifiers(const FVCamConnection& Connection, UVCamComponent& Component)
	{
		TArray<FModifierStackEntry> Modifiers;
		Modifiers.Reserve(Component.GetNumberOfModifiers());
		ForEachCompatibleConnectionPoint(Connection, Component, [&Modifiers](const FModifierStackEntry& Modifier, FName ConnectionPoint)
		{
			Modifiers.Add(Modifier);
			// Minor optimisation: no need to iterate the other connection points since we care only whether at least one is valid
			return EBreakBehavior::SkipCurrentModifier;
		});
		return Modifiers;
	}

	TArray<FName> FindCompatibleConnectionPoints(const FVCamConnection& Connection, UVCamModifier& Modifier)
	{
		TArray<FName> ConnectionPoints;
		ConnectionPoints.Reserve(Modifier.ConnectionPoints.Num());
		ForEachCompatibleConnectionPoint(Connection, { FModifierStackEntry{ Modifier } }, [&ConnectionPoints](const FModifierStackEntry& Modifier, FName ConnectionPoint)
		{
			ConnectionPoints.Add(ConnectionPoint);
			return EBreakBehavior::Continue;
		});
		return ConnectionPoints;
	}
	
	void ForEachCompatibleConnectionPoint(const FVCamConnection& Connection, const TArray<FModifierStackEntry>& Modifiers, FCompatibleModifierCallback ProcessConnectionPoint)
	{
		constexpr bool bLogWarnings = false;
		for (const FModifierStackEntry& StackEntry : Modifiers)
		{
			// We allow disabled modifier at this point - users of ForEachCompatibleConnectionPoint can filter it themselves
			if (!StackEntry.GeneratedModifier) 
			{
				continue;
			}
			
			for (TPair<FName, FVCamModifierConnectionPoint>& ConnectionPoint : StackEntry.GeneratedModifier->ConnectionPoints)
			{
				if (!Connection.IsConnectionValid(*StackEntry.GeneratedModifier, ConnectionPoint.Key, bLogWarnings))
				{
					continue;
				}

				const EBreakBehavior BreakBehavior = ProcessConnectionPoint(StackEntry, ConnectionPoint.Key);
				if (BreakBehavior == EBreakBehavior::SkipCurrentModifier)
				{
					break;
				}
				if (BreakBehavior == EBreakBehavior::Break)
				{
					return;
				}
			}
		}
	}
}