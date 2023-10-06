// Copyright Epic Games, Inc. All Rights Reserved.

#include "FilterModelFaderGroup.h"

#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "FilterModel.h"
#include "FilterModelFader.h"
#include "Library/DMXEntityFixturePatch.h"


namespace UE::DMXControlConsoleEditor::FilterModel::Private
{
	void FFaderGroupFilter::Parse(const FString& InString)
	{
		Reset();

		String = InString.TrimStartAndEnd();
		Names = ParseStringIntoArray(InString);
	}

	void FFaderGroupFilter::Reset()
	{
		String.Reset();
		Names.Reset();
	}

	FFilterModelFaderGroup::FFilterModelFaderGroup(UDMXControlConsoleFaderGroup* InFaderGroup)
		: WeakFaderGroup(InFaderGroup)
	{
		if (WeakFaderGroup.IsValid())
		{
			SetFilter(WeakFaderGroup->FilterString);
			UpdateFaderModels();
		}
	}

	UDMXControlConsoleFaderGroup* FFilterModelFaderGroup::GetFaderGroup() const
	{
		return WeakFaderGroup.Get();
	}

	void FFilterModelFaderGroup::SetFilter(const FString& InString)
	{
		UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (FaderGroup && FaderGroupFilter.String != InString)
		{
			FaderGroupFilter.Parse(InString);

			FaderGroup->Modify();
			FaderGroup->FilterString = FaderGroupFilter.String;
		}
	}

	bool FFilterModelFaderGroup::MatchesGlobalFilterUniverses(const FGlobalFilter& GlobalFilter) const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (!FaderGroup || GlobalFilter.Universes.IsEmpty())
		{
			return false;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			return false;
		}

		const int32* MatchingUniversePtr = Algo::FindByPredicate(GlobalFilter.Universes, [FixturePatch](const int32 Universe)
				{
					return FixturePatch->GetUniverseID() == Universe;
				});

		return MatchingUniversePtr != nullptr;
	}

	bool FFilterModelFaderGroup::MatchesGlobalFilterAbsoluteAddress(const FGlobalFilter& GlobalFilter) const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (!FaderGroup || !GlobalFilter.AbsoluteAddress.IsSet())
		{
			return false;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			return false;
		}

		return GlobalFilter.AbsoluteAddress.IsSet() && GlobalFilter.AbsoluteAddress == FixturePatch->GetStartingChannel();
	}

	bool FFilterModelFaderGroup::MatchesGlobalFilterNames(const FGlobalFilter& GlobalFilter) const
	{
		if (const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get())
		{
			const FString* MatchingStringPtr = Algo::FindByPredicate(GlobalFilter.Names, [FaderGroup](const FString& Name)
				{
					return FaderGroup->GetFaderGroupName().Contains(Name);
				});

			return MatchingStringPtr != nullptr;
		}
		return false;
	}

	bool FFilterModelFaderGroup::MatchesGlobalFilterFixtureIDs(const FGlobalFilter& GlobalFilter) const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (!FaderGroup )
		{
			return false;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			return false;
		}

		int32 FixtureID;
		if (FixturePatch->FindFixtureID(FixtureID))
		{
			const int32* MatchingIDPtr = Algo::FindByPredicate(GlobalFilter.FixtureIDs, [FixtureID](const int32 InFixtureID)
				{
					return FixtureID == InFixtureID;
				});

			return MatchingIDPtr != nullptr;
		}

		return false;
	}

	bool FFilterModelFaderGroup::HasFadersMatchingGlobalFilterNames(const FGlobalFilter& GlobalFilter) const
	{
		const TSharedRef<FFilterModelFader>* MatchingFaderModelPtr = Algo::FindByPredicate(FaderModels,
			[GlobalFilter](const TSharedRef<FFilterModelFader>& FaderModel)
			{
				return FaderModel->MatchesAnyName(GlobalFilter.Names);
			});

		return MatchingFaderModelPtr != nullptr;
	}

	void FFilterModelFaderGroup::Apply(const FGlobalFilter& GlobalFilter, ENameFilterMode NameFilterMode)
	{
		UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (!FaderGroup)
		{
			return;
		}

		// If no global filter, only apply fader group filter
		if (GlobalFilter.String.IsEmpty())
		{
			FaderGroup->SetIsMatchingFilter(true);
			for (const TSharedRef<FFilterModelFader>& FaderModel : FaderModels)
			{
				UDMXControlConsoleFaderBase* Fader = FaderModel->GetFader();
				if (!Fader)
				{
					continue;
				}

				const bool bMatchesFaderGroupFilter = FaderModel->MatchesFaderGroupFilter(FaderGroupFilter);
				Fader->SetIsMatchingFilter(bMatchesFaderGroupFilter);
			}

			return;
		}

		// Reset visibility
		FaderGroup->SetIsMatchingFilter(false);
		const TArray<UDMXControlConsoleFaderBase*> Faders = FaderGroup->GetAllFaders();
		for (UDMXControlConsoleFaderBase* Fader : Faders)
		{
			Fader->SetIsMatchingFilter(false);
		}

		// Apply filter
		const bool bRequiresMatchingFaderGroupName = NameFilterMode == ENameFilterMode::MatchFaderGroupNames || NameFilterMode == ENameFilterMode::MatchFaderAndFaderGroupNames;
		if (bRequiresMatchingFaderGroupName)
		{
			if (!IsMatchingGlobalFilterUniverses(GlobalFilter) &&
				!IsMatchingGlobalFilterAbsoluteAddress(GlobalFilter) &&
				!IsMatchingGlobalFilterNames(GlobalFilter) &&
				!IsMatchingGlobalFilterFixtureIDs(GlobalFilter))
			{
				return;
			}

			FaderGroup->SetIsMatchingFilter(true);
		}

		for (const TSharedRef<FFilterModelFader>& FaderModel : FaderModels)
		{
			UDMXControlConsoleFaderBase* Fader = FaderModel->GetFader();
			if (!Fader)
			{
				continue;
			}

			const bool bMatchesFilters = [NameFilterMode, GlobalFilter, FaderModel, this]() 
				{
					if (NameFilterMode == ENameFilterMode::MatchFaderGroupNames)
					{
						// True if matches global filter and fader group filter
						return
							FaderModel->MatchesGlobalFilter(GlobalFilter, NameFilterMode) &&
							FaderModel->MatchesFaderGroupFilter(FaderGroupFilter);
					}
					else
					{
						// True if matches global filter or fader group filter (or both)
						return
							FaderModel->MatchesGlobalFilter(GlobalFilter, NameFilterMode) ||
							(FaderModel->MatchesFaderGroupFilter(FaderGroupFilter) &&
								!FaderGroupFilter.Names.IsEmpty());
					}
				}();

			if (bMatchesFilters)
			{
				FaderGroup->SetIsMatchingFilter(true);
				Fader->SetIsMatchingFilter(true);
			}
		}
	}

	void FFilterModelFaderGroup::UpdateFaderModels()
	{
		FaderModels.Reset();

		const TArray<UDMXControlConsoleFaderBase*> Faders = WeakFaderGroup.IsValid() ? WeakFaderGroup->GetAllFaders() : TArray<UDMXControlConsoleFaderBase*>{};
		Algo::Transform(Faders, FaderModels, [](UDMXControlConsoleFaderBase* Fader)
			{
				return MakeShared<FFilterModelFader>(Fader);
			});
	}

	bool FFilterModelFaderGroup::IsMatchingGlobalFilterUniverses(const FGlobalFilter& GlobalFilter) const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (!FaderGroup || GlobalFilter.Universes.IsEmpty())
		{
			return false;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			return false;
		}

		const bool bMatchesFaderGroupUniverse =
			Algo::FindByPredicate(GlobalFilter.Universes, [FixturePatch](const int32 Universe)
				{
					return FixturePatch->GetUniverseID() == Universe;
				}) != nullptr;

		return bMatchesFaderGroupUniverse;
	}

	bool FFilterModelFaderGroup::IsMatchingGlobalFilterAbsoluteAddress(const FGlobalFilter& GlobalFilter) const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (!FaderGroup || !GlobalFilter.AbsoluteAddress.IsSet())
		{
			return false;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			return false;
		}

		const bool bMatchesFaderGroupAbsoluteAddress = GlobalFilter.AbsoluteAddress == FixturePatch->GetStartingChannel();
		return bMatchesFaderGroupAbsoluteAddress;
	}

	bool FFilterModelFaderGroup::IsMatchingGlobalFilterNames(const FGlobalFilter& GlobalFilter) const
	{
		if (const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get())
		{
			const bool bMatchesFaderGroupName =
				Algo::FindByPredicate(GlobalFilter.Names, [FaderGroup](const FString& Name)
					{
						return FaderGroup->GetFaderGroupName().Contains(Name);
					}) != nullptr;
			return bMatchesFaderGroupName;
		}

		return false;
	}

	bool FFilterModelFaderGroup::IsMatchingGlobalFilterFixtureIDs(const FGlobalFilter& GlobalFilter) const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = WeakFaderGroup.Get();
		if (!FaderGroup || GlobalFilter.FixtureIDs.IsEmpty())
		{
			return false;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			return false;
		}

		int32 FixtureID;
		if (FixturePatch->FindFixtureID(FixtureID))
		{
			const bool bMatchesFaderGroupFixtureIDs =
				Algo::FindByPredicate(GlobalFilter.FixtureIDs, [FixtureID](const int32 InFixtureID)
					{
						return FixtureID == InFixtureID;
					}) != nullptr;

			return bMatchesFaderGroupFixtureIDs;
		}

		return false;
	}
}
