// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorDMXLibraryView.h"

#include "Algo/AnyOf.h"
#include "Customizations/DMXControlConsoleDataDetails.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "IDetailsView.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Misc/TransactionObjectEvent.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorFixturePatchVerticalBox.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorDMXLibraryView"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorDMXLibraryView::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		checkf(InEditorModel, TEXT("Invalid control console editor model, can't constuct dmx library view correctly."));
		EditorModel = InEditorModel;

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;

		FPropertyEditorModule& PropertyEditor = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		ControlConsoleDataDetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);

		const FOnGetDetailCustomizationInstance ControlConsoleCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXControlConsoleDataDetails::MakeInstance, EditorModel);
		ControlConsoleDataDetailsView->RegisterInstancedCustomPropertyLayout(UDMXControlConsoleData::StaticClass(), ControlConsoleCustomizationInstance);
		ControlConsoleDataDetailsView->OnFinishedChangingProperties().AddSP(this, &SDMXControlConsoleEditorDMXLibraryView::OnControlConsoleDataPropertyChanged);

		UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		if (ControlConsoleData)
		{
			constexpr bool bForceRefresh = true;
			ControlConsoleDataDetailsView->SetObject(ControlConsoleData, bForceRefresh);
			ControlConsoleData->GetOnDMXLibraryChanged().AddSP(this, &SDMXControlConsoleEditorDMXLibraryView::OnDMXLibraryChanged);
			ControlConsoleData->GetOnDMXLibraryReloaded().AddSP(this, &SDMXControlConsoleEditorDMXLibraryView::OnDMXLibraryReloaded);
		}

		ChildSlot
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)

				+ SScrollBox::Slot()
				.AutoSize()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						ControlConsoleDataDetailsView.ToSharedRef()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(FixturePatchVerticalBox, SDMXControlConsoleEditorFixturePatchVerticalBox, EditorModel.Get())
					]
				]
			];
	}

	bool SDMXControlConsoleEditorDMXLibraryView::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		const TArray<UClass*> MatchingContextClasses =
		{
			UDMXControlConsoleEditorModel::StaticClass(),
			UDMXControlConsoleData::StaticClass(),
			UDMXControlConsoleEditorLayouts::StaticClass(),
			UDMXControlConsoleEditorGlobalLayoutBase::StaticClass(),
			UDMXControlConsoleEditorGlobalLayoutRow::StaticClass()
		};

		const bool bMatchesContext = Algo::AnyOf(TransactionObjectContexts, 
			[this, MatchingContextClasses](const TPair<UObject*, FTransactionObjectEvent>& Pair)
				{
					bool bMatchesClasses = false;
					const UObject* Object = Pair.Key;
					if (IsValid(Object))
					{
						const UClass* ObjectClass = Object->GetClass();
						bMatchesClasses = Algo::AnyOf(MatchingContextClasses, [ObjectClass](UClass* InClass)
							{
								return IsValid(ObjectClass) && ObjectClass->IsChildOf(InClass);
							});
					}

					return bMatchesClasses;
				});

		return bMatchesContext;
	}

	void SDMXControlConsoleEditorDMXLibraryView::PostUndo(bool bSuccess)
	{
		UpdateFixturePatchVerticalBox();
	}

	void SDMXControlConsoleEditorDMXLibraryView::PostRedo(bool bSuccess)
	{
		UpdateFixturePatchVerticalBox();
	}

	void SDMXControlConsoleEditorDMXLibraryView::UpdateFixturePatchVerticalBox()
	{
		if (FixturePatchVerticalBox.IsValid())
		{
			FixturePatchVerticalBox->ForceRefresh();
		}
	}

	void SDMXControlConsoleEditorDMXLibraryView::OnControlConsoleDataPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (PropertyChangedEvent.Property->NamePrivate == UDMXControlConsoleData::GetDMXLibraryPropertyName())
		{
			UpdateFixturePatchVerticalBox();
		}
	}

	void SDMXControlConsoleEditorDMXLibraryView::OnDMXLibraryChanged()
	{
		if (EditorModel.IsValid())
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			SelectionHandler->ClearSelection();
		}

		UpdateFixturePatchVerticalBox();
	}

	void SDMXControlConsoleEditorDMXLibraryView::OnDMXLibraryReloaded()
	{
		if (EditorModel.IsValid())
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			SelectionHandler->RemoveInvalidObjectsFromSelection();
		}

		UpdateFixturePatchVerticalBox();
	}
}

#undef LOCTEXT_NAMESPACE
