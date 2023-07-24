// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLensDataViewer.h"

#include "SCameraCalibrationCurveEditorPanel.h"

#include "CameraCalibrationCurveEditor.h"
#include "CameraCalibrationSettings.h"
#include "CameraCalibrationStepsController.h"
#include "CameraCalibrationTimeSliderController.h"
#include "CurveEditorSettings.h"
#include "ICurveEditorModule.h"
#include "ISinglePropertyView.h"
#include "LensFile.h"
#include "RichCurveEditorModel.h"
#include "SLensDataAddPointDialog.h"
#include "SLensDataCategoryListItem.h"
#include "SLensDataListItem.h"

#include "Curves/LensDataCurveModel.h"
#include "Curves/LensDistortionParametersCurveModel.h"
#include "Curves/LensEncodersCurveModel.h"
#include "Curves/LensFocalLengthCurveModel.h"
#include "Curves/LensImageCenterCurveModel.h"
#include "Curves/LensNodalOffsetCurveModel.h"
#include "Curves/LensSTMapCurveModel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "LensDataViewer"

namespace LensDataUtils
{
	const FName EncoderCategoryLabel(TEXT("Encoders"));
	const FName EncoderFocusLabel(TEXT("Focus"));
	const FName EncoderIrisLabel(TEXT("Iris"));
	const FName EncoderZoomLabel(TEXT("Focal Length"));
	const FName DistortionCategoryLabel(TEXT("Distortion"));
	const FName FxLabel(TEXT("Fx"));
	const FName FyLabel(TEXT("Fy"));
	const FName MapsCategoryLabel(TEXT("STMaps"));
	const FName ImageCenterCategory(TEXT("Image Center"));
	const FName CxLabel(TEXT("Cx"));
	const FName CyLabel(TEXT("Cy"));
	const FName NodalOffsetCategoryLabel(TEXT("Nodal Offset"));
	const FName LocationXLabel(TEXT("Location - X"));
	const FName LocationYLabel(TEXT("Location - Y"));
	const FName LocationZLabel(TEXT("Location - Z"));
	const FName RotationXLabel(TEXT("Yaw"));
	const FName RotationYLabel(TEXT("Pitch"));
	const FName RotationZLabel(TEXT("Roll"));

	template<typename TFocusPoint>
	void MakeFocusEntries(ULensFile* InLensFile, ELensDataCategory InCategory, int32 InSubCategoryIndex, TConstArrayView<TFocusPoint> FocusPoints, TArray<TSharedPtr<FLensDataListItem>>& OutDataItems, FOnDataRemoved InDataRemovedCallback)
	{
		OutDataItems.Reserve(FocusPoints.Num());
		for (const TFocusPoint& Point : FocusPoints)
		{
			//Add entry for focus
			TSharedPtr<FFocusDataListItem> CurrentFocus = MakeShared<FFocusDataListItem>(InLensFile, InCategory, InSubCategoryIndex, Point.Focus, InDataRemovedCallback);
			OutDataItems.Add(CurrentFocus);

			for(int32 Index = 0; Index < Point.GetNumPoints(); ++Index)
			{
				//Add zoom points for this focus
				TSharedPtr<FZoomDataListItem> ZoomItem = MakeShared<FZoomDataListItem>(InLensFile, InCategory, InSubCategoryIndex, CurrentFocus.ToSharedRef(),Point.GetZoom(Index), InDataRemovedCallback);
				CurrentFocus->Children.Add(ZoomItem);
			}
		}
	}
}

/**
 * Custom curve bounds based on live input
 */
class FCameraCalibrationCurveEditorBounds : public ICurveEditorBounds
{
public:
	FCameraCalibrationCurveEditorBounds(TSharedPtr<ITimeSliderController> InExternalTimeSliderController)
		: TimeSliderControllerWeakPtr(InExternalTimeSliderController)
	{}

	virtual void GetInputBounds(double& OutMin, double& OutMax) const override
	{
		if (const TSharedPtr<ITimeSliderController> ExternalTimeSliderController = TimeSliderControllerWeakPtr.Pin())
		{
			const FAnimatedRange ViewRange = ExternalTimeSliderController->GetViewRange();
			OutMin = ViewRange.GetLowerBoundValue();
			OutMax = ViewRange.GetUpperBoundValue();
		}
	}

	virtual void SetInputBounds(double InMin, double InMax) override
	{
		if (const TSharedPtr<ITimeSliderController> ExternalTimeSliderController = TimeSliderControllerWeakPtr.Pin())
		{
			ExternalTimeSliderController->SetViewRange(InMin, InMax, EViewRangeInterpolation::Immediate);
		}
	}

	TWeakPtr<ITimeSliderController> TimeSliderControllerWeakPtr;
};

void SLensDataViewer::Construct(const FArguments& InArgs, ULensFile* InLensFile, const TSharedRef<FCameraCalibrationStepsController>& InCalibrationStepsController)
{
	const UCameraCalibrationEditorSettings* EditorSettings = GetDefault<UCameraCalibrationEditorSettings>();
	
	LensFile = TStrongObjectPtr<ULensFile>(InLensFile);
		
	//Setup curve editor
	CurveEditor = MakeShared<FCameraCalibrationCurveEditor>();
	const FCurveEditorInitParams InitParams;
	CurveEditor->InitCurveEditor(InitParams);
	CurveEditor->GridLineLabelFormatXAttribute = LOCTEXT("GridXLabelFormat", "{0}");
	
	TUniquePtr<ICurveEditorBounds> EditorBounds;

	// We need to keep Time Slider outside the scope in order to be valid whe it passed to CurveEditorPanel
	TSharedPtr<FCameraCalibrationTimeSliderController> TimeSliderController;
	if (EditorSettings->bEnableTimeSlider)
	{
		TimeSliderController = MakeShared<FCameraCalibrationTimeSliderController>(InCalibrationStepsController, InLensFile);
		TimeSliderControllerWeakPtr = TimeSliderController;
		EditorBounds = MakeUnique<FCameraCalibrationCurveEditorBounds>(TimeSliderController);
	}
	else
	{
		EditorBounds = MakeUnique<FStaticCurveEditorBounds>();
		EditorBounds->SetInputBounds(0.05, 1.05);
	}
	CurveEditor->SetBounds(MoveTemp(EditorBounds));

	// Set zoom as mouse zoom by default
	check(CurveEditor->GetSettings()); // Should be valid all the time
	CurveEditor->GetSettings()->SetZoomPosition(ECurveEditorZoomPosition::MousePosition);

	// Set Delegates
	CurveEditor->OnAddDataPointDelegate.BindSP(this, &SLensDataViewer::OnAddDataPointHandler);

	// Snap only Y axis
	FCurveEditorAxisSnap SnapYAxisOnly = CurveEditor->GetAxisSnap();
	SnapYAxisOnly.RestrictedAxisList = EAxisList::Type::Y;
	CurveEditor->SetAxisSnap(SnapYAxisOnly);

	CurvePanel = SNew(SCameraCalibrationCurveEditorPanel, CurveEditor.ToSharedRef(), TimeSliderController);
	CurveEditor->ZoomToFit();

	CachedFIZ = InArgs._CachedFIZData;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeToolbarWidget(CurvePanel.ToSharedRef())
		]
		+ SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			[
				MakeLensDataWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			[
				CurvePanel.ToSharedRef()
			]
		]
	];

	Refresh();
}

TSharedPtr<FLensDataCategoryItem> SLensDataViewer::GetDataCategorySelection() const
{
	TArray<TSharedPtr<FLensDataCategoryItem>> SelectedNodes;
	TreeView->GetSelectedItems(SelectedNodes);
	if (SelectedNodes.Num())
	{
		return SelectedNodes[0];
	}
	return nullptr;
}

TSharedRef<ITableRow> SLensDataViewer::OnGenerateDataCategoryRow(TSharedPtr<FLensDataCategoryItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return Item->MakeTreeRowWidget(OwnerTable);
}

void SLensDataViewer::OnGetDataCategoryItemChildren(TSharedPtr<FLensDataCategoryItem> Item, TArray<TSharedPtr<FLensDataCategoryItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SLensDataViewer::OnDataCategorySelectionChanged(TSharedPtr<FLensDataCategoryItem> Item, ESelectInfo::Type SelectInfo)
{
	//Don't filter based on SelectInfo. We want to update on arrow key usage
	RefreshDataEntriesTree();
}

TSharedPtr<FLensDataListItem> SLensDataViewer::GetSelectedDataEntry() const
{
	TArray<TSharedPtr<FLensDataListItem>> SelectedNodes;
	DataEntriesTree->GetSelectedItems(SelectedNodes);
	if (SelectedNodes.Num())
	{
		return SelectedNodes[0];
	}
	return nullptr;
}

TSharedRef<ITableRow> SLensDataViewer::OnGenerateDataEntryRow(TSharedPtr<FLensDataListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return Item->MakeTreeRowWidget(OwnerTable);
}

void SLensDataViewer::OnGetDataEntryChildren(TSharedPtr<FLensDataListItem> Item, TArray<TSharedPtr<FLensDataListItem>>& OutItems)
{
	if (Item.IsValid())
	{
		OutItems = Item->Children;
	}
}

void SLensDataViewer::OnDataEntrySelectionChanged(TSharedPtr<FLensDataListItem> Node, ESelectInfo::Type SelectInfo)
{
	RefreshCurve();

	RefreshTimeSlider();
}

void SLensDataViewer::PostUndo(bool bSuccess)
{
	//Items in category could have changed
	RefreshDataEntriesTree();	
}

void SLensDataViewer::PostRedo(bool bSuccess)
{
	//Items in category could have changed
	RefreshDataEntriesTree();
}

TSharedRef<SWidget> SLensDataViewer::MakeLensDataWidget()
{
	const FSinglePropertyParams InitParams;
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const TSharedPtr<ISinglePropertyView> DataModeWidget = PropertyEditorModule.CreateSingleProperty(
		LensFile.Get()
		, GET_MEMBER_NAME_CHECKED(ULensFile, DataMode)
		, InitParams);

	FSimpleDelegate OnDataModeChangedDelegate = FSimpleDelegate::CreateSP(this, &SLensDataViewer::OnDataModeChanged);
	DataModeWidget->SetOnPropertyValueChanged(OnDataModeChangedDelegate);

	return 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(5.0f, 5.0f)
		.AutoHeight()
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
			[
				DataModeWidget.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot()
		.Padding(5.0f, 5.0f)
		.FillHeight(.5f)
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FLensDataCategoryItem>>)
				.TreeItemsSource(&DataCategories)
				.ItemHeight(24.0f)
				.OnGenerateRow(this, &SLensDataViewer::OnGenerateDataCategoryRow)
				.OnGetChildren(this, &SLensDataViewer::OnGetDataCategoryItemChildren)
				.OnSelectionChanged(this, &SLensDataViewer::OnDataCategorySelectionChanged)
				.ClearSelectionOnClick(false)
			]
			
		]
		+ SVerticalBox::Slot()
		.Padding(5.0f, 5.0f)
		.FillHeight(.5f)
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if(const TSharedPtr<FLensDataCategoryItem> CategoryItem = GetDataCategorySelection())
						{
							return FText::FromName(CategoryItem->Label);
						}
						else
						{
							return LOCTEXT("NoCategorySelected", "Select a category");
						}
					})
				]
			+ SVerticalBox::Slot()
			[
					SAssignNew(DataEntriesTree, STreeView<TSharedPtr<FLensDataListItem>>)
					.TreeItemsSource(&DataEntries)
					.ItemHeight(24.0f)
					.OnGenerateRow(this, &SLensDataViewer::OnGenerateDataEntryRow)
					.OnGetChildren(this, &SLensDataViewer::OnGetDataEntryChildren)
					.OnSelectionChanged(this, &SLensDataViewer::OnDataEntrySelectionChanged)
					.ClearSelectionOnClick(false)
				]
			]
		];
}

TSharedRef<SWidget> SLensDataViewer::MakeToolbarWidget(TSharedRef<SCameraCalibrationCurveEditorPanel> InEditorPanel)
{
	// Curve toolbar
	FToolBarBuilder ToolBarBuilder(CurvePanel->GetCommands(), FMultiBoxCustomization::None, CurvePanel->GetToolbarExtender(), true);
	ToolBarBuilder.BeginSection("Asset");
	ToolBarBuilder.BeginStyleOverride("AssetEditorToolbar");

	TSharedRef<SWidget> AddPointButton =
			 SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "FlatButton")
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("AddLensDataPoint", "Add a lens data point"))
			.OnClicked_Lambda([this]()
			{
				OnAddDataPointHandler();
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
			];

	TSharedRef<SWidget> ClearAllButton =
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "FlatButton")
			.VAlign(VAlign_Center)
			.ToolTipText(LOCTEXT("DeleteLensData", "Delete all calibrated lens data"))
			.OnClicked(this, &SLensDataViewer::OnClearLensFileClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
			];

	ToolBarBuilder.AddSeparator();
	ToolBarBuilder.AddWidget(AddPointButton);
	ToolBarBuilder.AddSeparator();
	ToolBarBuilder.AddWidget(ClearAllButton);
		
	ToolBarBuilder.EndSection();

	return SNew(SBox)
		.Padding(FMargin(2.f, 0.f))
		[
			ToolBarBuilder.MakeWidget()
		];
}

void SLensDataViewer::OnAddDataPointHandler()
{
	const FSimpleDelegate OnDataPointAdded = FSimpleDelegate::CreateSP(this, &SLensDataViewer::OnLensDataPointAdded);

	ELensDataCategory InitialCategory = ELensDataCategory::Distortion;
	if (TSharedPtr<FLensDataCategoryItem> CategoryItem = GetDataCategorySelection())
	{
		InitialCategory = CategoryItem->Category;
	}

	SLensDataAddPointDialog::OpenDialog(LensFile.Get(), InitialCategory, CachedFIZ, OnDataPointAdded);
}

FReply SLensDataViewer::OnClearLensFileClicked()
{
	FScopedTransaction Transaction(LOCTEXT("LensFileClearAll", "Cleared LensFile"));
	LensFile->Modify();

	//Warn the user that they are about to clear everything
	const FText Message = LOCTEXT("ClearAllWarning", "This will erase all data contained in this LensFile. Do you wish to continue?");
	if (FMessageDialog::Open(EAppMsgType::OkCancel, Message) != EAppReturnType::Ok)
	{
		Transaction.Cancel();
		return FReply::Handled();
	}

	LensFile->ClearAll();
	RefreshDataEntriesTree();

	return FReply::Handled();
}

void SLensDataViewer::OnDataModeChanged()
{
	Refresh();
}

void SLensDataViewer::Refresh()
{
	RefreshDataCategoriesTree();
	RefreshDataEntriesTree();
}

void SLensDataViewer::RefreshDataCategoriesTree()
{
	//Builds the data category tree
	
	DataCategories.Reset();

	DataCategories.Add(MakeShared<FLensDataCategoryItem>(LensFile.Get(), nullptr, ELensDataCategory::Focus,  LensDataUtils::EncoderFocusLabel));
	DataCategories.Add(MakeShared<FLensDataCategoryItem>(LensFile.Get(), nullptr, ELensDataCategory::Iris, LensDataUtils::EncoderIrisLabel));

	TSharedPtr<FLensDataCategoryItem> FocalLengthCategory = MakeShared<FLensDataCategoryItem>(LensFile.Get(), nullptr, ELensDataCategory::Zoom, LensDataUtils::EncoderZoomLabel);
	FocalLengthCategory->Children.Add(MakeShared<FFocalLengthCategoryItem>(LensFile.Get(), FocalLengthCategory, ELensDataCategory::Zoom, LensDataUtils::FxLabel, 0));
	FocalLengthCategory->Children.Add(MakeShared<FFocalLengthCategoryItem>(LensFile.Get(), FocalLengthCategory, ELensDataCategory::Zoom, LensDataUtils::FyLabel, 1));
	DataCategories.Add(FocalLengthCategory);

	
	if (LensFile->DataMode == ELensDataMode::Parameters)
	{
		TSharedPtr<FLensDataCategoryItem> DistortionEntry = MakeShared<FLensDataCategoryItem>(LensFile.Get(), nullptr, ELensDataCategory::Distortion, LensDataUtils::DistortionCategoryLabel);
		DataCategories.Add(DistortionEntry);

		TArray<FText> Parameters;
		if (LensFile->LensInfo.LensModel)
		{
			Parameters = LensFile->LensInfo.LensModel.GetDefaultObject()->GetParameterDisplayNames();
		}

		for (int32 Index = 0; Index< Parameters.Num(); ++Index)
		{
			const FText& Parameter = Parameters[Index];
			DistortionEntry->Children.Add(MakeShared<FDistortionParametersCategoryItem>(LensFile.Get(), DistortionEntry, ELensDataCategory::Distortion, *Parameter.ToString(), Index));
		}
	}
	else
	{
		DataCategories.Add(MakeShared<FLensDataCategoryItem>(LensFile.Get(), nullptr, ELensDataCategory::STMap, LensDataUtils::MapsCategoryLabel));
	}

	TSharedPtr<FLensDataCategoryItem> ImageCenterEntry = MakeShared<FLensDataCategoryItem>(LensFile.Get(), nullptr, ELensDataCategory::ImageCenter, LensDataUtils::ImageCenterCategory);
	DataCategories.Add(ImageCenterEntry);
	ImageCenterEntry->Children.Add(MakeShared<FImageCenterCategoryItem>(LensFile.Get(), ImageCenterEntry, ELensDataCategory::ImageCenter, LensDataUtils::CxLabel, 0));
	ImageCenterEntry->Children.Add(MakeShared<FImageCenterCategoryItem>(LensFile.Get(), ImageCenterEntry, ELensDataCategory::ImageCenter, LensDataUtils::CyLabel, 1));

	TSharedPtr<FLensDataCategoryItem> NodalOffsetCategory = MakeShared<FLensDataCategoryItem>(LensFile.Get(), nullptr, ELensDataCategory::NodalOffset, LensDataUtils::NodalOffsetCategoryLabel);
	DataCategories.Add(NodalOffsetCategory);

	NodalOffsetCategory->Children.Add(MakeShared<FNodalOffsetCategoryItem>(LensFile.Get(), NodalOffsetCategory, ELensDataCategory::NodalOffset, LensDataUtils::LocationXLabel, 0, EAxis::X));
	NodalOffsetCategory->Children.Add(MakeShared<FNodalOffsetCategoryItem>(LensFile.Get(), NodalOffsetCategory, ELensDataCategory::NodalOffset, LensDataUtils::LocationYLabel, 0, EAxis::Y));
	NodalOffsetCategory->Children.Add(MakeShared<FNodalOffsetCategoryItem>(LensFile.Get(), NodalOffsetCategory, ELensDataCategory::NodalOffset, LensDataUtils::LocationZLabel, 0, EAxis::Z));
	NodalOffsetCategory->Children.Add(MakeShared<FNodalOffsetCategoryItem>(LensFile.Get(), NodalOffsetCategory, ELensDataCategory::NodalOffset, LensDataUtils::RotationXLabel, 1, EAxis::X));
	NodalOffsetCategory->Children.Add(MakeShared<FNodalOffsetCategoryItem>(LensFile.Get(), NodalOffsetCategory, ELensDataCategory::NodalOffset, LensDataUtils::RotationYLabel, 1, EAxis::Y));
	NodalOffsetCategory->Children.Add(MakeShared<FNodalOffsetCategoryItem>(LensFile.Get(), NodalOffsetCategory, ELensDataCategory::NodalOffset, LensDataUtils::RotationZLabel, 1, EAxis::Z));

	TreeView->RequestTreeRefresh();
}

void SLensDataViewer::RefreshDataEntriesTree()
{
	TSharedPtr<FLensDataListItem> CurrentSelection = GetSelectedDataEntry();

	DataEntries.Reset();

	if (TSharedPtr<FLensDataCategoryItem> CategoryItem = GetDataCategorySelection())
	{
		FOnDataRemoved DataRemovedCallback = FOnDataRemoved::CreateSP(this, &SLensDataViewer::OnDataPointRemoved);

		switch (CategoryItem->Category)
		{
			case ELensDataCategory::Focus:
			{
				for (int32 Index = 0; Index <LensFile->EncodersTable.GetNumFocusPoints(); ++Index)
				{
					DataEntries.Add(MakeShared<FEncoderDataListItem>(LensFile.Get(), CategoryItem->Category, LensFile->EncodersTable.GetFocusInput(Index), Index, DataRemovedCallback));
				}
				break;
			}
			case ELensDataCategory::Iris:
			{
				for (int32 Index = 0; Index <LensFile->EncodersTable.GetNumIrisPoints(); ++Index)
				{
					DataEntries.Add(MakeShared<FEncoderDataListItem>(LensFile.Get(), CategoryItem->Category, LensFile->EncodersTable.GetIrisInput(Index), Index, DataRemovedCallback));
				}
				break;
			}
			case ELensDataCategory::Zoom:
			{
				const TConstArrayView<FFocalLengthFocusPoint> FocusPoints = LensFile->FocalLengthTable.GetFocusPoints();
				LensDataUtils::MakeFocusEntries(LensFile.Get(), CategoryItem->Category, CategoryItem->GetParameterIndex(), FocusPoints, DataEntries, DataRemovedCallback);
				break;
			}
			case ELensDataCategory::Distortion:
			{
				const TConstArrayView<FDistortionFocusPoint> FocusPoints = LensFile->DistortionTable.GetFocusPoints();
				LensDataUtils::MakeFocusEntries(LensFile.Get(), CategoryItem->Category, CategoryItem->GetParameterIndex(), FocusPoints, DataEntries, DataRemovedCallback);
				break;
			}
			case ELensDataCategory::ImageCenter:
			{
				const TConstArrayView<FImageCenterFocusPoint> FocusPoints = LensFile->ImageCenterTable.GetFocusPoints();
				LensDataUtils::MakeFocusEntries(LensFile.Get(), CategoryItem->Category, CategoryItem->GetParameterIndex(), FocusPoints, DataEntries, DataRemovedCallback);
				break;
			}
			case ELensDataCategory::NodalOffset:
			{
				const TConstArrayView<FNodalOffsetFocusPoint> Points = LensFile->NodalOffsetTable.GetFocusPoints();
				LensDataUtils::MakeFocusEntries(LensFile.Get(), CategoryItem->Category, CategoryItem->GetParameterIndex(), Points, DataEntries, DataRemovedCallback);
				break;
			}
			case ELensDataCategory::STMap:
			{
				const TConstArrayView<FSTMapFocusPoint> Points = LensFile->STMapTable.GetFocusPoints();
				LensDataUtils::MakeFocusEntries(LensFile.Get(), CategoryItem->Category, CategoryItem->GetParameterIndex(), Points, DataEntries, DataRemovedCallback);
				break;
			}
		}
	}

	//When data entries have been repopulated, refresh the tree and select first item
	DataEntriesTree->RequestListRefresh();

	//Try to put back the same selected Focus/Zoom item
	UpdateDataSelection(CurrentSelection);
}

void SLensDataViewer::RefreshCurve() const
{
	CurveEditor->RemoveAllCurves();
	TUniquePtr<FLensDataCurveModel> NewCurve;

	TSharedPtr<FLensDataCategoryItem> CategoryItem = GetDataCategorySelection();
	if (CategoryItem.IsValid())
	{
		const TSharedPtr<FLensDataListItem> CurrentDataItem = GetSelectedDataEntry();
		switch (CategoryItem->Category)
		{
			case ELensDataCategory::Focus:
			{
				NewCurve = MakeUnique<FLensEncodersCurveModel>(LensFile.Get(), EEncoderType::Focus);
				break;
			}
			case ELensDataCategory::Iris:
			{
				NewCurve = MakeUnique<FLensEncodersCurveModel>(LensFile.Get(), EEncoderType::Iris);
				break;
			}
			case ELensDataCategory::Zoom:
			{
				if (CurrentDataItem)
				{
					const TOptional<float> Focus = CurrentDataItem->GetFocus();
					if(Focus.IsSet())
					{
						const int32 ParameterIndex = CategoryItem->GetParameterIndex();
						NewCurve = MakeUnique<FLensFocalLengthCurveModel>(LensFile.Get(), Focus.GetValue(), ParameterIndex);
					}
				}
				break;
			}
			case ELensDataCategory::Distortion:
			{
				if (CurrentDataItem)
				{
					const TOptional<float> Focus = CurrentDataItem->GetFocus();
					if(Focus.IsSet())
					{
						const int32 ParameterIndex = CategoryItem->GetParameterIndex();
						NewCurve = MakeUnique<FLensDistortionParametersCurveModel>(LensFile.Get(), Focus.GetValue(), ParameterIndex);
					}
				}
				break;
			}
			case ELensDataCategory::ImageCenter:
			{
				if (CurrentDataItem)
				{
					const TOptional<float> Focus = CurrentDataItem->GetFocus();
					const int32 ParameterIndex = CategoryItem->GetParameterIndex();
					if(Focus.IsSet() && ParameterIndex != INDEX_NONE)
					{
						NewCurve = MakeUnique<FLensImageCenterCurveModel>(LensFile.Get(), Focus.GetValue(), ParameterIndex);
					}
				}
				break;
			}
			case ELensDataCategory::NodalOffset:
			{
				if (CurrentDataItem)
				{
					TSharedPtr<FNodalOffsetCategoryItem> NodalCategory = StaticCastSharedPtr<FNodalOffsetCategoryItem>(CategoryItem);
					const TOptional<float> Focus = CurrentDataItem->GetFocus();
					const int32 ParameterIndex = CategoryItem->GetParameterIndex();
					if(Focus.IsSet() && ParameterIndex != INDEX_NONE)
					{
						NewCurve = MakeUnique<FLensNodalOffsetCurveModel>(LensFile.Get(), Focus.GetValue(), ParameterIndex, NodalCategory->Axis);
					}
				}
				break;
			}
			case ELensDataCategory::STMap:
			{
					if (CurrentDataItem)
					{
						const TOptional<float> Focus = CurrentDataItem->GetFocus();
						if(Focus.IsSet())
						{
							NewCurve = MakeUnique<FLensSTMapCurveModel>(LensFile.Get(), Focus.GetValue());
						}
					}
				break;
			}
			default:
			{
				break;
			}
		}
	}

	//If a curve was setup, add it to the editor
	if (NewCurve)
	{
		NewCurve->SetShortDisplayName(FText::FromName(CategoryItem->Label));
		const UCameraCalibrationEditorSettings* EditorSettings = GetDefault<UCameraCalibrationEditorSettings>();
		NewCurve->SetColor(EditorSettings->CategoryColor.GetColorForCategory(CategoryItem->Category));
		const FCurveModelID CurveId = CurveEditor->AddCurve(MoveTemp(NewCurve));
		CurveEditor->PinCurve(CurveId);
	}
}

void SLensDataViewer::RefreshTimeSlider() const
{
	const TSharedPtr<FLensDataCategoryItem> CategoryItem = GetDataCategorySelection();
	const TSharedPtr<FLensDataListItem> CurrentDataItem = GetSelectedDataEntry();
	const TSharedPtr<FCameraCalibrationTimeSliderController> TimeSliderController = TimeSliderControllerWeakPtr.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}
	if (!CurrentDataItem.IsValid() || !CategoryItem.IsValid())
	{
		TimeSliderController->ResetSelection();
		return;
	}

	const bool bHasParameterIndex = CategoryItem->Category == ELensDataCategory::ImageCenter || CategoryItem->Category == ELensDataCategory::NodalOffset;
	const int32 ParameterIndex = CategoryItem->GetParameterIndex();

	// Reset selection if no selection of the curve for ImageCenter or NodalOffset
	if (bHasParameterIndex && ParameterIndex == INDEX_NONE)
	{
		TimeSliderController->ResetSelection();
	}
	else
	{
		TimeSliderController->UpdateSelection(CurrentDataItem->Category, CurrentDataItem->GetFocus());
	}
}

void SLensDataViewer::OnLensDataPointAdded()
{
	RefreshDataEntriesTree();
}

void SLensDataViewer::OnDataPointRemoved(float InFocus, TOptional<float> InZoom)
{
	RefreshDataEntriesTree();
}

void SLensDataViewer::OnDataTablePointsUpdated(ELensDataCategory InCategory)
{
	if (TSharedPtr<FLensDataCategoryItem> CategoryItem = GetDataCategorySelection())
	{
		if (CategoryItem->Category == InCategory)
		{
			RefreshDataEntriesTree();
		}
	}
}

void SLensDataViewer::UpdateDataSelection(const TSharedPtr<FLensDataListItem>& PreviousSelection)
{
	if (PreviousSelection.IsValid())
	{
		const TOptional<float> FocusValue = PreviousSelection->GetFocus();
		if(FocusValue.IsSet())
		{
			for(const TSharedPtr<FLensDataListItem>& Item : DataEntries)
			{
				if(Item->GetFocus() == FocusValue)
				{
					DataEntriesTree->SetSelection(Item);
					return;
				}
			}
		}
	}

	//If we haven't found a selection
	if (DataEntries.Num())
	{
		DataEntriesTree->SetSelection(DataEntries[0]);
	}
	else
	{
		DataEntriesTree->SetSelection(nullptr);
	}
}

#undef LOCTEXT_NAMESPACE /* LensDataViewer */


