// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailWidgetRow.h"
#include "EditorShowFlags.h"
#include "Graph/Renderers/MovieGraphShowFlags.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEditor"

/** Customize how show flags appear in the details panel. */
class FMovieGraphShowFlagsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FMovieGraphShowFlagsCustomization>();
	}

protected:
	//~ Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		static const TMap<EShowFlagGroup, FText> GroupNames =
		{
			{SFG_Normal, LOCTEXT("GraphNormalSF", "Normal")},
			{SFG_PostProcess, LOCTEXT("GraphPostProcessSF", "Post Processing")},
			{SFG_LightTypes, LOCTEXT("GraphLightTypesSF", "Light Types")},
			{SFG_LightingComponents, LOCTEXT("GraphLightingComponentsSF", "Lighting Components")},
			{SFG_LightingFeatures, LOCTEXT("GraphLightingFeaturesSF", "Lighting Features")},
			{SFG_Lumen, LOCTEXT("GraphLumenSF", "Lumen")},
			{SFG_Nanite, LOCTEXT("GraphNaniteSF", "Nanite")},
			{SFG_Developer, LOCTEXT("GraphDeveloperSF", "Developer")},
			{SFG_Visualize, LOCTEXT("GraphVisualizeSF", "Visualize")},
			{SFG_Advanced, LOCTEXT("GraphAdvancedSF", "Advanced")}
		};

		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);
		
		check(PropertyHandle->IsValidHandle());

		const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(PropertyHandle->GetProperty());
		UMovieGraphShowFlags* ShowFlagObject = Cast<UMovieGraphShowFlags>(
			ObjectProperty->GetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<void>(OuterObjects[0])));

		// Group together the show flags with the groups they show up under in the UI
		TMap<EShowFlagGroup, TArray<FShowFlagData>> GroupedShowFlags;
		for (FShowFlagData& ShowFlag : GetShowFlagMenuItems())
		{
			TArray<FShowFlagData>& GroupShowFlags = GroupedShowFlags.FindOrAdd(ShowFlag.Group);
			GroupShowFlags.Add(MoveTemp(ShowFlag));
		}

		// Make each show flag group a group in the UI, and each show flag a row under its respective group
		for (const TTuple<EShowFlagGroup, FText>& ShowFlagGroup : GroupNames)
		{
			const EShowFlagGroup& Group = ShowFlagGroup.Key;
			const FText& GroupName = ShowFlagGroup.Value;
			
			const TArray<FShowFlagData>* ShowFlags = GroupedShowFlags.Find(Group);
			if (!ShowFlags)
			{
				continue;
			}
			
			IDetailGroup& FlagGroup = ChildBuilder.AddGroup(FName(GroupName.ToString()), GroupName);

			for (const FShowFlagData& ShowFlag : *ShowFlags)
			{
				const uint32 ShowFlagIndex = ShowFlag.EngineShowFlagIndex;

				// Temporary workaround: ShowFlag.DisplayName can sometimes be blank, so the non-display name is used instead.
				const FText ShowFlagText = FText::FromName(ShowFlag.ShowFlagName);
				
				FlagGroup.AddWidgetRow()
				.FilterString(ShowFlagText)
				.NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(0, 0, 1, 0)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([ShowFlagObject, ShowFlagIndex]()
						{
							return ShowFlagObject->IsShowFlagOverridden(ShowFlagIndex)
								? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([ShowFlagObject, ShowFlagIndex](const ECheckBoxState NewState)
						{
							const bool bIsOverridden = (NewState == ECheckBoxState::Checked);
							ShowFlagObject->SetShowFlagOverridden(ShowFlagIndex, bIsOverridden);
						})
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.IsEnabled_Lambda([ShowFlagObject, ShowFlagIndex]() { return ShowFlagObject->IsShowFlagOverridden(ShowFlagIndex); })
						.Text(ShowFlagText)
						.Font(CustomizationUtils.GetRegularFont())
					]
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.IsEnabled_Lambda([ShowFlagObject, ShowFlagIndex]() { return ShowFlagObject->IsShowFlagOverridden(ShowFlagIndex); })
					.IsChecked_Lambda([ShowFlagObject, ShowFlagIndex]()
					{
						return ShowFlagObject->IsShowFlagEnabled(ShowFlagIndex)
							? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([ShowFlagObject, ShowFlagIndex](const ECheckBoxState NewState)
					{
						const bool bIsUsed = (NewState == ECheckBoxState::Checked);
						ShowFlagObject->SetShowFlagEnabled(ShowFlagIndex, bIsUsed);
					})
				];
			}
		}
	}
	//~ End IPropertyTypeCustomization interface
};

#undef LOCTEXT_NAMESPACE
