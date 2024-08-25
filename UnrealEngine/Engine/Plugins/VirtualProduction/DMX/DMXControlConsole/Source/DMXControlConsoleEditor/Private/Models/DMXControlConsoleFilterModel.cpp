// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFilterModel.h"

#include "DMXControlConsoleEditorData.h"
#include "Models/DMXControlConsoleEditorModel.h"


namespace UE::DMX::Private
{
	FDMXControlConsoleFilterModel::FDMXControlConsoleFilterModel(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel, const FString& InFilterLabel, const FString& InFilterString, const FLinearColor InFilterColor, bool bInIsUserFilter)
		: FilterLabel(InFilterLabel)
		, FilterString(InFilterString)
		, FilterColor(InFilterColor)
		, bIsUserFilter(bInIsUserFilter)
		, WeakEditorModel(InWeakEditorModel)
	{
	}

	void FDMXControlConsoleFilterModel::SetIsEnabled(bool bEnable)
	{
		bIsEnabled = bEnable;

		OnEnableStateChanged.ExecuteIfBound(SharedThis(this));
	}

	void FDMXControlConsoleFilterModel::RemoveFilter() const
	{
		UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		if (ControlConsoleEditorData)
		{
			ControlConsoleEditorData->RemoveUserFilter(FilterLabel);
		}
	}
}
