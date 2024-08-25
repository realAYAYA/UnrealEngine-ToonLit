// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConnectionTargetNodeBuilder.h"

#include "Customizations/StateSwitcher/SStringSelectionComboBox.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "FConnectionTargetNodeBuilder"

namespace UE::VCamCoreEditor::Private
{
	FConnectionTargetNodeBuilder::FConnectionTargetNodeBuilder(
		TSharedRef<IPropertyHandle> ConnectionTargets,
		TAttribute<TArray<FName>> ChooseableConnections,
		IPropertyTypeCustomizationUtils& CustomizationUtils
		)
		: ConnectionTargets(MoveTemp(ConnectionTargets))
		, ChooseableConnections(MoveTemp(ChooseableConnections))
		, RegularFont(CustomizationUtils.GetRegularFont())
		, PropertyUtilities(CustomizationUtils.GetPropertyUtilities())
	{}

	void FConnectionTargetNodeBuilder::InitDelegates()
	{
		ConnectionTargets->SetOnPropertyValueChanged(
			// Must be latent because SetOnPropertyValueChanged is called before the value is finally set
			FSimpleDelegate::CreateSP(this, &FConnectionTargetNodeBuilder::ScheduleChildRebuild)
			);
	}

	void FConnectionTargetNodeBuilder::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
	{
		OnRegenerateChildren = MoveTemp(InOnRegenerateChildren);
	}

	void FConnectionTargetNodeBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
	{
		NodeRow
			.NameContent()
			[
				ConnectionTargets->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				ConnectionTargets->CreatePropertyValueWidget()
			];
	}

	void FConnectionTargetNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
	{
		uint32 NumEntry = 0;
		ConnectionTargets->GetNumChildren(NumEntry);
		for (uint32 EntryIndex = 0; EntryIndex < NumEntry; ++EntryIndex)
		{
			TSharedPtr<IPropertyHandle> EntryHandle = ConnectionTargets->GetChildHandle(EntryIndex);
			if (!EntryHandle.IsValid())
			{
				continue;
			}

			EntryHandle->MarkHiddenByCustomization();
			TSharedPtr<IPropertyHandle> KeyHandle = EntryHandle->GetKeyHandle();
			
			constexpr bool bShowChildren = true;
			ChildrenBuilder.AddProperty(EntryHandle.ToSharedRef())
				.CustomWidget(bShowChildren)
				.WholeRowContent()
				[
					SNew(SHorizontalBox)

					// Add a warning in case the user has changed the widget from which the data of ChooseableConnections originates 
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.DesiredSizeOverride(FVector2D{ 24.f, 24.f })
						.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor"))
						.ToolTipText(LOCTEXT("ConnectionNotFound", "This connection was not found on the target widget."))
						.Visibility_Lambda([WeakThis = TWeakPtr<FConnectionTargetNodeBuilder>(SharedThis(this)), KeyHandle]()
						{
							if (TSharedPtr<FConnectionTargetNodeBuilder> ThisPin = WeakThis.Pin())
							{
								FName Name;
								KeyHandle->GetValue(Name);
								
								return ThisPin->ChooseableConnections.Get().Contains(Name)
									? EVisibility::Collapsed
									: EVisibility::Visible;
							}
							return EVisibility::Collapsed;
						})
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SStringSelectionComboBox)
							.SelectedItem_Lambda([KeyHandle]()
							{
								FString Value;
								KeyHandle->GetValue(Value);
								return Value;
							})
							.ItemList(this, &FConnectionTargetNodeBuilder::GetChooseableConnectionsAsStringArray)
							.OnItemSelected_Lambda([WeakThis = TWeakPtr<FConnectionTargetNodeBuilder>(SharedThis(this)), KeyHandle, PropertyUtils = PropertyUtilities](const FString& SelectedItem)
							{
								KeyHandle->NotifyPreChange();
								KeyHandle->SetValue(FName(*SelectedItem));
								KeyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
									
								if (const TSharedPtr<FConnectionTargetNodeBuilder> This = WeakThis.Pin())
								{
									// Must be latent because SetOnPropertyValueChanged is called before the value is finally set
									This->ScheduleChildRebuild();
								}
							})
							.Font(RegularFont)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton({}, FExecuteAction::CreateLambda([WeakThis = TWeakPtr<FConnectionTargetNodeBuilder>(SharedThis(this)), EntryIndex, ConnectionTargetsPin = ConnectionTargets]()
						{
							ConnectionTargetsPin->NotifyPreChange();
							ConnectionTargetsPin->AsMap()->DeleteItem(EntryIndex);
							ConnectionTargetsPin->NotifyPostChange(EPropertyChangeType::ValueSet);

							if (const TSharedPtr<FConnectionTargetNodeBuilder> This = WeakThis.Pin())
							{
								// Must be latent because SetOnPropertyValueChanged is called before the value is finally set
								This->ScheduleChildRebuild();
							}
						}), {})
					]
				];
		}
	}

	TArray<FString> FConnectionTargetNodeBuilder::GetChooseableConnectionsAsStringArray() const
	{
		TArray<FString> AsArray;
		Algo::Transform(ChooseableConnections.Get(), AsArray, [](FName Connection){ return Connection.ToString(); });
		return AsArray;
	}

	void FConnectionTargetNodeBuilder::ScheduleChildRebuild()
	{
		PropertyUtilities->EnqueueDeferredAction(FSimpleDelegate::CreateSP(this, &FConnectionTargetNodeBuilder::RebuilChildren));
	}

	void FConnectionTargetNodeBuilder::RebuilChildren()
	{
		check(OnRegenerateChildren.IsBound());
		OnRegenerateChildren.Execute();
	}
}

#undef LOCTEXT_NAMESPACE