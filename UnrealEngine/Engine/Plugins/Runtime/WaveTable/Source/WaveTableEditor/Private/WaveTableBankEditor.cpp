// Copyright Epic Games, Inc. All Rights Reserved.
#include "WaveTableBankEditor.h"

#include "CommonFrameRates.h"
#include "Containers/Set.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "CurveEditor.h"
#include "CurveEditorCommands.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "RichCurveEditorModel.h"
#include "SCurveEditorPanel.h"
#include "Tree/SCurveEditorTree.h"
#include "Tree/ICurveEditorTreeItem.h"
#include "Tree/SCurveEditorTreePin.h"
#include "WaveTableBank.h"
#include "WaveTableSampler.h"
#include "WaveTableSettings.h"
#include "WaveTableTransform.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SFrameRatePicker.h"


#define LOCTEXT_NAMESPACE "WaveTableEditor"



namespace WaveTable
{
	namespace Editor
	{
		namespace BankEditorPrivate
		{
			// Two seconds of samples at 48kHz
			constexpr int32 MaxLiveEditSamples = 48000 * 2;

			int32 GetDrawPointCount(EWaveTableResolution Resolution, const FWaveTableTransform& InTransform)
			{
				// Min/max number of curve points for mathematical
				// expressions/files drawn on curve stack view.
				static const FVector2D ExpressionCurvePointCountLimits = { 2, 4096 };

				float WaveTableSize = WaveTable::GetWaveTableSize(Resolution, InTransform.Curve);
				return FMath::Clamp(WaveTableSize, ExpressionCurvePointCountLimits.X, ExpressionCurvePointCountLimits.Y);
			}
		} // namespace BankEditorPrivate

		const FName FBankEditorBase::AppIdentifier(TEXT("WaveTableEditorApp"));
		const FName FBankEditorBase::CurveTabId(TEXT("WaveTableEditor_Curves"));
		const FName FBankEditorBase::PropertiesTabId(TEXT("WaveTableEditor_Properties"));

		FBankEditorBase::FBankEditorBase()
		{
		}

		void FBankEditorBase::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
		{
			WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_WaveTableEditor", "WaveTable Editor"));

			FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

			InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FBankEditorBase::SpawnTab_Properties))
				.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
				.SetGroup(WorkspaceMenuCategory.ToSharedRef())
				.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

			FSlateIcon CurveIcon(FAppStyle::GetAppStyleSetName(), "WaveTableEditor.Tabs.Properties");
			InTabManager->RegisterTabSpawner(CurveTabId, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args) { return SpawnTab_OutputCurve(Args); }))
				.SetDisplayName(LOCTEXT("TransformCurvesTab", "Transform Curves"))
				.SetGroup(WorkspaceMenuCategory.ToSharedRef())
				.SetIcon(CurveIcon);
		}

		void FBankEditorBase::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
		{
			InTabManager->UnregisterTabSpawner(PropertiesTabId);
			InTabManager->UnregisterTabSpawner(CurveTabId);
		}

		void FBankEditorBase::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InParentObject)
		{
			check(InParentObject);

			CurveEditor = MakeShared<FCurveEditor>();
			FCurveEditorInitParams InitParams;
			CurveEditor->InitCurveEditor(InitParams);
			CurveEditor->GridLineLabelFormatXAttribute = LOCTEXT("GridXLabelFormat", "{0}");

			TUniquePtr<ICurveEditorBounds> EditorBounds = MakeUnique<FStaticCurveEditorBounds>();
			EditorBounds->SetInputBounds(0.05, 1.05);
			CurveEditor->SetBounds(MoveTemp(EditorBounds));

			CurvePanel = SNew(SCurveEditorPanel, CurveEditor.ToSharedRef());

			// Support undo/redo
			InParentObject->SetFlags(RF_Transactional);
			GEditor->RegisterForUndo(this);

			FDetailsViewArgs Args;
			Args.bHideSelectionTip = true;
			Args.NotifyHook = this;

			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertiesView = PropertyModule.CreateDetailView(Args);
			PropertiesView->SetObject(InParentObject);

			TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_WaveTableEditor_Layout_v1")
			->AddArea
			(
				FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.9f)
					->SetOrientation(Orient_Horizontal)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.225f)
						->AddTab(PropertiesTabId, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewSplitter()
						->SetSizeCoefficient(0.775f)
						->SetOrientation(Orient_Vertical)
						->Split
						(
							FTabManager::NewStack()
							->SetHideTabWell(true)
							->SetSizeCoefficient(0.33f)
							->AddTab(CurveTabId, ETabState::OpenedTab)
						)
					)
				)
			);

			const bool bCreateDefaultStandaloneMenu = true;
			const bool bCreateDefaultToolbar = true;
			const bool bToolbarFocusable = false;
			const bool bUseSmallIcons = true;
			FAssetEditorToolkit::InitAssetEditor(
				Mode,
				InitToolkitHost,
				AppIdentifier,
				StandaloneDefaultLayout,
				bCreateDefaultStandaloneMenu,
				bCreateDefaultToolbar,
				InParentObject,
				bToolbarFocusable,
				bUseSmallIcons);

			AddToolbarExtender(CurvePanel->GetToolbarExtender());

			if (CurveEditor.IsValid())
			{
				RegenerateMenusAndToolbars();
			}
		}

		void FBankEditorBase::ClearExpressionCurve(int32 InTransformIndex)
		{
			if (CurveData.IsValidIndex(InTransformIndex))
			{
				CurveData[InTransformIndex].ExpressionCurve.Reset();
			}
		}

		void FBankEditorBase::GenerateExpressionCurve(WaveTable::Editor::FBankEditorBase::FCurveData& OutCurveData, int32 InTransformIndex, bool bInIsUnset)
		{
			TSharedPtr<FRichCurve>& Curve = OutCurveData.ExpressionCurve;
			if (!Curve.IsValid())
			{
				Curve = MakeShared<FRichCurve>();
			}

			const bool bIsBipolar = GetBankIsBipolar();

			if (!GetIsPropertyEditorDisabled())
			{
				if (const FWaveTableTransform* Transform = GetTransform(InTransformIndex))
				{
					// Need at least first and last point to generate expression curve
					const EWaveTableResolution BankResolution = GetBankResolution();

					const int32 PointCount = BankEditorPrivate::GetDrawPointCount(BankResolution, *Transform);
					TArray<float> KeyTable;
					KeyTable.AddZeroed(PointCount);

					// Optimization for really big source files if set to 'File'. Do not recreate WaveTable on the fly
					// for large samples as caching mechanism is too slow for rapid regeneration (i.e. when making
					// interactive edits).
					if (Transform->WaveTableSettings.SourcePCMData.Num() < BankEditorPrivate::MaxLiveEditSamples)
					{
						Transform->CreateWaveTable(KeyTable, bIsBipolar);
					}
					else
					{
						Transform->CopyToWaveTable(KeyTable, bIsBipolar);
					}

					const float Delta = 1.0 / KeyTable.Num();

					FRichCurve& CurveRef = *Curve.Get();
					TArray<FRichCurveKey> NewKeys;

					// Terminate curves at initial point for wavetables
					for (int32 i = 0; i < KeyTable.Num(); ++i)
					{
						NewKeys.Add({ Delta * i, KeyTable[i] });
					}

					// Terminate curves at initial point for wavetables
					// (as they loop) and at the final computed location
					// for non-wavetables (as they are envelopes).
					if (BankResolution == EWaveTableResolution::None)
					{
						float TermPoint = 1.0f;
						Transform->Apply(TermPoint, bIsBipolar);
						NewKeys.Add({ 1.0f, TermPoint });
					}
					else
					{
						NewKeys.Add({ 1.0f, KeyTable[0] });
					}

					CurveRef.SetKeys(NewKeys);

					const EWaveTableCurveSource Source = bInIsUnset ? EWaveTableCurveSource::Unset : EWaveTableCurveSource::Expression;
					SetCurve(InTransformIndex, *Curve.Get(), Source);
				}
			}
		}

		bool FBankEditorBase::RequiresNewCurve(int32 InTransformIndex, const FRichCurve& InRichCurve) const
		{
			const FCurveModelID CurveModelID = CurveData[InTransformIndex].ModelID;
			const TUniquePtr<FCurveModel>* CurveModel = CurveEditor->GetCurves().Find(CurveModelID);
			if (!CurveModel || !CurveModel->IsValid())
			{
				return true;
			}

			FWaveTableCurveModel* PatchCurveModel = static_cast<FWaveTableCurveModel*>(CurveModel->Get());
			check(PatchCurveModel);
			if (&PatchCurveModel->GetRichCurve() != &InRichCurve)
			{
				return true;
			}

			return false;
		}

		void FBankEditorBase::SetCurve(int32 InTransformIndex, FRichCurve& InRichCurve, EWaveTableCurveSource InSource)
		{
			check(CurveEditor.IsValid());

			if (!ensure(CurveData.IsValidIndex(InTransformIndex)))
			{
				return;
			}

			FWaveTableTransform* Transform = GetTransform(InTransformIndex);
			if (!ensure(Transform))
			{
				return;
			}

			FCurveData& CurveDataEntry = CurveData[InTransformIndex];

			const bool bIsBipolar = GetBankIsBipolar();
			const bool bRequiresNewCurve = RequiresNewCurve(InTransformIndex, InRichCurve);
			if (bRequiresNewCurve)
			{
				TUniquePtr<FWaveTableCurveModel> NewCurve = ConstructCurveModel(InRichCurve, GetEditingObject(), InSource);
				NewCurve->Refresh(*Transform, InTransformIndex, bIsBipolar);
				CurveDataEntry.ModelID = CurveEditor->AddCurve(MoveTemp(NewCurve));
				CurveEditor->PinCurve(CurveDataEntry.ModelID);
			}
			else
			{
				const TUniquePtr<FCurveModel>& CurveModel = CurveEditor->GetCurves().FindChecked(CurveDataEntry.ModelID);
				check(CurveModel.Get());
				static_cast<FWaveTableCurveModel*>(CurveModel.Get())->Refresh(*Transform, InTransformIndex, bIsBipolar);
			}
		}

		FName FBankEditorBase::GetToolkitFName() const
		{
			return FName("WaveTableEditor");
		}

		FText FBankEditorBase::GetBaseToolkitName() const
		{
			return LOCTEXT( "AppLabel", "WaveTable Editor" );
		}

		FString FBankEditorBase::GetWorldCentricTabPrefix() const
		{
			return LOCTEXT("WorldCentricTabPrefix", "WaveTable ").ToString();
		}

		FLinearColor FBankEditorBase::GetWorldCentricTabColorScale() const
		{
			return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
		}

		EOrientation FBankEditorBase::GetSnapLabelOrientation() const
		{
			return FMultiBoxSettings::UseSmallToolBarIcons.Get()
				? EOrientation::Orient_Horizontal
				: EOrientation::Orient_Vertical;
		}

		void FBankEditorBase::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
		{
			if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
			{
				RefreshCurves();
			}
			else
			{
				// TODO: Isolate file that changed and only update that curve for performance
				// (Could get bad for larger banks with many file curves).
				RegenerateFileCurves();
			}
		}

		TSharedRef<SDockTab> FBankEditorBase::SpawnTab_Properties(const FSpawnTabArgs& Args)
		{
			check(Args.GetTabId() == PropertiesTabId);

			return SNew(SDockTab)
				.Label(LOCTEXT("SoundWaveTableDetailsTitle", "Details"))
				[
					PropertiesView.ToSharedRef()
				];
		}

		TSharedRef<SDockTab> FBankEditorBase::SpawnTab_OutputCurve(const FSpawnTabArgs& Args)
		{
			RefreshCurves();
			CurveEditor->ZoomToFit();

			TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
				.Label(FText::Format(LOCTEXT("WaveTableFilterTitle", "Filter Transform Curve: {0}"), FText::FromString(GetEditingObject()->GetName())))
				.TabColorScale(GetTabColorScale())
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						CurvePanel.ToSharedRef()
					]
				];

				return NewDockTab;
		}

		void FBankEditorBase::PostUndo(bool bSuccess)
		{
			if (bSuccess)
			{
				RefreshCurves();
			}
		}

		void FBankEditorBase::ResetCurves()
		{
			check(CurveEditor.IsValid());

			CurveEditor->RemoveAllCurves();
			CurveData.Reset();
			CurveData.AddDefaulted(GetNumTransforms());
		}

		void FBankEditorBase::InitCurves()
		{
			for (int32 i = 0; i < GetNumTransforms(); ++i)
			{
				const FWaveTableTransform* Transform = GetTransform(i);
				if (!ensure(Transform))
				{
					continue;
				}

				switch (Transform->Curve)
				{
					case EWaveTableCurve::Exp:
					case EWaveTableCurve::Exp_Inverse:
					case EWaveTableCurve::Linear:
					case EWaveTableCurve::Linear_Inv:
					case EWaveTableCurve::Log:
					case EWaveTableCurve::SCurve:
					case EWaveTableCurve::Sin:
					case EWaveTableCurve::Sin_Full:
					case EWaveTableCurve::File:
					{
						if (RequiresNewCurve(i, *CurveData[i].ExpressionCurve.Get()))
						{
							ResetCurves();
						}
					}
					break;

					case EWaveTableCurve::Shared:
					{
						if (const UCurveFloat* SharedCurve = Transform->CurveShared)
						{
							if (RequiresNewCurve(i, SharedCurve->FloatCurve))
							{
								ResetCurves();
							}
						}
						else if (RequiresNewCurve(i, *CurveData[i].ExpressionCurve.Get()))
						{
							ResetCurves();
						}
					}
					break;

					case EWaveTableCurve::Custom:
					{
						if (RequiresNewCurve(i, Transform->CurveCustom))
						{
							ResetCurves();
						}
					}
					break;

					default:
					{
						static_assert(static_cast<int32>(EWaveTableCurve::Count) == 11, "Possible missing case coverage for output curve.");
					}
					break;
				}
			}
		}

		void FBankEditorBase::RegenerateFileCurves()
		{
			for (int32 i = 0; i < GetNumTransforms(); ++i)
			{
				if (CurveData.IsValidIndex(i))
				{
					FCurveData& CurveDataEntry = CurveData[i];

					if (FWaveTableTransform* Transform = GetTransform(i))
					{
						if (Transform->Curve == EWaveTableCurve::File)
						{
							GenerateExpressionCurve(CurveDataEntry, i);
						}
					}
				}
			}
		}

		void FBankEditorBase::RefreshCurves()
		{
			check(CurveEditor.IsValid());

			for (const FCurveData& CurveDataEntry : CurveData)
			{
				CurveEditor->UnpinCurve(CurveDataEntry.ModelID);
			}

			if (GetIsPropertyEditorDisabled())
			{
				ResetCurves();
				return;
			}

			const int32 NumCurves = GetNumTransforms();
			if (NumCurves == CurveData.Num())
			{
				InitCurves();
			}
			else
			{
				ResetCurves();
			}

			for (int32 i = 0; i < GetNumTransforms(); ++i)
			{
				if (!CurveData.IsValidIndex(i))
				{
					continue;
				}
				FCurveData& CurveDataEntry = CurveData[i];

				FWaveTableTransform* Transform = GetTransform(i);
				if (!Transform)
				{
					continue;
				}

				switch (Transform->Curve)
				{
					case EWaveTableCurve::Exp:
					case EWaveTableCurve::Exp_Inverse:
					case EWaveTableCurve::Linear:
					case EWaveTableCurve::Linear_Inv:
					case EWaveTableCurve::Log:
					case EWaveTableCurve::SCurve:
					case EWaveTableCurve::Sin:
					case EWaveTableCurve::Sin_Full:
					case EWaveTableCurve::File:
					{
						GenerateExpressionCurve(CurveDataEntry, i);
					}
					break;

					case EWaveTableCurve::Shared:
					{
						if (UCurveFloat* SharedCurve = Transform->CurveShared)
						{
							ClearExpressionCurve(i);
							SetCurve(i, SharedCurve->FloatCurve, EWaveTableCurveSource::Shared);
						}
						else
						{
							// Builds a dummy expression that just maps input to output in case
							// where asset isn't selected and leave source as unset
							GenerateExpressionCurve(CurveDataEntry, i, true /* bIsUnset */);
						}
					}
					break;

					case EWaveTableCurve::Custom:
					{
						FRichCurve& CustomCurve = Transform->CurveCustom;
						TrimKeys(CustomCurve);
						ClearExpressionCurve(i);
						SetCurve(i, CustomCurve, EWaveTableCurveSource::Custom);
					}
					break;

					default:
					{
						static_assert(static_cast<int32>(EWaveTableCurve::Count) == 11, "Possible missing case coverage for output curve.");
					}
					break;
				}
			}

			// Collect and remove stale curves from editor
			TArray<FCurveModelID> ToRemove;
			TSet<FCurveModelID> ActiveModelIDs;
			for (const FCurveData& CurveDataEntry : CurveData)
			{
				ActiveModelIDs.Add(CurveDataEntry.ModelID);
			}

			// Remove all dead curves
			{
				const TMap<FCurveModelID, TUniquePtr<FCurveModel>>& Curves = CurveEditor->GetCurves();
				for (const TPair<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : Curves)
				{
					if (!ActiveModelIDs.Contains(Pair.Key))
					{
						ToRemove.Add(Pair.Key);
					}
				}

				for (FCurveModelID ModelID : ToRemove)
				{
					CurveEditor->RemoveCurve(ModelID);
				}
			}

			// Re-pin all curves
			for (const FCurveData& CurveDataEntry : CurveData)
			{
				CurveEditor->PinCurve(CurveDataEntry.ModelID);
			}
		}

		void FBankEditorBase::PostRedo(bool bSuccess)
		{
			if (bSuccess)
			{
				RefreshCurves();
			}
		}

		void FBankEditorBase::TrimKeys(FRichCurve& OutCurve)
		{
			while (OutCurve.GetNumKeys() > 0 && 0.0f > OutCurve.GetFirstKey().Time)
			{
				OutCurve.DeleteKey(OutCurve.GetFirstKeyHandle());
			}

			while (OutCurve.GetNumKeys() > 0 && 1.0f < OutCurve.GetLastKey().Time)
			{
				OutCurve.DeleteKey(OutCurve.GetLastKeyHandle());
			}
		}

		TUniquePtr<WaveTable::Editor::FWaveTableCurveModel> FBankEditor::ConstructCurveModel(FRichCurve& InRichCurve, UObject* InParentObject, EWaveTableCurveSource InSource)
		{
			using namespace WaveTable::Editor;
			return MakeUnique<FWaveTableCurveModel>(InRichCurve, GetEditingObject(), InSource);
		}

		EWaveTableResolution FBankEditor::GetBankResolution() const
		{
			if (UWaveTableBank* Bank = Cast<UWaveTableBank>(GetEditingObject()))
			{
				return Bank->Resolution;
			}

			return EWaveTableResolution::None;
		}

		bool FBankEditor::GetBankIsBipolar() const
		{
			if (UWaveTableBank* Bank = Cast<UWaveTableBank>(GetEditingObject()))
			{
				return Bank->bBipolar;
			}

			return false;
		}

		int32 FBankEditor::GetNumTransforms() const
		{
			if (UWaveTableBank* Bank = Cast<UWaveTableBank>(GetEditingObject()))
			{
				return Bank->Entries.Num();
			}

			return 0;
		}

		FWaveTableTransform* FBankEditor::GetTransform(int32 InIndex) const
		{
			if (UWaveTableBank* Bank = Cast<UWaveTableBank>(GetEditingObject()))
			{
				if (Bank->Entries.IsValidIndex(InIndex))
				{
					return &Bank->Entries[InIndex].Transform;
				}
			}

			return nullptr;
		}
	} // namespace Editor
} // namespace WaveTable
#undef LOCTEXT_NAMESPACE
