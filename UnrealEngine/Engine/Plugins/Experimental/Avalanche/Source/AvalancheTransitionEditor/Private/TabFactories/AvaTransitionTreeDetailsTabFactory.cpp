// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionTreeDetailsTabFactory.h"
#include "AvaTransitionTreeEditorData.h"
#include "AvaTypeSharedPointer.h"
#include "Customizations/AvaTransitionTreeEditorDataCustomization.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ViewModels/AvaTransitionEditorViewModel.h"
#include "ViewModels/AvaTransitionViewModelSharedData.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "AvaTransitionTreeDetailsTabFactory"

const FName FAvaTransitionTreeDetailsTabFactory::TabId(TEXT("AvaTransitionTreeDetails"));

FAvaTransitionTreeDetailsTabFactory::FAvaTransitionTreeDetailsTabFactory(const TSharedRef<FAvaTransitionEditor>& InEditor)
	: FAvaTransitionTabFactory(TabId, InEditor)
{
	TabIcon             = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details");
	TabLabel            = LOCTEXT("TabLabel", "State Tree");
	ViewMenuTooltip     = LOCTEXT("ViewMenuTooltip", "State Tree Details");
	ViewMenuDescription = LOCTEXT("ViewMenuDescription", "State Tree Details");
	bIsSingleton        = true;
	ReadOnlyBehavior    = ETabReadOnlyBehavior::Custom;
}

TSharedRef<SWidget> FAvaTransitionTreeDetailsTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const
{
	TSharedPtr<FAvaTransitionEditor> Editor = GetEditor();
	if (!ensure(Editor.IsValid()))
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<FAvaTransitionEditorViewModel> EditorViewModel = Editor->GetEditorViewModel();
	if (!ensure(EditorViewModel.IsValid()))
	{
		return SNullWidget::NullWidget;
	}

	UAvaTransitionTreeEditorData* EditorData = EditorViewModel->GetEditorData();
	if (!ensure(EditorData))
	{
		return SNullWidget::NullWidget;
	}

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->RegisterInstancedCustomPropertyLayout(UStateTreeEditorData::StaticClass()
		, FOnGetDetailCustomizationInstance::CreateStatic(&FAvaTransitionTreeEditorDataCustomization::MakeInstance));

	// Read-only
	if (EditorViewModel->GetSharedData()->IsReadOnly())
	{
		DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateLambda([]{ return false; }));
	}

	DetailsView->SetObject(EditorData);

	return DetailsView;
}

#undef LOCTEXT_NAMESPACE
