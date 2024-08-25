// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEditorModule.h"

#include "AnimNextConfig.h"
#include "ISettingsModule.h"
#include "ScopedTransaction.h"
#include "SSimpleButton.h"
#include "SSimpleComboButton.h"
#include "UncookedOnlyUtils.h"
#include "Common/SRigVMAssetView.h"
#include "Framework/Application/SlateApplication.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphPanelNodeFactory.h"
#include "Graph/AnimNextGraph_EdGraphNodeCustomization.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/ParameterBlockParameterCustomization.h"
#include "Param/ParameterPickerArgs.h"
#include "Param/ParametersGraphPanelPinFactory.h"
#include "Param/ParamNamePropertyCustomization.h"
#include "Param/ParamPropertyCustomization.h"
#include "Param/ParamTypePropertyCustomization.h"
#include "Param/SParameterPicker.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Workspace/AnimNextWorkspaceEditor.h"

#define LOCTEXT_NAMESPACE "AnimNextEditorModule"

namespace UE::AnimNext::Editor
{

class FModule : public IModule
{
	virtual void StartupModule() override
	{
		// Register settings for user editing
		ISettingsModule& SettingsModule = FModuleManager::Get().LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings("Editor", "General", "AnimNext",
			LOCTEXT("SettingsName", "AnimNext"),
			LOCTEXT("SettingsDescription", "Customize AnimNext Settings."),
			GetMutableDefault<UAnimNextConfig>()
		);

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"AnimNextParamType",
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamTypePropertyTypeCustomization>(); }));

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"AnimNextParam",
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamPropertyTypeCustomization>(); }));

		Identifier = MakeShared<FParamNamePropertyTypeIdentifier>();
		PropertyModule.RegisterCustomPropertyTypeLayout(
			FNameProperty::StaticClass()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FParamNamePropertyTypeCustomization>(); }),
			Identifier);

		PropertyModule.RegisterCustomClassLayout("AnimNextParameterBlockParameter", 
			FOnGetDetailCustomizationInstance::CreateLambda([] { return MakeShared<FParameterBlockParameterCustomization>(); }));

		PropertyModule.RegisterCustomClassLayout("AnimNextGraph_EdGraphNode",
			FOnGetDetailCustomizationInstance::CreateLambda([] { return MakeShared<FAnimNextGraph_EdGraphNodeCustomization>(); }));

		AnimNextGraphPanelNodeFactory = MakeShared<FAnimNextGraphPanelNodeFactory>();
		FEdGraphUtilities::RegisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		ParametersGraphPanelPinFactory = MakeShared<FParametersGraphPanelPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(ParametersGraphPanelPinFactory);

		FWorkspaceEditor::RegisterAssetDocumentWidget(UAnimNextParameterBlock::StaticClass()->GetFName(), [](TSharedRef<FWorkspaceEditor> InEditor, UObject* InAsset)
		{
			UAnimNextParameterBlock* ParameterBlock = CastChecked<UAnimNextParameterBlock>(InAsset);
			UAnimNextParameterBlock_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(ParameterBlock);
			return SNew(SRigVMAssetView, EditorData)
				.OnSelectionChanged(&InEditor.Get(), &FWorkspaceEditor::SetSelectedObjects)
				.OnOpenGraph(&InEditor.Get(), &FWorkspaceEditor::OnOpenGraph)
				.OnDeleteEntries(&InEditor.Get(), &FWorkspaceEditor::OnDeleteEntries);
		});

		FWorkspaceEditor::RegisterAssetDocumentWidget(UAnimNextSchedule::StaticClass()->GetFName(), [](TSharedRef<FWorkspaceEditor> InEditor, UObject* InAsset)
		{
			UAnimNextSchedule* Schedule = CastChecked<UAnimNextSchedule>(InAsset);
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
			FDetailsViewArgs DetailsViewArgs;
			DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
			DetailsView->SetObject(Schedule);
			return DetailsView;
		});

		FWorkspaceEditor::RegisterAssetDocumentWidget(UAnimNextGraph::StaticClass()->GetFName(), [](TSharedRef<FWorkspaceEditor> InEditor, UObject* InAsset)
		{
			UAnimNextGraph* Graph = CastChecked<UAnimNextGraph>(InAsset);
			UAnimNextGraph_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Graph);

			EditorData->RigVMGraphModifiedEvent.RemoveAll(&InEditor.Get());
			EditorData->RigVMGraphModifiedEvent.AddSP(InEditor, &FWorkspaceEditor::OnGraphModified);

			return SNew(SRigVMAssetView, EditorData)
				.OnSelectionChanged(&InEditor.Get(), &FWorkspaceEditor::SetSelectedObjects)
				.OnOpenGraph(&InEditor.Get(), &FWorkspaceEditor::OnOpenGraph)
				.OnDeleteEntries(&InEditor.Get(), &FWorkspaceEditor::OnDeleteEntries);
		});

		SRigVMAssetView::RegisterCategoryFactory("Parameters", [](UAnimNextRigVMAssetEditorData* InEditorData)
		{
			UAnimNextParameterBlock_EditorData* EditorData = CastChecked<UAnimNextParameterBlock_EditorData>(InEditorData);
			return SNew(SSimpleComboButton)
				.Text(LOCTEXT("AddParameterButton", "Add Parameter"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.HasDownArrow(true)
				.OnGetMenuContent_Lambda([EditorData]()
				{
					FAssetData AssetData(UncookedOnly::FUtils::GetAsset(EditorData));
					
					FParameterPickerArgs Args;
					Args.bMultiSelect = false;
					Args.bShowBlocks = false;
					Args.bShowBoundParameters = false;
					Args.bShowBuiltInParameters = false; // Built-In parameters disabled for MVP
					Args.OnFilterParameter = FOnFilterParameter::CreateLambda([EditorData, AssetData](const FParameterBindingReference& InParameterBinding)
					{
						// Skip params that are already bound in this block
						if(InParameterBinding.Block == AssetData)
						{
							return EFilterParameterResult::Exclude;
						}
						
						return EFilterParameterResult::Include;
					});

					Args.OnAddParameter = FOnAddParameter::CreateLambda([EditorData](const FParameterToAdd& ParameterToAdd)
					{
						FSlateApplication::Get().DismissAllMenus();

						check(EditorData->FindEntry(ParameterToAdd.Name) == nullptr);
						FScopedTransaction Transaction(LOCTEXT("AddParameter", "Add parameter"));
						EditorData->AddParameter(ParameterToAdd.Name, ParameterToAdd.Type);
					});
					Args.OnParameterPicked = FOnParameterPicked::CreateLambda([EditorData](const FParameterBindingReference& InParameterBinding)
					{
						FSlateApplication::Get().DismissAllMenus();

						if (EditorData->FindEntry(InParameterBinding.Parameter) == nullptr)
						{
							const FAnimNextParamType Type = UncookedOnly::FUtils::GetParameterTypeFromName(InParameterBinding.Parameter);
							if (Type.IsValid())
							{
								EditorData->AddParameter(InParameterBinding.Parameter, Type);
							};
						}
					});
					
					return SNew(SParameterPicker)
						.Args(Args);
				});
		});

		SRigVMAssetView::RegisterCategoryFactory("Parameter Graphs", [](UAnimNextRigVMAssetEditorData* InEditorData)
		{
			UAnimNextParameterBlock_EditorData* EditorData = CastChecked<UAnimNextParameterBlock_EditorData>(InEditorData);
			return SNew(SSimpleButton)
				.Text(LOCTEXT("AddGraphButton", "Add Graph"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.OnClicked_Lambda([EditorData]()
				{
					FScopedTransaction Transaction(LOCTEXT("AddGraph", "Add Graph"));

					// Create a new entry for the graph
					EditorData->AddGraph(TEXT("NewGraph"));

					return FReply::Handled();
				});
		});

		SRigVMAssetView::RegisterCategoryFactory("Animation Graphs", [](UAnimNextRigVMAssetEditorData* InEditorData)
		{
			UAnimNextGraph_EditorData* EditorData = CastChecked<UAnimNextGraph_EditorData>(InEditorData);
			return SNew(SSimpleButton)
				.Text(LOCTEXT("AddGraphButton", "Add Graph"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.OnClicked_Lambda([EditorData]()
				{
					FScopedTransaction Transaction(LOCTEXT("AddGraph", "Add Graph"));

					// Create a new entry for the graph
					EditorData->AddGraph(TEXT("NewGraph"));

					return FReply::Handled();
				});
		});
	}

	virtual void ShutdownModule() override
	{
		if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomPropertyTypeLayout("AnimNextParamType");
			PropertyModule.UnregisterCustomPropertyTypeLayout("AnimNextParam");
			PropertyModule.UnregisterCustomPropertyTypeLayout("NameProperty");
			PropertyModule.UnregisterCustomClassLayout("AnimNextParameterBlockParameter");
			PropertyModule.UnregisterCustomClassLayout("AnimNextGraph_EdGraphNode");
		}

		FEdGraphUtilities::UnregisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		FEdGraphUtilities::UnregisterVisualPinFactory(ParametersGraphPanelPinFactory);

		FWorkspaceEditor::UnregisterAssetDocumentWidget("AnimNextSchedule");
		FWorkspaceEditor::UnregisterAssetDocumentWidget("AnimNextParameterBlock");
		FWorkspaceEditor::UnregisterAssetDocumentWidget("AnimNextGraph");
		
		SRigVMAssetView::UnregisterCategoryFactory("Parameters");
		SRigVMAssetView::UnregisterCategoryFactory("Parameter Graphs");
	}

	virtual TSharedRef<SWidget> CreateParameterPicker(const FParameterPickerArgs& InArgs) override
	{
		return SNew(SParameterPicker)
			.Args(InArgs);
	}

	/** Node factory for the AnimNext graph */
	TSharedPtr<FAnimNextGraphPanelNodeFactory> AnimNextGraphPanelNodeFactory;
	
	/** Pin factory for parameters */
	TSharedPtr<FParametersGraphPanelPinFactory> ParametersGraphPanelPinFactory;

	/** Type identifier for parameter names */
	TSharedPtr<FParamNamePropertyTypeIdentifier> Identifier;
};

}

IMPLEMENT_MODULE(UE::AnimNext::Editor::FModule, AnimNextEditor);

#undef LOCTEXT_NAMESPACE