// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	/** Model for a filter in a DMX Control Console */
	class FDMXControlConsoleFilterModel
		: public TSharedFromThis<FDMXControlConsoleFilterModel>
	{
		DECLARE_DELEGATE_OneParam(FDMXControlConsoleFilterModelDelegate, TSharedPtr<FDMXControlConsoleFilterModel>)

	public:
		/** Constructor */
		FDMXControlConsoleFilterModel(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel, const FString& InFilterLabel, const FString& InFilterString, const FLinearColor InFilterColor, bool bInIsUserFilter = false);

		/** Gets the name label of the filter */
		FString GetFilterLabel() const { return FilterLabel; }

		/** Gets the string associated with this filter */
		FString GetFilterString() const { return FilterString; }

		/** Gets the color of the filter */
		FLinearColor GetFilterColor() const { return FilterColor; }

		/** Gets wether this is a user created filter or not */
		bool IsUserFilter() const { return bIsUserFilter; };

		/** Gets the enable state of the filter */
		bool IsEnabled() const { return bIsEnabled; };

		/** Sets the enable state of the filter */
		void SetIsEnabled(bool bEnable);

		/** Removes the filter described by this model from the control console editor data */
		void RemoveFilter() const;

		/** Called when the enable state of this filter has changed */
		FDMXControlConsoleFilterModelDelegate& GetOnEnableStateChanged() { return OnEnableStateChanged; }

	private:
		/** The displayed name label of the filter */
		FString FilterLabel;

		/** The filter string associated with the filter */
		FString FilterString;

		/** The color showed by the filter in the editor */
		FLinearColor FilterColor;

		/** True if the filter is created by the user */
		bool bIsUserFilter = false;

		/** True if the filter is enabled */
		bool bIsEnabled = false;

		/** The delegate to execute when the enable state of this filter has changed */
		FDMXControlConsoleFilterModelDelegate OnEnableStateChanged;

		/** Weak reference to the Control Console edior model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
