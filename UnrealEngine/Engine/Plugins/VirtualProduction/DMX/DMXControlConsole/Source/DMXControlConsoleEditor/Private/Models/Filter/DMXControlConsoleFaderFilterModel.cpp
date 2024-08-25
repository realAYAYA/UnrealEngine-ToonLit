// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderFilterModel.h"

#include "Algo/Find.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupFilterModel.h"
#include "DMXControlConsoleGlobalFilterModel.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"


namespace UE::DMX::Private
{
	FDMXControlConsoleFaderFilterModel::FDMXControlConsoleFaderFilterModel(UDMXControlConsoleFaderBase* InFader)
		: WeakFader(InFader)
	{}

	bool FDMXControlConsoleFaderFilterModel::MatchesAnyName(const TArray<FString>& Names) const
	{
		if (UDMXControlConsoleFaderBase* Fader = WeakFader.Get())
		{
			const FString* NamePtr = Algo::FindByPredicate(Names, [Fader](const FString& Name)
				{
					return Fader->GetFaderName().Contains(Name);
				});
			return NamePtr != nullptr;
		}

		return false;
	}

	bool FDMXControlConsoleFaderFilterModel::MatchesGlobalFilter(const FGlobalFilter& GlobalFilter, ENameFilterMode NameFilterMode)
	{
		if (GlobalFilter.String.IsEmpty())
		{
			return true;
		}

		UDMXControlConsoleFaderBase* Fader = WeakFader.Get();
		if (!Fader)
		{
			return false;
		}

		const UDMXEntityFixturePatch* FixturePatch = Fader->GetOwnerFaderGroupChecked().GetFixturePatch();
		auto MatchesUniverseAndAddressLambda = [GlobalFilter, Fader, FixturePatch, this]()
		{
			// True if the fader's universe id matches one of the filtered universes
			const bool bMatchesUniverse = Algo::FindByPredicate(GlobalFilter.Universes, [Fader](int32 Universe)
				{
					return Universe == Fader->GetUniverseID();
				}) != nullptr;

			// Test fader's starting channel only if set
			if (GlobalFilter.AbsoluteAddress.IsSet())
			{
				const bool bMatchesAddress = FixturePatch && GlobalFilter.AbsoluteAddress == FixturePatch->GetStartingChannel();
				return bMatchesUniverse && bMatchesAddress;
			}

			return bMatchesUniverse;
		};

		auto MatchesFixtureIDLambda = [FixturePatch, GlobalFilter, Fader, this]()
		{
			return
				Algo::FindByPredicate(GlobalFilter.FixtureIDs, [Fader, FixturePatch](int32 FixtureID)
					{
						int32 FixturePatchFixtureID;
						if (FixturePatch && FixturePatch->FindFixtureID(FixturePatchFixtureID))
						{
							return FixturePatchFixtureID == FixtureID;
						}
						return true;
					}) != nullptr;
		};

		const FString& FaderGroupName = Fader->GetOwnerFaderGroupChecked().GetFaderGroupName();
		auto MatchesFaderGroupNameLambda = [FaderGroupName, GlobalFilter]()
		{
			return
				Algo::FindByPredicate(GlobalFilter.Names, [FaderGroupName](const FString& Name)
					{
						return FaderGroupName.Contains(Name);
					}) != nullptr;
		};

		auto MatchesFaderNameLambda = [GlobalFilter, NameFilterMode, Fader, this]()
		{
			if (NameFilterMode == ENameFilterMode::MatchFaderNames || NameFilterMode == ENameFilterMode::MatchFaderAndFaderGroupNames)
			{
				return MatchesAnyName(GlobalFilter.Names);
			}
			else
			{
				return true;
			}
		};

		// True if owner Fader Group matches filter
		const bool bFaderGroupMatchesFilter =
			MatchesUniverseAndAddressLambda() ||
			MatchesFixtureIDLambda() ||
			MatchesFaderGroupNameLambda();

		switch (NameFilterMode)
		{
		case ENameFilterMode::MatchFaderGroupNames:
			return bFaderGroupMatchesFilter;
			break;
		case ENameFilterMode::MatchFaderNames:
			return MatchesFaderNameLambda();
			break;
		case ENameFilterMode::MatchFaderAndFaderGroupNames:
			return MatchesFaderNameLambda() && bFaderGroupMatchesFilter;
			break;
		default:
			return true;
			break;
		}
	}

	bool FDMXControlConsoleFaderFilterModel::MatchesFaderGroupFilter(const FFaderGroupFilter& FaderGroupFilter)
	{
		return 
			FaderGroupFilter.Names.IsEmpty() || 
			MatchesAnyName(FaderGroupFilter.Names);
	}

	UDMXControlConsoleFaderBase* FDMXControlConsoleFaderFilterModel::GetFader() const
	{
		return WeakFader.Get();
	}
}
