// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SNiagaraSystemUserParameters.h"

#include "NiagaraConstants.h"
#include "NiagaraEditorData.h"
#include "NiagaraEditorUtilities.h"
#include "Customizations/NiagaraComponentDetails.h"
#include "Styling/StyleColors.h"
#include "SystemToolkitModes/NiagaraSystemToolkitModeBase.h"
#include "Modules/ModuleManager.h"
#include "ViewModels/NiagaraParameterPanelViewModel.h"
#include "Widgets/SNiagaraParameterMenu.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemUserParameters"

void SNiagaraSystemUserParameters::Construct(const FArguments& InArgs, TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel)
{
	SystemViewModel = InSystemViewModel;
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	TSharedRef<IDetailsView> ObjectDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	ObjectDetailsView->RegisterInstancedCustomPropertyLayout(UNiagaraSystem::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraSystemUserParameterDetails::MakeInstance));
	ObjectDetailsView->SetObject(&InSystemViewModel->GetSystem());
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.HAlign(HAlign_Right)
				.OnClicked(this, &SNiagaraSystemUserParameters::SummonHierarchyEditor)
				.Text(LOCTEXT("SummonUserParametersHierarchyButtonLabel", "Edit Hierarchy"))
			]
		]
		+ SVerticalBox::Slot()
		[
			SNew(SBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("UserParameters")))
			[
				ObjectDetailsView
			]
		]
	];
}

FReply SNiagaraSystemUserParameters::SummonHierarchyEditor()
{
	SystemViewModel.Pin()->FocusTab(FNiagaraSystemToolkitModeBase::UserParametersHierarchyTabID);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
