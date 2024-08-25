// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/PropertyAnimatorCoreEditorDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Subsystems/PropertyAnimatorCoreEditorSubsystem.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorDetailCustomization"

void FPropertyAnimatorCoreEditorDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	const TSharedPtr<IPropertyHandle> LinkedPropertiesHandle = InDetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreBase, LinkedProperties),
		UPropertyAnimatorCoreBase::StaticClass()
	);

	if (!LinkedPropertiesHandle.IsValid())
	{
		return;
	}

	TArray<TWeakObjectPtr<UPropertyAnimatorCoreBase>> AnimatorsWeak = InDetailBuilder.GetObjectsOfTypeBeingCustomized<UPropertyAnimatorCoreBase>();

	if (AnimatorsWeak.Num() != 1 || !AnimatorsWeak[0].IsValid())
	{
		return;
	}

	AnimatorWeak = AnimatorsWeak[0].Get();
	IDetailPropertyRow* PropertyRow = InDetailBuilder.EditDefaultProperty(LinkedPropertiesHandle);

	if (!PropertyRow)
	{
		return;
	}

	PropertyRow->ShouldAutoExpand(true);

	const TSharedRef<SWidget> NewValueWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f, 0.f)
		[
			SNew(SButton)
			.ContentPadding(2.f)
			.ToolTipText(LOCTEXT("UnlinkProperties", "Unlink properties from this animator"))
			.IsEnabled(this, &FPropertyAnimatorCoreEditorDetailCustomization::IsAnyPropertyLinked)
			.OnClicked(this, &FPropertyAnimatorCoreEditorDetailCustomization::UnlinkProperties)
			.Content()
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(16.f))
				.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f, 0.f)
		[
			SNew(SComboButton)
			.ContentPadding(2.f)
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ToolTipText(LOCTEXT("LinkProperties", "Link properties to this animator"))
			.HasDownArrow(false)
			.OnGetMenuContent(this, &FPropertyAnimatorCoreEditorDetailCustomization::GenerateLinkMenu)
			.ButtonContent()
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(16.f))
				.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
			]
		];

	if (FDetailWidgetDecl* ValueWidget = PropertyRow->CustomValueWidget())
	{
		ValueWidget->Widget = NewValueWidget;
	}
	else
	{
		PropertyRow->CustomWidget(true)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsEnabled(this, &FPropertyAnimatorCoreEditorDetailCustomization::IsAnyPropertyLinked)
				.IsChecked(this, &FPropertyAnimatorCoreEditorDetailCustomization::IsPropertiesEnabled)
				.OnCheckStateChanged(this, &FPropertyAnimatorCoreEditorDetailCustomization::OnPropertiesEnabled)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				LinkedPropertiesHandle->CreatePropertyNameWidget()
			]
		]
		.ValueContent()
		[
			NewValueWidget
		];
	}
}

TSharedRef<SWidget> FPropertyAnimatorCoreEditorDetailCustomization::GenerateLinkMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	UPropertyAnimatorCoreBase* Animator = AnimatorWeak.Get();

	if (!Animator || !ToolMenus)
	{
		return SNullWidget::NullWidget;
	}

	static constexpr TCHAR MenuName[] = TEXT("LinkPropertiesCustomizationMenu");

	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* const LinkPropertiesMenu = ToolMenus->RegisterMenu(MenuName, NAME_None, EMultiBoxType::Menu);
		LinkPropertiesMenu->AddDynamicSection(TEXT("FillLinkPropertiesCustomizationMenu"), FNewToolMenuDelegate::CreateStatic(&FPropertyAnimatorCoreEditorDetailCustomization::FillLinkMenu));
	}

	return ToolMenus->GenerateWidget(MenuName, FToolMenuContext(Animator));
}

void FPropertyAnimatorCoreEditorDetailCustomization::FillLinkMenu(UToolMenu* InToolMenu)
{
	UPropertyAnimatorCoreEditorSubsystem* AnimatorEditorSubsystem = UPropertyAnimatorCoreEditorSubsystem::Get();
	UPropertyAnimatorCoreBase* Animator = InToolMenu->FindContext<UPropertyAnimatorCoreBase>();

	if (!Animator || !AnimatorEditorSubsystem)
	{
		return;
	}

	const FPropertyAnimatorCoreEditorMenuContext MenuContext({Animator}, {});
	FPropertyAnimatorCoreEditorMenuOptions MenuOptions(
		{
			EPropertyAnimatorCoreEditorMenuType::Link
		}
	);
	MenuOptions.CreateSubMenu(false);

	AnimatorEditorSubsystem->FillAnimatorMenu(InToolMenu, MenuContext, MenuOptions);
}

bool FPropertyAnimatorCoreEditorDetailCustomization::IsAnyPropertyLinked() const
{
	const UPropertyAnimatorCoreBase* Animator = AnimatorWeak.Get();
	if (!Animator)
	{
		return false;
	}

	return Animator->GetLinkedPropertiesCount() > 0;
}

ECheckBoxState FPropertyAnimatorCoreEditorDetailCustomization::IsPropertiesEnabled() const
{
	ECheckBoxState State = ECheckBoxState::Undetermined;

	const UPropertyAnimatorCoreBase* Animator = AnimatorWeak.Get();
	if (!Animator)
	{
		return State;
	}

	int32 EnabledProperties = 0;
	constexpr bool bResolve = false;
	Animator->ForEachLinkedProperty<UPropertyAnimatorCoreContext>([&EnabledProperties](UPropertyAnimatorCoreContext* InOptions, const FPropertyAnimatorCoreData& InProperty)->bool
	{
		if (InOptions->IsAnimated())
		{
			EnabledProperties++;
		}
		return true;
	}, bResolve);

	const int32 LinkedProperties = Animator->GetLinkedPropertiesCount();

	if (LinkedProperties > 0 && EnabledProperties == 0)
	{
		State = ECheckBoxState::Unchecked;
	}
	else if (LinkedProperties > 0 && EnabledProperties < LinkedProperties)
	{
		State = ECheckBoxState::Undetermined;
	}
	else
	{
		State = ECheckBoxState::Checked;
	}

	return State;
}

void FPropertyAnimatorCoreEditorDetailCustomization::OnPropertiesEnabled(ECheckBoxState InNewState) const
{
	const UPropertyAnimatorCoreBase* Animator = AnimatorWeak.Get();
	if (!Animator)
	{
		return;
	}

	if (InNewState == ECheckBoxState::Undetermined)
	{
		return;
	}

	constexpr bool bResolve = false;
	Animator->ForEachLinkedProperty<UPropertyAnimatorCoreContext>([InNewState](UPropertyAnimatorCoreContext* InOptions, const FPropertyAnimatorCoreData& InProperty)->bool
	{
		InOptions->SetAnimated(InNewState == ECheckBoxState::Checked);
		return true;
	}, bResolve);
}

FReply FPropertyAnimatorCoreEditorDetailCustomization::UnlinkProperties() const
{
	UPropertyAnimatorCoreBase* Animator = AnimatorWeak.Get();
	if (!Animator)
	{
		return FReply::Handled();
	}

	if (UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		const TSet<FPropertyAnimatorCoreData> LinkedProperties = Animator->GetLinkedProperties();
		AnimatorSubsystem->UnlinkAnimatorProperties(Animator, LinkedProperties, true);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
