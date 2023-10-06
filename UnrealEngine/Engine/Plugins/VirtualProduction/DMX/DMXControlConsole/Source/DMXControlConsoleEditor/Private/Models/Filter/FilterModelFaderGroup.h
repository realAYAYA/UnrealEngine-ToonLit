// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatform.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class UDMXControlConsoleFaderGroup;


namespace UE::DMXControlConsoleEditor::FilterModel::Private
{
	enum class ENameFilterMode : uint8;
	class FFilterModelFader;
	struct FGlobalFilter;


	/** Filter of a fader group */
	struct FFaderGroupFilter
	{
		void Parse(const FString& InString);
		void Reset();

		FString String;
		TArray<FString> Names;
	};

	/** Filter model for a single fader group */
	class FFilterModelFaderGroup
		: public TSharedFromThis<FFilterModelFaderGroup>
	{
	public:
		FFilterModelFaderGroup() = delete;
		FFilterModelFaderGroup(UDMXControlConsoleFaderGroup* InFaderGroup);

		/** Returns the fader group of this model */
		UDMXControlConsoleFaderGroup* GetFaderGroup() const;

		/** Sets the fader group filter */
		void SetFilter(const FString& InString);

		/** Returns true if this group matches the global filter universes */
		[[nodiscard]] bool MatchesGlobalFilterUniverses(const FGlobalFilter& GlobalFilter) const;

		/** Returns true if this group matches the global filter absolute addresses */
		[[nodiscard]] bool MatchesGlobalFilterAbsoluteAddress(const FGlobalFilter& GlobalFilter) const;

		/** Returns true if this group matches the global filter names  (e.g. "MovingHead") */
		[[nodiscard]] bool MatchesGlobalFilterNames(const FGlobalFilter& GlobalFilter) const;

		/** Returns true if this group matches the global filter fixture ids */
		[[nodiscard]] bool MatchesGlobalFilterFixtureIDs(const FGlobalFilter& GlobalFilter) const;

		/** Returns true if this group has faders that match the global filter names (e.g. "R, G, Blue") */
		[[nodiscard]] bool HasFadersMatchingGlobalFilterNames(const FGlobalFilter& GlobalFilter) const;

		/** Applies the filters */
		void Apply(const FGlobalFilter& GlobalFilter, ENameFilterMode NameFilterMode);

		/** Updates the array of fader models */
		void UpdateFaderModels();

	private:
		/** Returns true if this group matches one of the universes in the global filter */
		bool IsMatchingGlobalFilterUniverses(const FGlobalFilter& GlobalFilter) const;

		/** Returns true if this group matches one of the absolute addresses in the global filter */
		bool IsMatchingGlobalFilterAbsoluteAddress(const FGlobalFilter& GlobalFilter) const;

		/** Returns true if this group matches one of the names in the global filter */
		bool IsMatchingGlobalFilterNames(const FGlobalFilter& GlobalFilter) const;

		/** Returns true if this group matches one of the fixture ids in the global filter */
		bool IsMatchingGlobalFilterFixtureIDs(const FGlobalFilter& GlobalFilter) const;

		/** The filter of this group */
		FFaderGroupFilter FaderGroupFilter;

		/** Fader Filter Models used in this model */
		TArray<TSharedRef<FFilterModelFader>> FaderModels;

		/** Fader group of this model */
		TWeakObjectPtr<UDMXControlConsoleFaderGroup> WeakFaderGroup;
	};
}
