// Copyright Epic Games, Inc. All Rights Reserved.

#include "SClothPaintTab.h"

#include "ClothPainter.h"
#include "ClothPaintingModule.h"
#include "ClothingAsset.h"
#include "ClothingPaintEditMode.h"
#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "DetailsViewArgs.h"
#include "EditorModeManager.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "IPersonaToolkit.h"
#include "ISkeletalMeshEditor.h"
#include "Layout/Children.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "SClothAssetSelector.h"
#include "SClothPaintWidget.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Layout/SScrollBox.h"

class UObject;
struct FGeometry;

#define LOCTEXT_NAMESPACE "SClothPaintTab"

SClothPaintTab::SClothPaintTab() 
	: bModeApplied(false), bPaintModeEnabled(false)
{	
}

SClothPaintTab::~SClothPaintTab()
{
	if(ISkeletalMeshEditor* SkeletalMeshEditor = static_cast<ISkeletalMeshEditor*>(HostingApp.Pin().Get()))
	{
		SkeletalMeshEditor->GetEditorModeManager().ActivateDefaultMode();
	}
}

void SClothPaintTab::Construct(const FArguments& InArgs)
{
	// Detail view for UClothingAssetCommon
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	DetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	
	// Add delegate for editing enabled, which allows us to show a greyed out version with the CDO
	// selected when we haven't got an asset selected to avoid the UI popping.
	DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &SClothPaintTab::IsAssetDetailsPanelEnabled));

	// Add the CDO by default
	TArray<UObject*> Objects;
	Objects.Add(UClothingAssetCommon::StaticClass()->GetDefaultObject());
	DetailsView->SetObjects(Objects, true);

	HostingApp = InArgs._InHostingApp;

	ModeWidget = nullptr;
	
	FSlateIcon TexturePaintIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.MeshPaintMode.TexturePaint");

	this->ChildSlot
	.Padding(4.f)
	[
		SAssignNew(ContentBox, SScrollBox)
	];

	ISkeletalMeshEditor* SkeletalMeshEditor = static_cast<ISkeletalMeshEditor*>(HostingApp.Pin().Get());

	if(SkeletalMeshEditor)
	{
		IPersonaToolkit& Persona = SkeletalMeshEditor->GetPersonaToolkit().Get();

		ContentBox->AddSlot()
		[
			SAssignNew(SelectorWidget, SClothAssetSelector, Persona.GetMesh())
				.OnSelectionChanged(this, &SClothPaintTab::OnAssetSelectionChanged)
		];

		ContentBox->AddSlot()
		[
			DetailsView->AsShared()
		];
	}
}

void SClothPaintTab::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SClothPaintTab::TogglePaintMode()
{
	bPaintModeEnabled = !bPaintModeEnabled;
	UpdatePaintTools();
}

bool SClothPaintTab::IsPaintModeActive() const
{
	return bPaintModeEnabled;
}

void SClothPaintTab::UpdatePaintTools()
{
	if(!HostingApp.IsValid())
	{
		// If we have no valid host, we can't do anything with our editor below, so don't perform a
		// state update on the tool mode.
		return;
	}

	if (bPaintModeEnabled)
	{
		ISkeletalMeshEditor* SkeletalMeshEditor = static_cast<ISkeletalMeshEditor*>(HostingApp.Pin().Get());
		SkeletalMeshEditor->GetEditorModeManager().ActivateMode(PaintModeID, true);

		FClothingPaintEditMode* PaintMode = (FClothingPaintEditMode*)SkeletalMeshEditor->GetEditorModeManager().GetActiveMode(PaintModeID);
		if (PaintMode)
		{
			FClothPainter* ClothPainter = static_cast<FClothPainter*>(PaintMode->GetMeshPainter());
			check(ClothPainter);

			ClothPainter->Reset();
			ModeWidget = StaticCastSharedPtr<SClothPaintWidget>(ClothPainter->GetWidget());
			PaintMode->SetPersonaToolKit(SkeletalMeshEditor->GetPersonaToolkit());

			ContentBox->AddSlot()
			[
				ModeWidget->AsShared()
			];

			if(SelectorWidget.IsValid())
			{
				TWeakObjectPtr<UClothingAssetCommon> WeakAsset = SelectorWidget->GetSelectedAsset();

				if(WeakAsset.Get())
				{
					ClothPainter->OnAssetSelectionChanged(WeakAsset.Get(), SelectorWidget->GetSelectedLod(), SelectorWidget->GetSelectedMask());
				}
			}
		}
	}
	else
	{
		ContentBox->RemoveSlot(ModeWidget->AsShared());
		ISkeletalMeshEditor* SkeletalMeshEditor = static_cast<ISkeletalMeshEditor*>(HostingApp.Pin().Get());
		SkeletalMeshEditor->GetEditorModeManager().ActivateDefaultMode();
		ModeWidget = nullptr;
	}
}

void SClothPaintTab::OnAssetSelectionChanged(TWeakObjectPtr<UClothingAssetCommon> InAssetPtr, int32 InLodIndex, int32 InMaskIndex)
{
	if(bPaintModeEnabled)
	{
		ISkeletalMeshEditor* SkeletalMeshEditor = static_cast<ISkeletalMeshEditor*>(HostingApp.Pin().Get());

		FClothingPaintEditMode* PaintMode = (FClothingPaintEditMode*)SkeletalMeshEditor->GetEditorModeManager().GetActiveMode(PaintModeID);
		if(PaintMode)
		{
			FClothPainter* ClothPainter = static_cast<FClothPainter*>(PaintMode->GetMeshPainter());

			if(ClothPainter)
			{
				ClothPainter->OnAssetSelectionChanged(InAssetPtr.Get(), InLodIndex, InMaskIndex);
			}
		}
	}

	if(UClothingAssetCommon* Asset = InAssetPtr.Get())
	{
		TArray<UObject*> Objects;

		Objects.Add(Asset);

		DetailsView->SetObjects(Objects, true);
	}
}

bool SClothPaintTab::IsAssetDetailsPanelEnabled()
{
	// Only enable editing if we have a valid details panel that is not observing the CDO
	if(DetailsView.IsValid())
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailsView->GetSelectedObjects();

		if(SelectedObjects.Num() > 0)
		{
			return SelectedObjects[0].Get() != UClothingAssetCommon::StaticClass()->GetDefaultObject();
		}
	}

	return false;
}

TSharedRef<IPersonaToolkit> SClothPaintTab::GetPersonaToolkit() const
{
	return GetSkeletalMeshEditor()->GetPersonaToolkit();
}

ISkeletalMeshEditor* SClothPaintTab::GetSkeletalMeshEditor() const
{
	ISkeletalMeshEditor* Editor = static_cast<ISkeletalMeshEditor*>(HostingApp.Pin().Get());
	check(Editor);

	return Editor;
}

#undef LOCTEXT_NAMESPACE //"SClothPaintTab"
