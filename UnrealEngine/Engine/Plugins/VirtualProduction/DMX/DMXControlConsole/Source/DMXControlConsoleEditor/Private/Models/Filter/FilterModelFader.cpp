// Copyright Epic Games, Inc. All Rights Reserved.

#include "FilterModelFader.h"

#include "Algo/Find.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "FilterModel.h"
#include "FilterModelFaderGroup.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"


namespace UE::DMXControlConsoleEditor::FilterModel::Private
{
	FFilterModelFader::FFilterModelFader(UDMXControlConsoleFaderBase* InFader)
		: WeakFader(InFader)
	{}

	bool FFilterModelFader::MatchesAnyName(const TArray<FString>& Names) const
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

	bool FFilterModelFader::MatchesGlobalFilter(const FGlobalFilter& GlobalFilter, ENameFilterMode NameFilterMode)
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

		auto MatchesUniverseLambda = [GlobalFilter, Fader, this]()
		{
			return
				Algo::FindByPredicate(GlobalFilter.Universes, [Fader](int32 Universe)
					{
						return Universe == Fader->GetUniverseID();
					}) != nullptr;
		};

		const UDMXEntityFixturePatch* FixturePatch = Fader->GetOwnerFaderGroupChecked().GetFixturePatch();
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

		auto MatchesAddressLambda = [FixturePatch, GlobalFilter]()
		{
			return
				FixturePatch &&
				GlobalFilter.AbsoluteAddress.IsSet() &&
				GlobalFilter.AbsoluteAddress == FixturePatch->GetStartingChannel();
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
			MatchesUniverseLambda() ||
			MatchesAddressLambda() ||
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

	bool FFilterModelFader::MatchesFaderGroupFilter(const FFaderGroupFilter& FaderGroupFilter)
	{
		return 
			FaderGroupFilter.Names.IsEmpty() || 
			MatchesAnyName(FaderGroupFilter.Names);
	}

	UDMXControlConsoleFaderBase* FFilterModelFader::GetFader() const
	{
		return WeakFader.Get();
	}
}
