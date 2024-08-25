// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupControllerComboBox.h"

#include "Algo/AnyOf.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXEditorStyle.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupControllerComboBox"

namespace UE::DMX::Private
{ 
	void SDMXControlConsoleEditorFaderGroupControllerComboBox::Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, cannot create fader group toolbar correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InFaderGroupControllerModel.IsValid(), TEXT("Invalid fader group controller model, cannot create fader group controller toolbar correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		WeakFaderGroupControllerModel = InFaderGroupControllerModel;

		UpdateComboBoxSource();

		ChildSlot
			[
				SNew(SBox)
				.MinDesiredWidth(120.f)
				[
					SNew(SHorizontalBox)

					// Combo Box section
					+ SHorizontalBox::Slot()
					.MaxWidth(120.f)
					.AutoWidth()
					[
						SAssignNew(FixturePatchesComboBox, SComboBox<TSharedPtr<FDMXEntityFixturePatchRef>>)
						.OptionsSource(&ComboBoxSource)
						.OnGenerateWidget(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::GenerateFixturePatchesComboBoxWidget)
						.OnComboBoxOpening(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::UpdateComboBoxSource)
						.OnSelectionChanged(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::OnComboBoxSelectionChanged)
						.ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")))
						.ItemStyle(&FDMXControlConsoleEditorStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("DMXControlConsole.FaderGroupToolbar")))
						.ToolTipText(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupControllerFixturePatchNameText)
						.Visibility(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::GetComboBoxVisibility)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.Padding(4.f)
							.AutoWidth()
							[
								SNew(SImage)
								.Image(FDMXEditorStyle::Get().GetBrush("Icons.FixturePatch"))
								.ColorAndOpacity(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupControllerEditorColor)
							]

							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.Padding(4.f, 0.f)
							.AutoWidth()
							[
								SNew(SBox)
								.WidthOverride(56.f)
								[
									SNew(STextBlock)
									.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
									.Text(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupControllerFixturePatchNameText)
								]
							]
						]
					]

					// Fader Group Controller user name text box section
					+ SHorizontalBox::Slot()
					.MaxWidth(120.f)
					.AutoWidth()
					[
						SNew(SBox)
						.MinDesiredHeight(26.f)
						[
							SNew(SEditableTextBox)
							.SelectAllTextWhenFocused(true)
							.Justification(ETextJustify::Center)
							.MinDesiredWidth(120.f)
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.Text(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupControllerUserNameText)
							.IsReadOnly(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::IsUserNameTextBoxReadOnly)
							.OnTextCommitted(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::OnComboBoxTextCommitted)
							.Visibility(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::GetUserNameTextBoxVisibility)
						]
					]
				]
			];
	}

	UDMXControlConsoleFaderGroupController* SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupController() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		return FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupControllerComboBox::GenerateFixturePatchesComboBoxWidget(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef)
	{
		if (FixturePatchRef.IsValid())
		{
			const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef->GetFixturePatch();
			const FLinearColor EditorTagColor = IsValid(FixturePatch) ? FixturePatch->EditorColor : FLinearColor::White;
			const FString FixturePatchName = IsValid(FixturePatch) ? FixturePatch->Name : TEXT("Undefined");

			const TSharedRef<SWidget> ComboBoxWidget =
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.MinDesiredWidth(6.f)
					.MinDesiredHeight(14.f)
					[
						SNew(SImage)
						.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
						.ColorAndOpacity(EditorTagColor)
					]
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.MaxWidth(140.f)
				.Padding(6.f, 0.f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(FText::FromString(FixturePatchName))
					.ToolTipText(FText::FromString(FixturePatchName))
				];

			if (FixturePatch)
			{
				ComboBoxWidget->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerComboBox::IsFixturePatchStillAvailable, FixturePatch));
			}

			return ComboBoxWidget;
		}

		return SNullWidget::NullWidget;
	}

	bool SDMXControlConsoleEditorFaderGroupControllerComboBox::IsFixturePatchStillAvailable(const UDMXEntityFixturePatch* InFixturePatch) const
	{
		if (!InFixturePatch)
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return false;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();

		const auto IsFixturePatchInUseLambda = [InFixturePatch](const UDMXControlConsoleFaderGroupController* FaderGroupController)
			{
				if (!FaderGroupController || !FaderGroupController->IsActive())
				{
					return false;
				}

				const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
				const bool bIsFixturePatchInUse = Algo::AnyOf(FaderGroups,
					[InFixturePatch](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
					{
						return FaderGroup.IsValid() && FaderGroup->GetFixturePatch() == InFixturePatch;
					});

				return bIsFixturePatchInUse;
			};

		return !Algo::AnyOf(AllFaderGroupControllers, IsFixturePatchInUseLambda);
	}

	void SDMXControlConsoleEditorFaderGroupControllerComboBox::UpdateComboBoxSource()
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		DMXLibrary = ControlConsoleData ? ControlConsoleData->GetDMXLibrary() : nullptr;

		ComboBoxSource.Reset(ComboBoxSource.Num());
		ComboBoxSource.Add(MakeShared<FDMXEntityFixturePatchRef>());

		if (DMXLibrary.IsValid())
		{
			const TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
			for (UDMXEntityFixturePatch* FixturePatch : FixturePatchesInLibrary)
			{
				if (FixturePatch && IsFixturePatchStillAvailable(FixturePatch))
				{
					const TSharedRef<FDMXEntityFixturePatchRef> FixturePatchRef = MakeShared<FDMXEntityFixturePatchRef>();
					FixturePatchRef->SetEntity(FixturePatch);
					ComboBoxSource.Add(FixturePatchRef);
				}
			}
		}

		if (FixturePatchesComboBox.IsValid())
		{
			FixturePatchesComboBox->RefreshOptions();
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerComboBox::OnComboBoxSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef, ESelectInfo::Type SelectInfo)
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		if (!FaderGroupControllerModel.IsValid() || !FaderGroupControllerModel->HasSingleFaderGroup())
		{
			return;
		}

		UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel->GetFaderGroupController();
		UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel->GetFirstAvailableFaderGroup();
		if (!FaderGroupController || !FaderGroup || !EditorModel.IsValid())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearElementControllersSelection(FaderGroupController);
		
		UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ControlConsoleData || !ActiveLayout)
		{
			return;
		}

		const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
		UDMXControlConsoleFaderGroup* FaderGroupToAdd = nullptr;

		const FScopedTransaction GenerateFaderGroupControllerFromComboBoxSelectionTransaction(LOCTEXT("GenerateFaderGroupControllerFromComboBoxSelectionTransaction", "Replace Fader Group"));
		if (FixturePatch)
		{
			// Find Fader Group Controller to Add in Control Console Data
			FaderGroupToAdd = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		}
		else if (ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			// Fader Group Controller to Add is a new Fader Group Controller
			UDMXControlConsoleFaderGroupRow& OwnerRow = FaderGroup->GetOwnerFaderGroupRowChecked();
			OwnerRow.PreEditChange(nullptr);
			FaderGroupToAdd = OwnerRow.AddFaderGroup(FaderGroup->GetIndex());
			OwnerRow.PostEditChange();
		}

		if (FaderGroupToAdd)
		{
			const int32 RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(FaderGroupController);
			const int32 ColumnIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(FaderGroupController);

			// Emplace Fader Group Controller with FaderGroupControllerToAdd
			ActiveLayout->PreEditChange(nullptr);
			UDMXControlConsoleFaderGroupController* NewController = ActiveLayout->AddToLayout(FaderGroupToAdd, FaderGroupToAdd->GetFaderGroupName(), RowIndex, ColumnIndex);
			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
			ActiveLayout->RemoveFromActiveFaderGroupControllers(FaderGroupController);
			ActiveLayout->PostEditChange();
			if (NewController)
			{
				NewController->Modify();
				NewController->SetIsActive(true);
				NewController->SetIsExpanded(FaderGroupController->IsExpanded());
			}

			FaderGroupController->Modify();
			FaderGroupController->SetIsActive(false);
			if (!FaderGroupController->HasFixturePatch())
			{
				FaderGroup->Modify();
				FaderGroup->Destroy();
				FaderGroupController->Destroy();
			}

			if (SelectionHandler->IsSelected(FaderGroupController))
			{
				constexpr bool bNotifySelectionChange = false;
				SelectionHandler->AddToSelection(NewController, bNotifySelectionChange);
				SelectionHandler->RemoveFromSelection(FaderGroupController);
			}
		}

		EditorModel->RequestUpdateEditorModel();
	}

	void SDMXControlConsoleEditorFaderGroupControllerComboBox::OnComboBoxTextCommitted(const FText& NewName, ETextCommit::Type InCommit)
	{
		if (NewName.IsEmpty())
		{
			return;
		}

		if (UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController())
		{
			const FScopedTransaction RenameFaderGroupTransaction(LOCTEXT("RenameFaderGroupTransaction", "Rename Fader Group"));

			const FString& NewNameAsString = NewName.ToString();
			FaderGroupController->Modify();
			FaderGroupController->SetUserName(NewNameAsString);
		}
	}

	FSlateColor SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupControllerEditorColor() const
	{
		if (const UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController())
		{
			return FaderGroupController->GetEditorColor();
		}

		return FLinearColor::White;
	}

	FText SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupControllerFixturePatchNameText() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
		if (!FaderGroupController)
		{
			return LOCTEXT("UndefinedFixturePatchNameText", "Undefined");
		}

		if(!FaderGroupController->HasFixturePatch())
		{
			return LOCTEXT("UndefinedFixturePatchNameText", "Undefined");
		}

		const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel->GetFirstAvailableFaderGroup();
		if (!FaderGroup)
		{
			return LOCTEXT("UndefinedFixturePatchNameText", "Undefined");
		}

		return FText::FromString(FaderGroup->GetFixturePatch()->Name);
	}

	FText SDMXControlConsoleEditorFaderGroupControllerComboBox::GetFaderGroupControllerUserNameText() const
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return LOCTEXT("UndefinedUserNameText", "Undefined");
		}

		const UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if(!FaderGroupController)
		{
			return LOCTEXT("UndefinedUserNameText", "Undefined");
		}
		
		if (ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return LOCTEXT("GroupUserNameText", "Group");
		}
		else
		{
			return FText::FromString(FaderGroupController->GetUserName());
		}
	}

	bool SDMXControlConsoleEditorFaderGroupControllerComboBox::IsUserNameTextBoxReadOnly() const
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		return ActiveLayout && ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked();
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerComboBox::GetUserNameTextBoxVisibility() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const bool bIsVisible = FaderGroupControllerModel.IsValid() && !FaderGroupControllerModel->HasSingleFaderGroup();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerComboBox::GetComboBoxVisibility() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const bool bIsVisible = FaderGroupControllerModel.IsValid() && FaderGroupControllerModel->HasSingleFaderGroup();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
