// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatform.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class UDMXControlConsoleFaderBase;


namespace UE::DMXControlConsoleEditor::FilterModel::Private
{
	enum class ENameFilterMode : uint8;
	struct FFaderGroupFilter;
	struct FGlobalFilter;


	/** Filter model for a single fader group */
	class FFilterModelFader
		: public TSharedFromThis<FFilterModelFader>
	{
	public:
		FFilterModelFader() = delete;
		FFilterModelFader(UDMXControlConsoleFaderBase* InFader);

		/** returns true if the fader name contains one of the names in the Names array */
		[[nodiscard]] bool MatchesAnyName(const TArray<FString>& Names) const;

		/** Returns true if the fader matches the global filter */
		[[nodiscard]] bool MatchesGlobalFilter(const FGlobalFilter& GlobalFilter, ENameFilterMode NameFilterMode);

		/** Returns true if the fader matches the fader group filter */
		[[nodiscard]] bool MatchesFaderGroupFilter(const FFaderGroupFilter& FaderGroupFilter);

		/** Gets the fader this model uses */
		UDMXControlConsoleFaderBase* GetFader() const;

	private:
		/** The fader this model uses */
		TWeakObjectPtr<UDMXControlConsoleFaderBase> WeakFader;
	};
}
