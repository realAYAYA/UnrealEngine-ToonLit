// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "CameraCalibrationToolkit.h"
#include "IStructureDetailsView.h"
#include "LensFile.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "LensDataEditPointDialog"


/**
 * Editing Lens Point Dialog, it opens in separate dialog popup window
 */
template<typename TStructType>
class SLensDataEditPointDialog : public SCompoundWidget
{
public:
	/** Delegate for save struct on scope with specific struct type */
	DECLARE_DELEGATE_OneParam(FOnSave, TSharedPtr<TStructOnScope<TStructType>> /*InStructToEdit */)
	
	SLATE_BEGIN_ARGS(SLensDataEditPointDialog)
	{}
	SLATE_EVENT(FOnSave, OnSave)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, ULensFile* InLensFile, TSharedPtr<TStructOnScope<TStructType>> InStructToEdit)
	{
		StructToEdit = InStructToEdit;
        LensFile = TStrongObjectPtr<ULensFile>(InLensFile);
		OnSaveDelegate = InArgs._OnSave;
    
        const TSharedPtr<SWidget> LensDataWidget = [this]()
        {
        	TSharedPtr<SWidget> WidgetToReturn;
    
        	// In case the struct does not have valid pointer or the data is invalid
        	if (!StructToEdit.IsValid() || !StructToEdit->IsValid())
        	{
        		WidgetToReturn = SNew(STextBlock).Text(LOCTEXT("ErrorEditStruct", "Point can't be edited"));
        	}
        	else
        	{
        		const FStructureDetailsViewArgs StructureViewArgs;
        		FDetailsViewArgs DetailArgs;
        		DetailArgs.bAllowSearch = false;
        		DetailArgs.bShowScrollBar = true;
    
        		FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        		TSharedPtr<IStructureDetailsView> StructureDetailView = PropertyEditor.CreateStructureDetailView(DetailArgs, StructureViewArgs, StructToEdit);
    
        		WidgetToReturn = StructureDetailView->GetWidget();
        	}
        	
        	return WidgetToReturn;
        }();
    
        const TSharedPtr<SWidget> ButtonsWidget = [this]()
        {
        	return SNew(SHorizontalBox)
        		+ SHorizontalBox::Slot()
        		[
        			SNew(SButton)
        			.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
        			.OnClicked(this, &SLensDataEditPointDialog::OnSaveDataPointClicked)
        			.HAlign(HAlign_Center)
        			.Text(LOCTEXT("SaveDataPoint", "Save"))
        		]
        		+ SHorizontalBox::Slot()
        		[
        			SNew(SButton)
        			.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
        			.OnClicked(this, &SLensDataEditPointDialog::OnCancelDataPointClicked)
        			.HAlign(HAlign_Center)
        			.Text(LOCTEXT("CancelEditDataPoint", "Cancel"))
        		];
        }();
    
        ChildSlot
        [
        	SNew(SVerticalBox)
        	+ SVerticalBox::Slot()
        	.Padding(5.0f, 5.0f)
        	.FillHeight(1.f)
        	[
        		SNew(SBorder)
        		.HAlign(HAlign_Fill)
        		.VAlign(VAlign_Fill)
        		.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
        		[
        			LensDataWidget.ToSharedRef()
        		]
        	]
        	+ SVerticalBox::Slot()
        	.Padding(5.0f, 5.0f)
        	.AutoHeight()
        	[
        		SNew(SBorder)
        		.HAlign(HAlign_Fill)
        		.VAlign(VAlign_Fill)
        		.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
        		[
        			ButtonsWidget.ToSharedRef()
        		]
        	]
        ];
	}

private:
	/** Save Button handler */
	FReply OnSaveDataPointClicked() const
	{
		if (StructToEdit.IsValid())
		{
			const FScopedTransaction MapPointEdited(LOCTEXT("MapPointEdited", "Map Point Edited"));
			LensFile->Modify();
		}

		OnSaveDelegate.ExecuteIfBound(StructToEdit);

		FCameraCalibrationToolkit::DestroyPopupWindow();
	
		return FReply::Handled();
	}

	/** Cancel Button handler */
	FReply OnCancelDataPointClicked() const
	{
		FCameraCalibrationToolkit::DestroyPopupWindow();

		return FReply::Handled();
	}

private:
	/** LensFile being edited */
	TStrongObjectPtr<ULensFile> LensFile;

	/** Editing Struct for visualize with StructureDetailsView */ 
	TSharedPtr<TStructOnScope<TStructType>> StructToEdit;

	/** On save struct delegate instance */
	FOnSave OnSaveDelegate;
};

namespace LensDataEditPointDialog
{
	/** Get specific struct for edit, based on Data Table Type */
	template<typename TStructType, typename TableType>
	TSharedPtr<TStructOnScope<TStructType>> GetStructToEdit(const float InFocus, const float InZoom, const TableType& InTable)
	{
		TSharedPtr<TStructOnScope<TStructType>> ReturnStruct = nullptr;
		
		TStructType PointCopy;
		if (InTable.GetPoint(InFocus, InZoom, PointCopy))
		{
			const FStructOnScope StructOnScopeCopy(TStructType::StaticStruct(),reinterpret_cast<uint8*>(&PointCopy));
			ReturnStruct = MakeShared<TStructOnScope<TStructType>>();
			ReturnStruct->InitializeFrom(StructOnScopeCopy);
		}

		return ReturnStruct;
	}

	/** Finds currently opened dialog window or spawns a new one for editing */
	template<typename TStructType, typename TableType>
	static void OpenDialog(ULensFile* InLensFile, const float InFocus, const float InZoom, TableType& InTable)
	{
		using FOnSaveType = typename SLensDataEditPointDialog<TStructType>::FOnSave;
		using FStructType = TSharedPtr<TStructOnScope<TStructType>>;

		// On save struct delegate
		FOnSaveType OnSaveDelegate = FOnSaveType::CreateLambda([InLensFile, InFocus, InZoom, &InTable](FStructType InStructOnScope)
		{
			InTable.SetPoint(InFocus, InZoom, *InStructOnScope->Get());
		});

		FStructType InStructToEdit = GetStructToEdit<TStructType>(InFocus, InZoom, InTable);
		TSharedPtr<SWindow> PopupWindow = FCameraCalibrationToolkit::OpenPopupWindow(LOCTEXT("LensEditorEditPointDialog", "Edit Lens Data Point"));
		PopupWindow->SetContent(SNew(SLensDataEditPointDialog<TStructType>, InLensFile, InStructToEdit).OnSave(OnSaveDelegate));
	}
}

#undef LOCTEXT_NAMESPACE
