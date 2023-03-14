// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAnimationModifierContentBrowserWindow.h"

#include "Animation/AnimSequence.h"
#include "AnimationModifier.h"
#include "AnimationModifierHelpers.h"
#include "AnimationModifiersAssetUserData.h"
#include "ClassViewerModule.h"
#include "Delegates/Delegate.h"
#include "DetailsViewArgs.h"
#include "Framework/SlateDelegates.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "IDetailsView.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SModifierListview.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/SubclassOf.h"
#include "Types/SlateEnums.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

struct FGeometry;

#define LOCTEXT_NAMESPACE "AnimationModifierContentBrowserWindow"

void SAnimationModifierContentBrowserWindow::Construct(const FArguments& InArgs)
{
	CreateInstanceDetailsView();

	WidgetWindow = InArgs._WidgetWindow;
	AnimSequences = InArgs._AnimSequences;

	FOnGetContent GetContent = FOnGetContent::CreateLambda(
		[this]()
	{
		return FAnimationModifierHelpers::GetModifierPicker(FOnClassPicked::CreateRaw(this, &SAnimationModifierContentBrowserWindow::OnModifierPicked));
	});

	this->ChildSlot
	[
		SNew(SOverlay)		
		+SOverlay::Slot()
		[				
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(3.0f, 3.0f)
					.AutoWidth()
					[
						SAssignNew(AddModifierCombobox, SComboButton)
						.OnGetMenuContent(GetContent)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AnimationModifierWindow_AddModifier", "Add Modifier"))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[				
					SNew(SSplitter)
					.Orientation(EOrientation::Orient_Vertical)

					+ SSplitter::Slot()
					.Value(.5f)
					[
						SNew(SBox)
						.Padding(2.0f)
						[
							SAssignNew(ModifierListView, SModifierListView)
							.Items(&ModifierItems)
							.InstanceDetailsView(ModifierInstanceDetailsView)
							.OnRemoveModifier(FOnModifierArray::CreateSP(this, &SAnimationModifierContentBrowserWindow::RemoveModifiersCallback))
						]
					]

					+ SSplitter::Slot()
					.Value(.5f)
					[	
						SNew(SBox)
						.Padding(2.0f)
						[
							ModifierInstanceDetailsView->AsShared()
						]
					]
				]
			]	

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("AnimationModifierWindow_Import", "Apply"))
					.ToolTipText(LOCTEXT("AnimationModifierWindow_Import_ToolTip", "Apply adding modifiers(s)."))
					.IsEnabled(this, &SAnimationModifierContentBrowserWindow::CanApply)
					.OnClicked(this, &SAnimationModifierContentBrowserWindow::OnApply)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("AnimationModifierWindow_Cancel", "Cancel"))
					.ToolTipText(LOCTEXT("AnimationModifierWindow_Cancel_ToolTip", "Cancels adding modifiers(s)."))
					.OnClicked(this, &SAnimationModifierContentBrowserWindow::OnCancel)
				]
			]	
		]
	];
}

FReply SAnimationModifierContentBrowserWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancel();
	}

	return FReply::Unhandled();
}

void SAnimationModifierContentBrowserWindow::RemoveModifiersCallback(const TArray<TWeakObjectPtr<UAnimationModifier>>& ModifiersToRemove)
{
	Modifiers.RemoveAll([&ModifiersToRemove](UAnimationModifier* Modifier) { return ModifiersToRemove.Contains(Modifier); });
	ModifierItems.RemoveAll([&ModifiersToRemove](TSharedPtr<FModifierListviewItem> ModifierItem) { return ModifiersToRemove.Contains(ModifierItem->Instance); });
	ModifierListView->Refresh();
}

void SAnimationModifierContentBrowserWindow::OnModifierPicked(UClass* PickedClass)
{	
	UAnimationModifier* Processor = FAnimationModifierHelpers::CreateModifierInstance(GetTransientPackage(), PickedClass);

	Modifiers.Add(Processor);

	FModifierListviewItem* Item = new FModifierListviewItem();
	Item->Instance = Processor;
	Item->Class = Processor->GetClass();
	Item->Index = Modifiers.Num() - 1;
	Item->OuterClass = nullptr;
	ModifierItems.Add(ModifierListviewItem(Item));

	// Close the combo box
	AddModifierCombobox->SetIsOpen(false);

	ModifierListView->Refresh();
}

void SAnimationModifierContentBrowserWindow::CreateInstanceDetailsView()
{
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;

	ModifierInstanceDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	ModifierInstanceDetailsView->SetDisableCustomDetailLayouts(true);
}

FReply SAnimationModifierContentBrowserWindow::OnApply()
{
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_ApplyModifiers", "Applying Animation Modifier(s) to Animation Sequence(s)"));

	TArray<UAnimationModifiersAssetUserData*> AssetUserData;

	bool bCloseWindow = true;

	// Retrieve or create asset user data 
	for (UAnimSequence* AnimationSequence : AnimSequences)
	{
		UAnimationModifiersAssetUserData* UserData = AnimationSequence->GetAssetUserData<UAnimationModifiersAssetUserData>();
		if (!UserData)
		{
			UserData = NewObject<UAnimationModifiersAssetUserData>(AnimationSequence, UAnimationModifiersAssetUserData::StaticClass());
			checkf(UserData, TEXT("Unable to instantiate AssetUserData class"));
			UserData->SetFlags(RF_Transactional);
			AnimationSequence->AddAssetUserData(UserData);
		}

		AssetUserData.Add(UserData);
	}

	// For each added modifier create add a new instance to each of the user data entries, using the one(s) set up in the window as template(s)
	for (UAnimationModifier* Modifier : Modifiers)
	{
		for (UAnimationModifiersAssetUserData* UserData : AssetUserData)
		{
			const bool bAlreadyContainsModifier = UserData->GetAnimationModifierInstances().ContainsByPredicate([Modifier](UAnimationModifier* TestModifier) { return Modifier->GetClass() == TestModifier->GetClass(); });

			bool bUserInputResult = true;
			if (bAlreadyContainsModifier)
			{
				FText MessageFormat = LOCTEXT("AnimationModifierWindow_AlreadyContainsModifierDialogText", "{0} already contains Animation Modifier {1}, are you sure you want to add another instance?");
				FText MessageTitle = LOCTEXT("AnimationModifierWindow_AlreadyContainsModifierTitle", "Already contains Animation Modifier!");
				bUserInputResult = FMessageDialog::Open(EAppMsgType::YesNo, FText::FormatOrdered(MessageFormat, FText::FromString(UserData->GetOuter()->GetName()), FText::FromString(Modifier->GetClass()->GetName())), &MessageTitle) == EAppReturnType::Yes;
			}
			
			bCloseWindow = bUserInputResult;
			if (!bAlreadyContainsModifier || bUserInputResult)
			{
				UObject* Outer = UserData;
				UAnimationModifier* Processor = FAnimationModifierHelpers::CreateModifierInstance(Outer, Modifier->GetClass(), Modifier);
				UserData->Modify();
				UserData->AddAnimationModifier(Processor);
			}
		}
	}

	/** For each user data retrieve all modifiers and apply them */
	for (int32 Index = 0; Index < AssetUserData.Num(); ++Index)
	{
		UAnimationModifiersAssetUserData* UserData = AssetUserData[Index];		
		if (UserData)
		{
			UAnimSequence* AnimSequence = AnimSequences[Index];
			AnimSequence->Modify();

			const TArray<UAnimationModifier*>& ModifierInstances = UserData->GetAnimationModifierInstances();
			for (UAnimationModifier* Modifier : ModifierInstances)
			{
				Modifier->ApplyToAnimationSequence(AnimSequence);
			}
		}
	}

	if (bCloseWindow && WidgetWindow.IsValid())
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SAnimationModifierContentBrowserWindow::OnCancel()
{
	if (WidgetWindow.IsValid())
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

bool SAnimationModifierContentBrowserWindow::CanApply() const
{
	return Modifiers.Num() > 0;
}

#undef LOCTEXT_NAMESPACE // "AnimationModifierContentBrowserWindow"