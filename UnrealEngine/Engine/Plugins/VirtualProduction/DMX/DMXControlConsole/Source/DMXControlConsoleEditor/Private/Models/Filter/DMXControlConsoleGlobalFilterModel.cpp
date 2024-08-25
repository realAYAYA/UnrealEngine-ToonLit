// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleGlobalFilterModel.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupFilterModel.h"
#include "DMXEditorUtils.h"
#include "Internationalization/Regex.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Misc/TransactionObjectEvent.h"
#include "Models/DMXControlConsoleEditorModel.h"


namespace UE::DMX::Private
{
	TArray<FString> ParseStringIntoArray(const FString& InString)
	{
		constexpr int32 NumDelimiters = 4;
		static const TCHAR* AttributeNameParamDelimiters[] =
		{
			TEXT("."),
			TEXT(","),
			TEXT(":"),
			TEXT(";")
		};

		TArray<FString> Substrings;
		constexpr bool bCullEmpty = false;
		InString.ParseIntoArray(Substrings, AttributeNameParamDelimiters, NumDelimiters, bCullEmpty);

		// Cull whitespace and empty
		TArray<FString> Result;
		for (FString& Substring : Substrings)
		{
			Substring.TrimStartAndEndInline();
			if (!Substring.IsEmpty())
			{
				Result.Add(Substring);
			}
		}

		return Result;
	}

	void FGlobalFilter::Parse(const FString& InString)
	{
		Reset();

		String = InString.TrimStartAndEnd();

		// Parse universes
		const FRegexPattern UniversePattern(TEXT("(universe|uni\\s*[0-9, -]*)"));
		FRegexMatcher UniverseRegex(UniversePattern, String);
		FString UniverseSubstring;
		if (UniverseRegex.FindNext())
		{
			UniverseSubstring = UniverseRegex.GetCaptureGroup(1);
			Universes.Append(FDMXEditorUtils::ParseUniverses(UniverseSubstring));
		}

		const FRegexPattern UniverseWithAddressPattern(TEXT("(\\d+\\.)*"));
		FRegexMatcher UniverseWithAddressRegex(UniverseWithAddressPattern, String);
		FString UniverseWithAddressSubstring;
		if (UniverseWithAddressRegex.FindNext())
		{
			UniverseWithAddressSubstring = UniverseWithAddressRegex.GetCaptureGroup(0);
			Universes.Append(FDMXEditorUtils::ParseUniverses(UniverseWithAddressSubstring));
		}

		// Parse address. Ignore universes.
		const FString StringWithoutUniverses = String.Replace(*UniverseSubstring, TEXT(""));
		int32 Address = -1;
		if (FDMXEditorUtils::ParseAddress(StringWithoutUniverses, Address))
		{
			AbsoluteAddress = Address;
		}

		// Parse fixture IDs. Ignore universes and address.
		const FString AddressAsString = FString::FromInt(Address);
		const FString StringWithoutUniversesWithAddress = StringWithoutUniverses.Replace(*UniverseWithAddressSubstring, TEXT(""));
		const FString StringWithoutUniversesAndAddress = StringWithoutUniversesWithAddress.Replace(*AddressAsString, TEXT(""));

		FixtureIDs = FDMXEditorUtils::ParseFixtureIDs(StringWithoutUniversesAndAddress);

		// Parse names. Ignore universes.
		Names = ParseStringIntoArray(StringWithoutUniversesAndAddress);
	}

	void FGlobalFilter::Reset()
	{
		String.Reset();
		Universes.Reset();
		FixtureIDs.Reset();
		AbsoluteAddress.Reset();
		Names.Reset();
	}

	TOptional<int32> FGlobalFilter::GetUniverse() const
	{
		if (AbsoluteAddress.IsSet())
		{
			return AbsoluteAddress.GetValue() / DMX_UNIVERSE_SIZE;
		}
		return TOptional<int32>();
	}

	TOptional<int32> FGlobalFilter::GetChannel() const
	{
		if (AbsoluteAddress.IsSet())
		{
			return AbsoluteAddress.GetValue() % DMX_UNIVERSE_SIZE;
		}
		return TOptional<int32>();
	}

	FDMXControlConsoleGlobalFilterModel::FDMXControlConsoleGlobalFilterModel(UDMXControlConsoleEditorModel* InEditorModel)
		: EditorModel(InEditorModel)
	{
	}

	void FDMXControlConsoleGlobalFilterModel::Initialize()
	{
		InitializeInternal();
	}

	void FDMXControlConsoleGlobalFilterModel::SetGlobalFilter(const FString& NewFilter)
	{
		UDMXControlConsoleData* ControlConsoleData = WeakControlConsoleData.Get();
		if (ControlConsoleData)
		{
			GlobalFilter.Parse(NewFilter);
			UpdateNameFilterMode();

			ApplyFilter();

			ControlConsoleData->Modify();
			ControlConsoleData->FilterString = NewFilter;

			OnFilterChanged.Broadcast();
		}
	}

	void FDMXControlConsoleGlobalFilterModel::SetFaderGroupFilter(UDMXControlConsoleFaderGroup* FaderGroup, const FString& InString)
	{
		const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>* FaderGroupModelPtr = Algo::FindBy(FaderGroupModels, FaderGroup,
			[](const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>& Model)
			{
				return Model->GetFaderGroup();
			});
		if (FaderGroupModelPtr)
		{
			(*FaderGroupModelPtr)->SetFilter(InString);
			(*FaderGroupModelPtr)->Apply(GlobalFilter, NameFilterMode);

			OnFilterChanged.Broadcast();
		}
	}

	bool FDMXControlConsoleGlobalFilterModel::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		const TArray<UClass*> MatchingClasses = { UDMXControlConsoleData::StaticClass(), UDMXControlConsoleFaderGroup::StaticClass(), UDMXControlConsoleFaderBase::StaticClass() };

		const bool bMatchesContext = Algo::AnyOf(TransactionObjectContexts, [MatchingClasses](const TPair<UObject*, FTransactionObjectEvent>& Pair)
			{
				bool bMatchesClasses = false;
				const UObject* Object = Pair.Key;
				if (IsValid(Object))
				{
					const UClass* ObjectClass = Object->GetClass();
					if (IsValid(ObjectClass))
					{
						bMatchesClasses = Algo::AnyOf(MatchingClasses, [ObjectClass](UClass* InClass)
							{
								return ObjectClass->IsChildOf(InClass);
							});
					}
				}

				return bMatchesClasses;
			});
		
		return bMatchesContext;
	}

	void FDMXControlConsoleGlobalFilterModel::PostUndo(bool bSuccess)
	{
		if (WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();
			UpdateNameFilterMode();
			ApplyFilter();
		}
	}

	void FDMXControlConsoleGlobalFilterModel::PostRedo(bool bSuccess)
	{
		if (WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();
			UpdateNameFilterMode();
			ApplyFilter();
		}
	}

	void FDMXControlConsoleGlobalFilterModel::UpdateNameFilterMode()
	{
		bool bGroupMatchesGlobalFilterNames = false;
		bool bHasFadersMatchingGlobalFilterNames = false;
		for (const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>& FaderGroupModel : FaderGroupModels)
		{
			bGroupMatchesGlobalFilterNames |= 
				FaderGroupModel->MatchesGlobalFilterUniverseAndAddress(GlobalFilter) ||
				FaderGroupModel->MatchesGlobalFilterNames(GlobalFilter) ||
				FaderGroupModel->MatchesGlobalFilterFixtureIDs(GlobalFilter);

			bHasFadersMatchingGlobalFilterNames |= FaderGroupModel->HasFadersMatchingGlobalFilterNames(GlobalFilter);

			if (bGroupMatchesGlobalFilterNames && bHasFadersMatchingGlobalFilterNames)
			{
				break;
			}
		}

		if (bGroupMatchesGlobalFilterNames && bHasFadersMatchingGlobalFilterNames)
		{
			NameFilterMode = ENameFilterMode::MatchFaderAndFaderGroupNames;
		}
		else if (bHasFadersMatchingGlobalFilterNames)
		{
			NameFilterMode = ENameFilterMode::MatchFaderNames;
		}
		else 
		{
			NameFilterMode = ENameFilterMode::MatchFaderGroupNames;
		}
	}

	void FDMXControlConsoleGlobalFilterModel::InitializeInternal()
	{
		UpdateControlConsoleData();

		if (WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();

			GlobalFilter.Parse(WeakControlConsoleData->FilterString);
			ApplyFilter();
		}
	}

	void FDMXControlConsoleGlobalFilterModel::UpdateFaderGroupModels()
	{
		Algo::ForEach(FaderGroupModels, [this](const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>& Model)
			{
				if (UDMXControlConsoleFaderGroup* FaderGroup = Model->GetFaderGroup())
				{
					FaderGroup->GetOnElementAdded().RemoveAll(this);
					FaderGroup->GetOnElementRemoved().RemoveAll(this);
					FaderGroup->GetOnFixturePatchChanged().RemoveAll(this);
				}
			});
		FaderGroupModels.Reset();

		const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = WeakControlConsoleData.IsValid() ? WeakControlConsoleData->GetAllFaderGroups() : TArray<UDMXControlConsoleFaderGroup*>{};
		Algo::Transform(FaderGroups, FaderGroupModels, [this](UDMXControlConsoleFaderGroup* FaderGroup)
			{
				FaderGroup->GetOnElementAdded().AddSP(AsShared(), &FDMXControlConsoleGlobalFilterModel::OnFaderGroupElementsChanged);
				FaderGroup->GetOnElementRemoved().AddSP(AsShared(), &FDMXControlConsoleGlobalFilterModel::OnFaderGroupElementsChanged);
				FaderGroup->GetOnFixturePatchChanged().AddSP(AsShared(), &FDMXControlConsoleGlobalFilterModel::OnFaderGroupFixturePatchChanged);
				return MakeShared<FDMXControlConsoleFaderGroupFilterModel>(FaderGroup);
			});
	}

	void FDMXControlConsoleGlobalFilterModel::UpdateControlConsoleData()
	{
		UDMXControlConsoleData* ControlConsoleData = WeakControlConsoleData.Get();
		if (ControlConsoleData)
		{
			WeakControlConsoleData->GetOnFaderGroupAdded().RemoveAll(this);
			WeakControlConsoleData->GetOnFaderGroupRemoved().RemoveAll(this);
		}

		ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		WeakControlConsoleData = ControlConsoleData;
		if (ControlConsoleData)
		{
			WeakControlConsoleData->GetOnFaderGroupAdded().AddSP(AsShared(), &FDMXControlConsoleGlobalFilterModel::OnEditorConsoleDataChanged);
			WeakControlConsoleData->GetOnFaderGroupRemoved().AddSP(AsShared(), &FDMXControlConsoleGlobalFilterModel::OnEditorConsoleDataChanged);
		}
	}

	void FDMXControlConsoleGlobalFilterModel::ApplyFilter()
	{
		TArray<UDMXControlConsoleFaderBase*> MatchingFaders;
		for (const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>& FaderGroupModel : FaderGroupModels)
		{
			FaderGroupModel->Apply(GlobalFilter, NameFilterMode);
		}
	}

	void FDMXControlConsoleGlobalFilterModel::OnEditorConsoleDataChanged(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		if (FaderGroup && WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();
			UpdateNameFilterMode();
			ApplyFilter();
		}
	}

	void FDMXControlConsoleGlobalFilterModel::OnFaderGroupElementsChanged(IDMXControlConsoleFaderGroupElement* Element)
	{
		if (Element)
		{
			const UDMXControlConsoleFaderGroup& FaderGroup = Element->GetOwnerFaderGroupChecked();
			const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>* FaderGroupModelPtr = Algo::FindBy(FaderGroupModels, &FaderGroup,
				[](const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>& Model)
				{
					return Model->GetFaderGroup();
				});
			if (FaderGroupModelPtr)
			{
				FaderGroupModelPtr->Get().UpdateFaderModels();
				UpdateNameFilterMode();
				FaderGroupModelPtr->Get().Apply(GlobalFilter, NameFilterMode);
			}
		}
	}

	void FDMXControlConsoleGlobalFilterModel::OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch)
	{
		if (FaderGroup)
		{
			const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>* FaderGroupModelPtr = Algo::FindBy(FaderGroupModels, FaderGroup,
				[](const TSharedRef<FDMXControlConsoleFaderGroupFilterModel>& Model)
				{
					return Model->GetFaderGroup();
				});
			if (FaderGroupModelPtr)
			{
				FaderGroupModelPtr->Get().UpdateFaderModels();
				UpdateNameFilterMode();
				FaderGroupModelPtr->Get().Apply(GlobalFilter, NameFilterMode);
			}
		}
	}
}
