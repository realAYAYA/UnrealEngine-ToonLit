// Copyright Epic Games, Inc. All Rights Reserved.

#include "FilterModel.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXEditorUtils.h"
#include "FilterModelFaderGroup.h"
#include "Internationalization/Regex.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Misc/TransactionObjectEvent.h"
#include "Models/DMXControlConsoleEditorModel.h"


namespace UE::DMXControlConsoleEditor::FilterModel::Private
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
			Universes = FDMXEditorUtils::ParseUniverses(UniverseSubstring);
		}

		// Parse fixture IDs. Ignore universes.
		const FString StringWithoutUniverses = String.Replace(*UniverseSubstring, TEXT(""));
		FixtureIDs = FDMXEditorUtils::ParseFixtureIDs(StringWithoutUniverses);

		// Parse address. Ignore universes.
		int32 Address;
		if (FDMXEditorUtils::ParseAddress(StringWithoutUniverses, Address))
		{
			AbsoluteAddress = Address;
		}

		// Parse names. Ignore universes.
		Names = ParseStringIntoArray(StringWithoutUniverses);
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


	void FFilterModel::Initialize()
	{
		InitializeInternal();

		// Listen to loading control consoles in editor 
		UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		EditorModel->GetOnConsoleLoaded().AddSP(AsShared(), &FFilterModel::InitializeInternal);
	}

	FFilterModel& FFilterModel::Get()
	{
		UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		check(EditorModel->FilterModel.IsValid());
		return *EditorModel->FilterModel;
	}

	void FFilterModel::SetGlobalFilter(const FString& NewFilter)
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

	void FFilterModel::SetFaderGroupFilter(UDMXControlConsoleFaderGroup* FaderGroup, const FString& InString)
	{
		const TSharedRef<FFilterModelFaderGroup>* FaderGroupModelPtr = Algo::FindBy(FaderGroupModels, FaderGroup,
			[](const TSharedRef<FFilterModelFaderGroup>& Model)
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

	bool FFilterModel::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
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

	void FFilterModel::PostUndo(bool bSuccess)
	{
		if (WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();
			UpdateNameFilterMode();
			ApplyFilter();
		}
	}

	void FFilterModel::PostRedo(bool bSuccess)
	{
		if (WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();
			UpdateNameFilterMode();
			ApplyFilter();
		}
	}

	void FFilterModel::UpdateNameFilterMode()
	{
		bool bGroupMatchesGlobalFilterNames = false;
		bool bHasFadersMatchingGlobalFilterNames = false;
		for (const TSharedRef<FFilterModelFaderGroup>& FaderGroupModel : FaderGroupModels)
		{
			bGroupMatchesGlobalFilterNames |= 
				FaderGroupModel->MatchesGlobalFilterUniverses(GlobalFilter) ||
				FaderGroupModel->MatchesGlobalFilterAbsoluteAddress(GlobalFilter) ||
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

	void FFilterModel::InitializeInternal()
	{
		UpdateControlConsoleData();

		if (WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();

			GlobalFilter.Parse(WeakControlConsoleData->FilterString);
			ApplyFilter();
		}
	}

	void FFilterModel::UpdateFaderGroupModels()
	{
		Algo::ForEach(FaderGroupModels, [this](const TSharedRef<FFilterModelFaderGroup>& Model)
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
				FaderGroup->GetOnElementAdded().AddSP(AsShared(), &FFilterModel::OnFaderGroupElementsChanged);
				FaderGroup->GetOnElementRemoved().AddSP(AsShared(), &FFilterModel::OnFaderGroupElementsChanged);
				FaderGroup->GetOnFixturePatchChanged().AddSP(AsShared(), &FFilterModel::OnFaderGroupFixturePatchChanged);
				return MakeShared<FFilterModelFaderGroup>(FaderGroup);
			});
	}

	void FFilterModel::UpdateControlConsoleData()
	{
		if (WeakControlConsoleData.IsValid())
		{
			WeakControlConsoleData->GetOnFaderGroupAdded().RemoveAll(this);
			WeakControlConsoleData->GetOnFaderGroupRemoved().RemoveAll(this);
		}

		const UDMXControlConsoleEditorModel* EditorModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsole* EditorConsole = EditorModel->GetEditorConsole();
		WeakControlConsoleData = EditorConsole ? EditorConsole->GetControlConsoleData() : nullptr;
		if (WeakControlConsoleData.IsValid())
		{
			WeakControlConsoleData->GetOnFaderGroupAdded().AddSP(AsShared(), &FFilterModel::OnEditorConsoleDataChanged);
			WeakControlConsoleData->GetOnFaderGroupRemoved().AddSP(AsShared(), &FFilterModel::OnEditorConsoleDataChanged);
		}
	}

	void FFilterModel::ApplyFilter()
	{
		TArray<UDMXControlConsoleFaderBase*> MatchingFaders;
		for (const TSharedRef<FFilterModelFaderGroup>& FaderGroupModel : FaderGroupModels)
		{
			FaderGroupModel->Apply(GlobalFilter, NameFilterMode);
		}
	}

	void FFilterModel::OnEditorConsoleDataChanged(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		if (FaderGroup && WeakControlConsoleData.IsValid())
		{
			UpdateFaderGroupModels();
			UpdateNameFilterMode();
			ApplyFilter();
		}
	}

	void FFilterModel::OnFaderGroupElementsChanged(IDMXControlConsoleFaderGroupElement* Element)
	{
		if (Element)
		{
			const UDMXControlConsoleFaderGroup& FaderGroup = Element->GetOwnerFaderGroupChecked();
			const TSharedRef<FFilterModelFaderGroup>* FaderGroupModelPtr = Algo::FindBy(FaderGroupModels, &FaderGroup,
				[](const TSharedRef<FFilterModelFaderGroup>& Model)
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

	void FFilterModel::OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch)
	{
		if (FaderGroup)
		{
			const TSharedRef<FFilterModelFaderGroup>* FaderGroupModelPtr = Algo::FindBy(FaderGroupModels, FaderGroup,
				[](const TSharedRef<FFilterModelFaderGroup>& Model)
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
