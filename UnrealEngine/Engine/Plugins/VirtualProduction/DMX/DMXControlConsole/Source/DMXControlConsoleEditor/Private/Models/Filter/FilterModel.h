// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "GenericPlatform/GenericPlatform.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleData;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


namespace UE::DMXControlConsoleEditor::FilterModel::Private
{ 
	class FFilterModelFader;
	class FFilterModelFaderGroup;

	/** Utility to parse a filter string into an array */
	TArray<FString> ParseStringIntoArray(const FString& InString);

	/** Defines if the model fitlers for fader group names, fader names or both. */
	enum class ENameFilterMode : uint8
	{
		MatchFaderGroupNames,
		MatchFaderNames,
		MatchFaderAndFaderGroupNames,
		None 
	};

	/** Global filter */
	struct FGlobalFilter
	{
		void Parse(const FString& InString);
		void Reset();

		TOptional<int32> GetUniverse() const;
		TOptional<int32> GetChannel() const;

		FString String;
		TArray<int32> Universes;
		TArray<int32> FixtureIDs;
		TOptional<int32> AbsoluteAddress;
		TArray<FString> Names;
		//... Add further members to Reset()
	};


	/** Model for control console filtering */
	class FFilterModel
		: public TSharedFromThis<FFilterModel>
		, public FSelfRegisteringEditorUndoClient
	{
		// Allow the DMXControlConsoleFaderGroupFilterModel to read Data
		friend FFilterModelFaderGroup;
		friend FFilterModelFader;

	public:
		/** Initializes the model */
		void Initialize();

		/** Returns the model. Should not be called before the model is initialized. */
		static FFilterModel& Get();

		/** Sets a filter that is applied to the model */
		void SetGlobalFilter(const FString& FilterString);

		/** Sets a filter to the specified fader group */
		void SetFaderGroupFilter(UDMXControlConsoleFaderGroup* FaderGroup, const FString& FilterString);

		/** Delegate broadcast when the filter changed */
		FSimpleMulticastDelegate OnFilterChanged;

	protected:
		//~ Begin FSelfRegisteringEditorUndoClient interface
		virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FSelfRegisteringEditorUndoClient interface

	private:
		/** Updates the name filter mode to use */
		void UpdateNameFilterMode();

		/** Initializes the model from the current editor control console */
		void InitializeInternal();

		/** Updates the array of fader group models */
		void UpdateFaderGroupModels();

		/** Updates DMX Control Console Data reference */
		void UpdateControlConsoleData();

		/** Applies filter */
		void ApplyFilter();

		/** Called when Control Console Data have been changed by adding/removing Fader Groups */
		void OnEditorConsoleDataChanged(const UDMXControlConsoleFaderGroup* FaderGroup);

		/** Called when Fader Group has been changed by adding/removing Elements */
		void OnFaderGroupElementsChanged(IDMXControlConsoleFaderGroupElement* Element);

		/** Called when Fader Group Fixture Patch has changed */
		void OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch);

		/** The global filter used in this model */
		FGlobalFilter GlobalFilter;

		/** Mode in which the filter currently shoudl be applied */
		ENameFilterMode NameFilterMode = ENameFilterMode::None;

		/** Fader Group Models used in this model */
		TArray<TSharedRef<FFilterModelFaderGroup>> FaderGroupModels;

		/** Control console data used in this model */
		TWeakObjectPtr<UDMXControlConsoleData> WeakControlConsoleData;
	};
}
