// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "MoviePipelineQueue.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEditor"

/** Customize how properties for a job appear in the details panel. */
class FJobDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FJobDetailsCustomization>();
	}

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override
	{
		CustomizeDetails(*DetailBuilder);
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

		// Only display variables if there is one job selected
		if (ObjectsBeingCustomized.Num() != 1)
		{
			return;
		}

		// Hide the original assignments property (since it presents an asset picker by default), and add the Value
		// property (which is a property bag) instead -- that will present each of the variable overrides
		if (UMoviePipelineExecutorJob* Job = CastChecked<UMoviePipelineExecutorJob>(ObjectsBeingCustomized[0]))
		{
			const TSharedRef<IPropertyHandle> AssignmentsProperty =
				DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMoviePipelineExecutorJob, VariableAssignments));

			// Always hide the assignments property
			DetailBuilder.HideProperty(AssignmentsProperty);

			if (Job->VariableAssignments->GetNumAssignments() > 0)
			{
				// Add a new "Graph Variables" category if there is at least one assignment. Set as "Uncommon" priority to
				// push variables down below the other properties.
				IDetailCategoryBuilder& GraphVariablesCategory = DetailBuilder.EditCategory(
					"GraphVariables", LOCTEXT("GraphVariablesCategory", "Graph Variables"), ECategoryPriority::Uncommon);
			
				GraphVariablesCategory.AddExternalObjectProperty({Job->VariableAssignments.Get()}, FName("Value"));
			}
		}
	}
	//~ End IDetailCustomization interface
};

#undef LOCTEXT_NAMESPACE