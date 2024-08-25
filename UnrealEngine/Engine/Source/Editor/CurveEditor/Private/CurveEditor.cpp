// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditor.h"

#include "Algo/Transform.h"
#include "Containers/SparseArray.h"
#include "CoreGlobals.h"
#include "CurveEditorCommands.h"
#include "CurveEditorCopyBuffer.h"
#include "CurveEditorSettings.h"
#include "CurveEditorSnapMetrics.h"
#include "CurveModel.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Exporters/Exporter.h"
#include "Factories.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/SlateDelegates.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ICurveEditorExtension.h"
#include "ICurveEditorModule.h"
#include "ICurveEditorToolExtension.h"
#include "ITimeSlider.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Geometry.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/Color.h"
#include "Math/NumericLimits.h"
#include "Math/Range.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameTime.h"
#include "Modules/ModuleManager.h"
#include "SCurveEditor.h" // for access to LogCurveEditor
#include "SCurveEditorPanel.h"
#include "SCurveEditorView.h"
#include "ScopedTransaction.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UnrealExporter.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "CurveEditor"

FCurveModelID FCurveModelID::Unique()
{
	static uint32 CurrentID = 1;

	FCurveModelID ID;
	ID.ID = CurrentID++;
	return ID;
}

FCurveEditor::FCurveEditor()
	: Bounds(new FStaticCurveEditorBounds)
	, bBoundTransformUpdatesSuppressed(false)
	, ActiveCurvesSerialNumber(0)
	, SuspendBroadcastCount(0)
{
	Settings = GetMutableDefault<UCurveEditorSettings>();
	CommandList = MakeShared<FUICommandList>();

	OutputSnapEnabledAttribute = true;
	InputSnapEnabledAttribute  = true;
	InputSnapRateAttribute = FFrameRate(10, 1);

	GridLineLabelFormatXAttribute = LOCTEXT("GridXLabelFormat", "{0}s");
	GridLineLabelFormatYAttribute = LOCTEXT("GridYLabelFormat", "{0}");
	
	Settings->GetOnCustomColorsChanged().AddRaw(this, &FCurveEditor::OnCustomColorsChanged);
}

FCurveEditor::~FCurveEditor()
{
	if (Settings)
	{
		Settings->GetOnCustomColorsChanged().RemoveAll(this);
	}
}

void FCurveEditor::InitCurveEditor(const FCurveEditorInitParams& InInitParams)
{
	ICurveEditorModule& CurveEditorModule = FModuleManager::LoadModuleChecked<ICurveEditorModule>("CurveEditor");

	Selection = FCurveEditorSelection(SharedThis(this));

	// Editor Extensions can be registered in the Curve Editor module. To allow users to derive from FCurveEditor
	// we have to manually reach out to the module and get a list of extensions to create an instance of them.
	// If none of your extensions are showing up, it's because you forgot to call this function after construction
	// We're not allowed to use SharedThis(...) in a Constructor so it must exist as a separate function call.
	TArrayView<const FOnCreateCurveEditorExtension> Extensions = CurveEditorModule.GetEditorExtensions();
	for (int32 DelegateIndex = 0; DelegateIndex < Extensions.Num(); ++DelegateIndex)
	{
		check(Extensions[DelegateIndex].IsBound());

		// We call a delegate and have the delegate create the instance to cover cross-module
		TSharedRef<ICurveEditorExtension> NewExtension = Extensions[DelegateIndex].Execute(SharedThis(this));
		EditorExtensions.Add(NewExtension);
	}

	TArrayView<const FOnCreateCurveEditorToolExtension> Tools = CurveEditorModule.GetToolExtensions();
	for (int32 DelegateIndex = 0; DelegateIndex < Tools.Num(); ++DelegateIndex)
	{
		check(Tools[DelegateIndex].IsBound());

		// We call a delegate and have the delegate create the instance to cover cross-module
		AddTool(Tools[DelegateIndex].Execute(SharedThis(this)));
	}
	SuspendBroadcastCount = 0;
	// Listen to global undo so we can fix up our selection state for keys that no longer exist.
	GEditor->RegisterForUndo(this);
}

int32 FCurveEditor::GetSupportedTangentTypes()
{
	return ((int32)ECurveEditorTangentTypes::InterpolationConstant |
		(int32)ECurveEditorTangentTypes::InterpolationLinear |
		(int32)ECurveEditorTangentTypes::InterpolationCubicAuto |
		(int32)ECurveEditorTangentTypes::InterpolationCubicUser |
		(int32)ECurveEditorTangentTypes::InterpolationCubicBreak |
		(int32)ECurveEditorTangentTypes::InterpolationCubicWeighted);
		//nope we don't support smart auto by default, FRichCurve doesn't support i
}

void FCurveEditor::SetPanel(TSharedPtr<SCurveEditorPanel> InPanel)
{
	WeakPanel = InPanel;
}

TSharedPtr<SCurveEditorPanel> FCurveEditor::GetPanel() const
{
	return WeakPanel.Pin();
}

void FCurveEditor::SetView(TSharedPtr<SCurveEditorView> InView)
{
	WeakView = InView;
}

TSharedPtr<SCurveEditorView> FCurveEditor::GetView() const
{
	return WeakView.Pin();
}

FCurveModel* FCurveEditor::FindCurve(FCurveModelID CurveID) const
{
	const TUniquePtr<FCurveModel>* Ptr = CurveData.Find(CurveID);
	return Ptr ? Ptr->Get() : nullptr;
}

const TMap<FCurveModelID, TUniquePtr<FCurveModel>>& FCurveEditor::GetCurves() const
{
	return CurveData;
}

FCurveEditorToolID FCurveEditor::AddTool(TUniquePtr<ICurveEditorToolExtension>&& InTool)
{
	FCurveEditorToolID NewID = FCurveEditorToolID::Unique();
	ToolExtensions.Add(NewID, MoveTemp(InTool));
	ToolExtensions[NewID]->SetToolID(NewID);
	return NewID;
}

FCurveModelID FCurveEditor::AddCurve(TUniquePtr<FCurveModel>&& InCurve)
{
	FCurveModelID NewID = FCurveModelID::Unique();
	FCurveModel *Curve = InCurve.Get();

	CurveData.Add(NewID, MoveTemp(InCurve));
	++ActiveCurvesSerialNumber;
	if (IsBroadcasting())
	{
		OnCurveArrayChanged.Broadcast(Curve, true, this);
	}
	return NewID;
}

void FCurveEditor::BroadcastCurveChanged(FCurveModel* InCurve)
{
	if (IsBroadcasting())
	{
		OnCurveArrayChanged.Broadcast(InCurve, true, this);
	}
}

FCurveModelID FCurveEditor::AddCurveForTreeItem(TUniquePtr<FCurveModel>&& InCurve, FCurveEditorTreeItemID TreeItemID)
{
	FCurveModelID NewID = FCurveModelID::Unique();
	FCurveModel *Curve = InCurve.Get();

	if(IsBroadcasting())
	{
		OnCurveArrayChanged.Broadcast(InCurve.Get(), true, this);
	}

	CurveData.Add(NewID, MoveTemp(InCurve));
	TreeIDByCurveID.Add(NewID, TreeItemID);

	++ActiveCurvesSerialNumber;

	return NewID;
}
void FCurveEditor::ResetMinMaxes()
{
	TSharedPtr<SCurveEditorPanel> Panel = WeakPanel.Pin();
	if (Panel.IsValid())
	{
		Panel->ResetMinMaxes();
	}
}
void FCurveEditor::RemoveCurve(FCurveModelID InCurveID)
{
	TSharedPtr<SCurveEditorPanel> Panel = WeakPanel.Pin();
	if (Panel.IsValid())
	{
		Panel->RemoveCurveFromViews(InCurveID);
	}

	if(IsBroadcasting())
	{
		OnCurveArrayChanged.Broadcast(FindCurve(InCurveID), false,this);
	}


	CurveData.Remove(InCurveID);
	Selection.Remove(InCurveID);
	PinnedCurves.Remove(InCurveID);


	++ActiveCurvesSerialNumber;

}

void FCurveEditor::RemoveAllCurves()
{
	TSharedPtr<SCurveEditorPanel> Panel = WeakPanel.Pin();
	if (Panel.IsValid())
	{
		for (TPair<FCurveModelID, TUniquePtr<FCurveModel>>& CurvePair : CurveData)
		{
			Panel->RemoveCurveFromViews(CurvePair.Key);
		}
	}

	CurveData.Empty();
	Selection.Clear();
	PinnedCurves.Empty();

	++ActiveCurvesSerialNumber;
}

bool FCurveEditor::IsCurvePinned(FCurveModelID InCurveID) const
{
	return PinnedCurves.Contains(InCurveID);
}

void FCurveEditor::PinCurve(FCurveModelID InCurveID)
{
	PinnedCurves.Add(InCurveID);
	++ActiveCurvesSerialNumber;
}

void FCurveEditor::UnpinCurve(FCurveModelID InCurveID)
{
	PinnedCurves.Remove(InCurveID);
	++ActiveCurvesSerialNumber;
}

const SCurveEditorView* FCurveEditor::FindFirstInteractiveView(FCurveModelID InCurveID) const
{
	TSharedPtr<SCurveEditorPanel> Panel = WeakPanel.Pin();
	if (Panel.IsValid())
	{
		for (auto ViewIt = Panel->FindViews(InCurveID); ViewIt; ++ViewIt)
		{
			if (ViewIt.Value()->IsInteractive())
			{
				return &ViewIt.Value().Get();
			}
		}
	}
	return nullptr;
}

FCurveEditorTreeItem& FCurveEditor::GetTreeItem(FCurveEditorTreeItemID ItemID)
{
	return Tree.GetItem(ItemID);
}

const FCurveEditorTreeItem& FCurveEditor::GetTreeItem(FCurveEditorTreeItemID ItemID) const
{
	return Tree.GetItem(ItemID);
}

FCurveEditorTreeItem* FCurveEditor::FindTreeItem(FCurveEditorTreeItemID ItemID)
{
	return Tree.FindItem(ItemID);
}

const FCurveEditorTreeItem* FCurveEditor::FindTreeItem(FCurveEditorTreeItemID ItemID) const
{
	return Tree.FindItem(ItemID);
}

const TArray<FCurveEditorTreeItemID>& FCurveEditor::GetRootTreeItems() const
{
	return Tree.GetRootItems();
}

FCurveEditorTreeItemID FCurveEditor::GetTreeIDFromCurveID(FCurveModelID CurveID) const
{
	if (TreeIDByCurveID.Contains(CurveID))
	{
		return TreeIDByCurveID[CurveID];	
	}

	return FCurveEditorTreeItemID();
}

FCurveEditorTreeItem* FCurveEditor::AddTreeItem(FCurveEditorTreeItemID ParentID)
{
	return Tree.AddItem(ParentID);
}

void FCurveEditor::RemoveTreeItem(FCurveEditorTreeItemID ItemID)
{
	FCurveEditorTreeItem* Item = Tree.FindItem(ItemID);
	if (!Item)
	{
		return;
	}

	Tree.RemoveItem(ItemID, this);
	++ActiveCurvesSerialNumber;
}

void FCurveEditor::RemoveAllTreeItems()
{
	TArray<FCurveEditorTreeItemID> RootItems = Tree.GetRootItems();
	for(FCurveEditorTreeItemID ItemID : RootItems)
	{
		Tree.RemoveItem(ItemID, this);
	}
	++ActiveCurvesSerialNumber;
}

void FCurveEditor::SetTreeSelection(TArray<FCurveEditorTreeItemID>&& TreeItems)
{
	Tree.SetDirectSelection(MoveTemp(TreeItems), this);
}

void FCurveEditor::RemoveFromTreeSelection(TArrayView<const FCurveEditorTreeItemID> TreeItems)
{
	Tree.RemoveFromSelection(TreeItems, this);
}

ECurveEditorTreeSelectionState FCurveEditor::GetTreeSelectionState(FCurveEditorTreeItemID InTreeItemID) const
{
	return Tree.GetSelectionState(InTreeItemID);
}

const TMap<FCurveEditorTreeItemID, ECurveEditorTreeSelectionState>& FCurveEditor::GetTreeSelection() const
{
	return Tree.GetSelection();
}

void FCurveEditor::SetBounds(TUniquePtr<ICurveEditorBounds>&& InBounds)
{
	check(InBounds.IsValid());
	Bounds = MoveTemp(InBounds);
}

bool FCurveEditor::ShouldAutoFrame() const
{
	return Settings->GetAutoFrameCurveEditor();
}


void FCurveEditor::BindCommands()
{
	UCurveEditorSettings* CurveSettings = Settings;
		
	CommandList->MapAction(FGenericCommands::Get().Undo,   FExecuteAction::CreateLambda([]{ GEditor->UndoTransaction(); }));
	CommandList->MapAction(FGenericCommands::Get().Redo,   FExecuteAction::CreateLambda([]{ GEditor->RedoTransaction(); }));
	CommandList->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateSP(this, &FCurveEditor::DeleteSelection));

	CommandList->MapAction(FGenericCommands::Get().Cut, FExecuteAction::CreateSP(this, &FCurveEditor::CutSelection));
	CommandList->MapAction(FGenericCommands::Get().Copy, FExecuteAction::CreateSP(this, &FCurveEditor::CopySelection));
	CommandList->MapAction(FGenericCommands::Get().Paste, FExecuteAction::CreateSP(this, &FCurveEditor::PasteKeys, TSet<FCurveModelID>(), false));
	CommandList->MapAction(FCurveEditorCommands::Get().PasteOverwriteRange, FExecuteAction::CreateSP(this, &FCurveEditor::PasteKeys, TSet<FCurveModelID>(), true));

	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFit, FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFit, EAxisList::All));
	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFitHorizontal, FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFit, EAxisList::X));
	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFitVertical, FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFit, EAxisList::Y));
	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFitAll, FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFitAll, EAxisList::All));

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleExpandCollapseNodes, FExecuteAction::CreateSP(this, &FCurveEditor::ToggleExpandCollapseNodes, false));
	CommandList->MapAction(FCurveEditorCommands::Get().ToggleExpandCollapseNodesAndDescendants, FExecuteAction::CreateSP(this, &FCurveEditor::ToggleExpandCollapseNodes, true));

	CommandList->MapAction(FCurveEditorCommands::Get().TranslateSelectedKeysLeft, FExecuteAction::CreateSP(this, &FCurveEditor::TranslateSelectedKeysLeft));
	CommandList->MapAction(FCurveEditorCommands::Get().TranslateSelectedKeysRight, FExecuteAction::CreateSP(this, &FCurveEditor::TranslateSelectedKeysRight));

	CommandList->MapAction(FCurveEditorCommands::Get().StepToNextKey, FExecuteAction::CreateSP(this, &FCurveEditor::StepToNextKey));
	CommandList->MapAction(FCurveEditorCommands::Get().StepToPreviousKey, FExecuteAction::CreateSP(this, &FCurveEditor::StepToPreviousKey));
	CommandList->MapAction(FCurveEditorCommands::Get().StepForward, FExecuteAction::CreateSP(this, &FCurveEditor::StepForward), EUIActionRepeatMode::RepeatEnabled);
	CommandList->MapAction(FCurveEditorCommands::Get().StepBackward, FExecuteAction::CreateSP(this, &FCurveEditor::StepBackward), EUIActionRepeatMode::RepeatEnabled);
	CommandList->MapAction(FCurveEditorCommands::Get().JumpToStart, FExecuteAction::CreateSP(this, &FCurveEditor::JumpToStart));
	CommandList->MapAction(FCurveEditorCommands::Get().JumpToEnd, FExecuteAction::CreateSP(this, &FCurveEditor::JumpToEnd));

	CommandList->MapAction(FCurveEditorCommands::Get().SetSelectionRangeStart, FExecuteAction::CreateSP(this, &FCurveEditor::SetSelectionRangeStart));
	CommandList->MapAction(FCurveEditorCommands::Get().SetSelectionRangeEnd, FExecuteAction::CreateSP(this, &FCurveEditor::SetSelectionRangeEnd));
	CommandList->MapAction(FCurveEditorCommands::Get().ClearSelectionRange, FExecuteAction::CreateSP(this, &FCurveEditor::ClearSelectionRange));

	CommandList->MapAction(FCurveEditorCommands::Get().SelectAllKeys, FExecuteAction::CreateSP(this, &FCurveEditor::SelectAllKeys));
	CommandList->MapAction(FCurveEditorCommands::Get().SelectForward, FExecuteAction::CreateSP(this, &FCurveEditor::SelectForward));
	CommandList->MapAction(FCurveEditorCommands::Get().SelectBackward, FExecuteAction::CreateSP(this, &FCurveEditor::SelectBackward));
	CommandList->MapAction(FCurveEditorCommands::Get().SelectNone, FExecuteAction::CreateSP(this, &FCurveEditor::SelectNone));

	{
		FExecuteAction   ToggleInputSnapping     = FExecuteAction::CreateSP(this,   &FCurveEditor::ToggleInputSnapping);
		FIsActionChecked IsInputSnappingEnabled  = FIsActionChecked::CreateSP(this, &FCurveEditor::IsInputSnappingEnabled);
		FExecuteAction   ToggleOutputSnapping    = FExecuteAction::CreateSP(this,   &FCurveEditor::ToggleOutputSnapping);
		FIsActionChecked IsOutputSnappingEnabled = FIsActionChecked::CreateSP(this, &FCurveEditor::IsOutputSnappingEnabled);

		CommandList->MapAction(FCurveEditorCommands::Get().ToggleInputSnapping, ToggleInputSnapping, FCanExecuteAction(), IsInputSnappingEnabled);
		CommandList->MapAction(FCurveEditorCommands::Get().ToggleOutputSnapping, ToggleOutputSnapping, FCanExecuteAction(), IsOutputSnappingEnabled);
	}

	// Flatten and Straighten Tangents
	{
		CommandList->MapAction(FCurveEditorCommands::Get().FlattenTangents, FExecuteAction::CreateSP(this, &FCurveEditor::FlattenSelection), FCanExecuteAction::CreateSP(this, &FCurveEditor::CanFlattenOrStraightenSelection) );
		CommandList->MapAction(FCurveEditorCommands::Get().StraightenTangents, FExecuteAction::CreateSP(this, &FCurveEditor::StraightenSelection), FCanExecuteAction::CreateSP(this, &FCurveEditor::CanFlattenOrStraightenSelection) );
	}

	// Curve Colors
	{
		CommandList->MapAction(FCurveEditorCommands::Get().SetRandomCurveColorsForSelected, FExecuteAction::CreateSP(this, &FCurveEditor::SetRandomCurveColorsForSelected), FCanExecuteAction());
		CommandList->MapAction(FCurveEditorCommands::Get().SetCurveColorsForSelected, FExecuteAction::CreateSP(this, &FCurveEditor::SetCurveColorsForSelected), FCanExecuteAction());
	}

	// Tangent Visibility
	{
		FExecuteAction SetAllTangents          = FExecuteAction::CreateUObject(Settings, &UCurveEditorSettings::SetTangentVisibility, ECurveEditorTangentVisibility::AllTangents);
		FExecuteAction SetSelectedKeyTangents  = FExecuteAction::CreateUObject(Settings, &UCurveEditorSettings::SetTangentVisibility, ECurveEditorTangentVisibility::SelectedKeys);
		FExecuteAction SetNoTangents           = FExecuteAction::CreateUObject(Settings, &UCurveEditorSettings::SetTangentVisibility, ECurveEditorTangentVisibility::NoTangents);

		FIsActionChecked IsAllTangents         = FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetTangentVisibility() == ECurveEditorTangentVisibility::AllTangents; } );
		FIsActionChecked IsSelectedKeyTangents = FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetTangentVisibility() == ECurveEditorTangentVisibility::SelectedKeys; } );
		FIsActionChecked IsNoTangents          = FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetTangentVisibility() == ECurveEditorTangentVisibility::NoTangents; } );

		CommandList->MapAction(FCurveEditorCommands::Get().SetAllTangentsVisibility, SetAllTangents, FCanExecuteAction(), IsAllTangents);
		CommandList->MapAction(FCurveEditorCommands::Get().SetSelectedKeysTangentVisibility, SetSelectedKeyTangents, FCanExecuteAction(), IsSelectedKeyTangents);
		CommandList->MapAction(FCurveEditorCommands::Get().SetNoTangentsVisibility, SetNoTangents, FCanExecuteAction(), IsNoTangents);
	}

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleAutoFrameCurveEditor,
		FExecuteAction::CreateLambda( [CurveSettings]{ CurveSettings->SetAutoFrameCurveEditor( !CurveSettings->GetAutoFrameCurveEditor() ); } ),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetAutoFrameCurveEditor(); } )
	);

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleShowBars,
		FExecuteAction::CreateLambda([this, CurveSettings] { CurveSettings->SetShowBars(!CurveSettings->GetShowBars()); Tree.RecreateModelsFromExistingSelection(this); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([CurveSettings] { return CurveSettings->GetShowBars(); })
	);

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleSnapTimeToSelection,
		FExecuteAction::CreateLambda( [CurveSettings]{ CurveSettings->SetSnapTimeToSelection( !CurveSettings->GetSnapTimeToSelection() ); } ),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetSnapTimeToSelection(); } )
	);

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleShowBufferedCurves,
		FExecuteAction::CreateLambda( [CurveSettings]{ CurveSettings->SetShowBufferedCurves( !CurveSettings->GetShowBufferedCurves() ); } ),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetShowBufferedCurves(); } ) );

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleShowCurveEditorCurveToolTips,
		FExecuteAction::CreateLambda( [CurveSettings]{ CurveSettings->SetShowCurveEditorCurveToolTips( !CurveSettings->GetShowCurveEditorCurveToolTips() ); } ),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetShowCurveEditorCurveToolTips(); } ) );

	// Deactivate Current Tool
	CommandList->MapAction(FCurveEditorCommands::Get().DeactivateCurrentTool,
		FExecuteAction::CreateSP(this, &FCurveEditor::MakeToolActive, FCurveEditorToolID::Unset()),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda( [this]{ return ActiveTool.IsSet() == false; } ) );

	// Bind commands for Editor Extensions
	for (TSharedRef<ICurveEditorExtension> Extension : EditorExtensions)
	{
		Extension->BindCommands(CommandList.ToSharedRef());
	}

	// Bind Commands for Tool Extensions
	for (TPair<FCurveEditorToolID, TUniquePtr<ICurveEditorToolExtension>>& Pair : ToolExtensions)
	{
		Pair.Value->BindCommands(CommandList.ToSharedRef());
	}
}

FCurveSnapMetrics FCurveEditor::GetCurveSnapMetrics(FCurveModelID CurveModel) const
{
	FCurveSnapMetrics CurveMetrics;

	const SCurveEditorView* View = FindFirstInteractiveView(CurveModel);
	if (!View)
	{
		return CurveMetrics;
	}

	// get the grid lines in view space
	TArray<float> ViewSpaceGridLines;
	View->GetGridLinesY(SharedThis(this), ViewSpaceGridLines, ViewSpaceGridLines);

	// convert the grid lines from view space
	TArray<double> CurveSpaceGridLines;
	ViewSpaceGridLines.Reserve(ViewSpaceGridLines.Num());
	FCurveEditorScreenSpace CurveSpace = View->GetCurveSpace(CurveModel);
	Algo::Transform(ViewSpaceGridLines, CurveSpaceGridLines, [&CurveSpace](float VSVal) { return CurveSpace.ScreenToValue(VSVal); });
	
	// create metrics struct;
	CurveMetrics.bSnapOutputValues = OutputSnapEnabledAttribute.Get();
	CurveMetrics.bSnapInputValues = InputSnapEnabledAttribute.Get();
	CurveMetrics.AllGridLines = CurveSpaceGridLines;
	CurveMetrics.InputSnapRate = InputSnapRateAttribute.Get();

	return CurveMetrics;
}

void FCurveEditor::ZoomToFit(EAxisList::Type Axes)
{
	// If they have keys selected, we fit the specific keys.
	if (Selection.Count() > 0)
	{
		ZoomToFitSelection(Axes);
	}
	else
	{
		ZoomToFitAll(Axes);
	}
}

void FCurveEditor::ZoomToFitAll(EAxisList::Type Axes)
{
	TMap<FCurveModelID, FKeyHandleSet> AllCurves;
	for (FCurveModelID ID : GetEditedCurves())
	{
		AllCurves.Add(ID);
	}
	ZoomToFitInternal(Axes, AllCurves);
}

void FCurveEditor::ZoomToFitCurves(TArrayView<const FCurveModelID> CurveModelIDs, EAxisList::Type Axes)
{
	TMap<FCurveModelID, FKeyHandleSet> AllCurves;
	for (FCurveModelID ID : CurveModelIDs)
	{
		AllCurves.Add(ID);
	}
	ZoomToFitInternal(Axes, AllCurves);
}

void FCurveEditor::ZoomToFitSelection(EAxisList::Type Axes)
{
	ZoomToFitInternal(Axes, Selection.GetAll());
}

void FCurveEditor::ZoomToFitInternal(EAxisList::Type Axes, const TMap<FCurveModelID, FKeyHandleSet>& CurveKeySet)
{
	TArray<FKeyPosition> KeyPositionsScratch;

	double InputMin = TNumericLimits<double>::Max(), InputMax = TNumericLimits<double>::Lowest();

	TMap<TSharedRef<SCurveEditorView>, TTuple<double, double>> ViewToOutputBounds;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveKeySet)
	{
		FCurveModelID CurveID = Pair.Key;
		const FCurveModel* Curve = FindCurve(CurveID);
		if (!Curve)
		{
			continue;
		}

		double OutputMin = TNumericLimits<double>::Max(), OutputMax = TNumericLimits<double>::Lowest();

		int32 NumKeys = Pair.Value.AsArray().Num();
		if (NumKeys == 0)
		{
			double LocalMin = 0.0, LocalMax = 1.0;

			// Zoom to the entire curve range if no specific keys are specified
			if (Curve->GetNumKeys())
			{
				// Only zoom time range if there are keys on the curve (otherwise where do we zoom *to* on an infinite timeline?)
				Curve->GetTimeRange(LocalMin, LocalMax);
				InputMin = FMath::Min(InputMin, LocalMin);
				InputMax = FMath::Max(InputMax, LocalMax);
			}

			// Most curve types we know about support default values, so we can zoom to that even if there are no keys
			Curve->GetValueRange(LocalMin, LocalMax);
			OutputMin = FMath::Min(OutputMin, LocalMin);
			OutputMax = FMath::Max(OutputMax, LocalMax);
		}
		else
		{
			// Zoom to the min/max of the specified key set
			KeyPositionsScratch.SetNum(NumKeys, EAllowShrinking::No);
			Curve->GetKeyPositions(Pair.Value.AsArray(), KeyPositionsScratch);
			for (const FKeyPosition& Key : KeyPositionsScratch)
			{
				InputMin  = FMath::Min(InputMin, Key.InputValue);
				InputMax  = FMath::Max(InputMax, Key.InputValue);
				OutputMin = FMath::Min(OutputMin, Key.OutputValue);
				OutputMax = FMath::Max(OutputMax, Key.OutputValue);
			}
		}

		if (Axes & EAxisList::Y)
		{
			TSharedPtr<SCurveEditorPanel> Panel = WeakPanel.Pin();
			TSharedPtr<SCurveEditorView> View = WeakView.Pin();
			if (Panel.IsValid())
			{
				// Store the min max for each view
				for (auto ViewIt = Panel->FindViews(CurveID); ViewIt; ++ViewIt)
				{
					TTuple<double, double>* ViewBounds = ViewToOutputBounds.Find(ViewIt.Value());
					if (ViewBounds)
					{
						ViewBounds->Get<0>() = FMath::Min(ViewBounds->Get<0>(), OutputMin);
						ViewBounds->Get<1>() = FMath::Max(ViewBounds->Get<1>(), OutputMax);
					}
					else
					{
						ViewToOutputBounds.Add(ViewIt.Value(), MakeTuple(OutputMin, OutputMax));
					}
				}
			}
			else if(View.IsValid())
			{
				TTuple<double, double>* ViewBounds = ViewToOutputBounds.Find(View.ToSharedRef());
				if (ViewBounds)
				{
					ViewBounds->Get<0>() = FMath::Min(ViewBounds->Get<0>(), OutputMin);
					ViewBounds->Get<1>() = FMath::Max(ViewBounds->Get<1>(), OutputMax);
				}
				else
				{
					ViewToOutputBounds.Add(View.ToSharedRef(), MakeTuple(OutputMin, OutputMax));
				}
			}
		}
	}

	if (Axes & EAxisList::X && InputMin != TNumericLimits<double>::Max() && InputMax != TNumericLimits<double>::Lowest())
	{
		// If zooming to the same (or invalid) min/max, keep the same zoom scale and center within the timeline
		if (InputMin >= InputMax)
		{
			double CurrentInputMin = 0.0, CurrentInputMax = 1.0;
			Bounds->GetInputBounds(CurrentInputMin, CurrentInputMax);

			const double HalfInputScale = (CurrentInputMax - CurrentInputMin)*0.5;
			InputMin -= HalfInputScale;
			InputMax += HalfInputScale;
		}
		else
		{
			TSharedPtr<SCurveEditorPanel> Panel = WeakPanel.Pin();
			TSharedPtr<SCurveEditorView> View = WeakView.Pin();

			double PanelWidth = 0;
			if (Panel.IsValid())
			{
				PanelWidth = WeakPanel.Pin()->GetViewContainerGeometry().GetLocalSize().X;
			}
			else if (View.IsValid())
			{
				PanelWidth = View->GetViewSpace().GetPhysicalWidth();
			}
			
			double InputPercentage = PanelWidth != 0 ? FMath::Min(Settings->GetFrameInputPadding() / PanelWidth, 0.5) : 0.1; // Cannot pad more than half the width

			const double MinInputZoom = InputSnapEnabledAttribute.Get() ? InputSnapRateAttribute.Get().AsInterval() : 0.00001;
			const double InputPadding = FMath::Max((InputMax - InputMin) * InputPercentage, MinInputZoom);
			InputMax = FMath::Max(InputMin + MinInputZoom, InputMax);

			InputMin -= InputPadding;
			InputMax += InputPadding;
		}

		Bounds->SetInputBounds(InputMin, InputMax);
	}

	// Perform per-view output zoom for any computed ranges
	for (const TTuple<TSharedRef<SCurveEditorView>, TTuple<double, double>>& ViewAndBounds : ViewToOutputBounds)
	{
		TSharedRef<SCurveEditorView> View = ViewAndBounds.Key;

		double OutputMin = ViewAndBounds.Value.Get<0>();
		double OutputMax = ViewAndBounds.Value.Get<1>();

		// If zooming to the same (or invalid) min/max, keep the same zoom scale and center within the timeline
		if (OutputMin >= OutputMax)
		{
			const double HalfOutputScale = (View->GetOutputMax() - View->GetOutputMin()) * 0.5;
			OutputMin -= HalfOutputScale;
			OutputMax += HalfOutputScale;
		}
		else
		{
			TSharedPtr<SCurveEditorPanel> Panel = WeakPanel.Pin();

			double PanelHeight = 0;
			if (Panel.IsValid())
			{
				PanelHeight = WeakPanel.Pin()->GetViewContainerGeometry().GetLocalSize().Y;
			}
			else
			{
				PanelHeight = View->GetViewSpace().GetPhysicalHeight();
			}

			double OutputPercentage = PanelHeight != 0 ? FMath::Min(Settings->GetFrameOutputPadding() / PanelHeight, 0.5) : 0.1; // Cannot pad more than half the height

			constexpr double MinOutputZoom = 0.00001;
			const double OutputPadding = FMath::Max((OutputMax - OutputMin) * OutputPercentage, MinOutputZoom);

			OutputMin -= OutputPadding;
			OutputMax = FMath::Max(OutputMin + MinOutputZoom, OutputMax) + OutputPadding;
		}
		View->FrameVertical(OutputMin, OutputMax);
	}
}

void FCurveEditor::TranslateSelectedKeys(double SecondsToAdd)
{
	if (Selection.Count() > 0)
	{
		for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
		{
			if (FCurveModel* Curve = FindCurve(Pair.Key))
			{
				int32 NumKeys = Pair.Value.Num();

				if (NumKeys > 0)
				{
					TArrayView<const FKeyHandle> KeyHandles = Pair.Value.AsArray();
					TArray<FKeyPosition> KeyPositions;
					KeyPositions.SetNum(KeyHandles.Num());

					Curve->GetKeyPositions(KeyHandles, KeyPositions);

					for (int KeyIndex = 0; KeyIndex < KeyPositions.Num(); ++KeyIndex)
					{
						KeyPositions[KeyIndex].InputValue += SecondsToAdd;
					}
					Curve->SetKeyPositions(KeyHandles, KeyPositions);
				}
			}
		}
	}
}

void FCurveEditor::TranslateSelectedKeysLeft()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("TranslateKeysLeft", "Translate Keys Left"));
	FFrameRate FrameRate = TimeSliderController->GetDisplayRate();
	double SecondsToAdd =  -FrameRate.AsInterval();
	TranslateSelectedKeys(SecondsToAdd);
}

void FCurveEditor::TranslateSelectedKeysRight()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("TranslateKeyRight", "Translate Keys Right"));
	FFrameRate FrameRate = TimeSliderController->GetDisplayRate();
	double SecondsToAdd = FrameRate.AsInterval();

	TranslateSelectedKeys(SecondsToAdd);
}

void FCurveEditor::SnapToSelectedKey()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();

	TOptional<double> MinTime;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			int32 NumKeys = Pair.Value.Num();

			if (NumKeys > 0)
			{
				TArrayView<const FKeyHandle> KeyHandles = Pair.Value.AsArray();
				TArray<FKeyPosition> KeyPositions;
				KeyPositions.SetNum(KeyHandles.Num());

				Curve->GetKeyPositions(KeyHandles, KeyPositions);

				for (const FKeyPosition& KeyPosition : KeyPositions)
				{
					if (MinTime.IsSet())
					{
						MinTime = FMath::Min(KeyPosition.InputValue, MinTime.GetValue());
					}
					else
					{
						MinTime = KeyPosition.InputValue;
					}
				}
			}
		}
	}

	if (MinTime.IsSet())
	{
		TimeSliderController->SetScrubPosition(MinTime.GetValue() * TickResolution,/*bEvaluate*/ true);		
	}
}

void FCurveEditor::StepToNextKey()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();

	double CurrentTime = TickResolution.AsSeconds(TimeSliderController->GetScrubPosition());

	TOptional<double> NextTime;
	TOptional<double> MinTime;

	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveData)
	{
		FCurveModel* CurveModel = Pair.Value.Get();

		if (CurveModel)
		{
			TArray<FKeyHandle> KeyHandles;
			double MaxTime = NextTime.IsSet() ? NextTime.GetValue() : TNumericLimits<double>::Max();
			CurveModel->GetKeys(*this, CurrentTime, MaxTime, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);

			TArray<FKeyPosition> KeyPositions;
			KeyPositions.SetNum(KeyHandles.Num());
			CurveModel->GetKeyPositions(TArrayView<FKeyHandle>(KeyHandles), KeyPositions);

			for (const FKeyPosition& KeyPosition : KeyPositions)
			{
				if (KeyPosition.InputValue > CurrentTime)
				{
					if (!NextTime.IsSet() || KeyPosition.InputValue < NextTime.GetValue())
					{
						NextTime = KeyPosition.InputValue;
					}
				}
			}

			double CurveMinTime, CurveMaxTime;
			CurveModel->GetTimeRange(CurveMinTime, CurveMaxTime);
			if (!MinTime.IsSet() || CurveMinTime < MinTime.GetValue())
			{
				MinTime = CurveMinTime;
			}
		}
	}

	if (NextTime.IsSet())
	{
		TimeSliderController->SetScrubPosition(NextTime.GetValue() * TickResolution,/*bEvaluate*/ true);
	}
	else if (MinTime.IsSet())
	{
		TimeSliderController->SetScrubPosition(MinTime.GetValue() * TickResolution, /*bEvaluate*/ true);
	}
}

void FCurveEditor::StepToPreviousKey()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();

	double CurrentTime = TickResolution.AsSeconds(TimeSliderController->GetScrubPosition());

	TOptional<double> PreviousTime;
	TOptional<double> MaxTime;

	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveData)
	{
		FCurveModel* CurveModel = Pair.Value.Get();

		if (CurveModel)
		{
			TArray<FKeyHandle> KeyHandles;
			double MinTime = PreviousTime.IsSet() ? PreviousTime.GetValue() : TNumericLimits<double>::Lowest();
			CurveModel->GetKeys(*this, MinTime, CurrentTime, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);

			TArray<FKeyPosition> KeyPositions;
			KeyPositions.SetNum(KeyHandles.Num());
			CurveModel->GetKeyPositions(TArrayView<FKeyHandle>(KeyHandles), KeyPositions);

			for (const FKeyPosition& KeyPosition : KeyPositions)
			{
				if (KeyPosition.InputValue < CurrentTime)
				{
					if (!PreviousTime.IsSet() || KeyPosition.InputValue > PreviousTime.GetValue())
					{
						PreviousTime = KeyPosition.InputValue;
					}
				}
			}

			double CurveMinTime, CurveMaxTime;
			CurveModel->GetTimeRange(CurveMinTime, CurveMaxTime);
			if (!MaxTime.IsSet() || CurveMaxTime > MaxTime.GetValue())
			{
				MaxTime = CurveMaxTime;
			}
		}
	}

	if (PreviousTime.IsSet())
	{
		TimeSliderController->SetScrubPosition(PreviousTime.GetValue() * TickResolution,/*bEvaluate*/ true);
	}
	else if (MaxTime.IsSet())
	{
		TimeSliderController->SetScrubPosition(MaxTime.GetValue() * TickResolution, /*bEvaluate*/ true);
	}
}


void FCurveEditor::StepForward()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	FFrameRate DisplayRate = TimeSliderController->GetDisplayRate();

	FFrameTime OneFrame = FFrameRate::TransformTime(FFrameTime(1), DisplayRate, TickResolution);

	TimeSliderController->SetScrubPosition(TimeSliderController->GetScrubPosition() + OneFrame, /*bEvaluate*/ true);
}

void FCurveEditor::StepBackward()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	FFrameRate DisplayRate = TimeSliderController->GetDisplayRate();

	FFrameTime OneFrame = FFrameRate::TransformTime(FFrameTime(1), DisplayRate, TickResolution);

	TimeSliderController->SetScrubPosition(TimeSliderController->GetScrubPosition() - OneFrame, /*bEvaluate*/ true);
}

void FCurveEditor::JumpToStart()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	TimeSliderController->SetScrubPosition(TimeSliderController->GetTimeBounds().GetLowerBoundValue(), /*bEvaluate*/ true);
}

void FCurveEditor::JumpToEnd()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	const bool bInsetDisplayFrame = IsInputSnappingEnabled();

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();
	FFrameRate DisplayRate = TimeSliderController->GetDisplayRate();

	// Calculate an offset from the end to go to. If they have snapping on (and the scrub style is a block) the last valid frame is represented as one
	// whole display rate frame before the end, otherwise we just subtract a single frame which matches the behavior of hitting play and letting it run to the end.
	FFrameTime OneFrame = bInsetDisplayFrame ? FFrameRate::TransformTime(FFrameTime(1), DisplayRate, TickResolution) : FFrameTime(1);
	FFrameTime NewTime = TimeSliderController->GetTimeBounds().GetUpperBoundValue() - OneFrame;

	TimeSliderController->SetScrubPosition(NewTime, /*bEvaluate*/ true);
}

void FCurveEditor::SetSelectionRangeStart()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameNumber LocalTime = TimeSliderController->GetScrubPosition().FrameNumber;
	FFrameNumber UpperBound = TimeSliderController->GetSelectionRange().GetUpperBoundValue();
	if (UpperBound <= LocalTime)
	{
		TimeSliderController->SetSelectionRange(TRange<FFrameNumber>(LocalTime, LocalTime + 1));
	}
	else
	{
		TimeSliderController->SetSelectionRange(TRange<FFrameNumber>(LocalTime, UpperBound));
	}
}

void FCurveEditor::SetSelectionRangeEnd()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameNumber LocalTime = TimeSliderController->GetScrubPosition().FrameNumber;
	FFrameNumber LowerBound = TimeSliderController->GetSelectionRange().GetLowerBoundValue();
	if (LowerBound >= LocalTime)
	{
		TimeSliderController->SetSelectionRange(TRange<FFrameNumber>(LocalTime - 1, LocalTime));
	}
	else
	{
		TimeSliderController->SetSelectionRange(TRange<FFrameNumber>(LowerBound, LocalTime));
	}
}

void FCurveEditor::ClearSelectionRange()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	TimeSliderController->SetSelectionRange(TRange<FFrameNumber>::Empty());
}

void FCurveEditor::SelectAllKeys()
{
	for (FCurveModelID ID : GetEditedCurves())
	{
		if (FCurveModel* Curve = FindCurve(ID))
		{
			TArray<FKeyHandle> KeyHandles;
			Curve->GetKeys(*this, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);
			Selection.Add(ID, ECurvePointType::Key, KeyHandles);
		}
	}
}

void FCurveEditor::SelectForward()
{
	Selection.Clear();

	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();

	double CurrentTime = TickResolution.AsSeconds(TimeSliderController->GetScrubPosition());

	for (FCurveModelID ID : GetEditedCurves())
	{
		if (FCurveModel* Curve = FindCurve(ID))
		{
			TArray<FKeyHandle> KeyHandles;
			Curve->GetKeys(*this, CurrentTime, TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);
			Selection.Add(ID, ECurvePointType::Key, KeyHandles);
		}
	}
}

void FCurveEditor::SelectBackward()
{
	Selection.Clear();

	TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (!TimeSliderController.IsValid())
	{
		return;
	}

	FFrameRate TickResolution = TimeSliderController->GetTickResolution();

	double CurrentTime = TickResolution.AsSeconds(TimeSliderController->GetScrubPosition());

	for (FCurveModelID ID : GetEditedCurves())
	{
		if (FCurveModel* Curve = FindCurve(ID))
		{
			TArray<FKeyHandle> KeyHandles;
			Curve->GetKeys(*this, TNumericLimits<double>::Min(), CurrentTime, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);
			Selection.Add(ID, ECurvePointType::Key, KeyHandles);
		}
	}
}

void FCurveEditor::SelectNone()
{
	Selection.Clear();
}


bool FCurveEditor::IsInputSnappingEnabled() const
{
	return InputSnapEnabledAttribute.Get();
}

void FCurveEditor::ToggleInputSnapping()
{
	bool NewValue = !InputSnapEnabledAttribute.Get();

	if (!InputSnapEnabledAttribute.IsBound())
	{
		InputSnapEnabledAttribute = NewValue;
	}
	else
	{
		OnInputSnapEnabledChanged.ExecuteIfBound(NewValue);
	}
}

bool FCurveEditor::IsOutputSnappingEnabled() const
{
	return OutputSnapEnabledAttribute.Get();
}

void FCurveEditor::ToggleOutputSnapping()
{
	bool NewValue = !OutputSnapEnabledAttribute.Get();

	if (!OutputSnapEnabledAttribute.IsBound())
	{
		OutputSnapEnabledAttribute = NewValue;
	}
	else
	{
		OnOutputSnapEnabledChanged.ExecuteIfBound(NewValue);
	}
}

void
FCurveEditor::ToggleExpandCollapseNodes(bool bRecursive)
{
	Tree.ToggleExpansionState(bRecursive);
}

FCurveEditorScreenSpaceH FCurveEditor::GetPanelInputSpace() const
{
	const float PanelWidth = FMath::Max(1.f, WeakPanel.Pin()->GetViewContainerGeometry().GetLocalSize().X);

	double InputMin = 0.0, InputMax = 1.0;
	Bounds->GetInputBounds(InputMin, InputMax);

	InputMax = FMath::Max(InputMax, InputMin + 1e-10);
	return FCurveEditorScreenSpaceH(PanelWidth, InputMin, InputMax);
}

void FCurveEditor::ConstructXGridLines(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels) const
{
	FCurveEditorScreenSpaceH InputSpace = GetPanelInputSpace();

	double MajorGridStep  = 0.0;
	int32  MinorDivisions = 0;
	if (InputSnapRateAttribute.Get().ComputeGridSpacing(InputSpace.PixelsPerInput(), MajorGridStep, MinorDivisions))
	{
		FText GridLineLabelFormatX = GridLineLabelFormatXAttribute.Get();
		const double FirstMajorLine = FMath::FloorToDouble(InputSpace.GetInputMin() / MajorGridStep) * MajorGridStep;
		const double LastMajorLine  = FMath::CeilToDouble(InputSpace.GetInputMax() / MajorGridStep) * MajorGridStep;

		for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
		{
			MajorGridLines.Add( (CurrentMajorLine - InputSpace.GetInputMin()) * InputSpace.PixelsPerInput() );
			if (MajorGridLabels)
			{
				MajorGridLabels->Add(FText::Format(GridLineLabelFormatX, FText::AsNumber(CurrentMajorLine)));
			}

			for (int32 Step = 1; Step < MinorDivisions; ++Step)
			{
				float MinorLine = CurrentMajorLine + Step*MajorGridStep/MinorDivisions;
				MinorGridLines.Add( (MinorLine - InputSpace.GetInputMin()) * InputSpace.PixelsPerInput() );
			}
		}
	}
}

void FCurveEditor::CutSelection()
{
	FScopedTransaction Transaction(LOCTEXT("CutKeys", "Cut Keys"));

	CopySelection();
	DeleteSelection();
}

void FCurveEditor::GetChildCurveModelIDs(const FCurveEditorTreeItemID TreeItemID, TSet<FCurveModelID>& OutCurveModelIDs) const
{
	const FCurveEditorTreeItem& TreeItem = GetTreeItem(TreeItemID);
	for (const FCurveModelID& CurveModelID : TreeItem.GetCurves())
	{
		OutCurveModelIDs.Add(CurveModelID);
	}

	for (const FCurveEditorTreeItemID& ChildTreeItem : TreeItem.GetChildren())
	{
		GetChildCurveModelIDs(ChildTreeItem, OutCurveModelIDs);
	}
}

void FCurveEditor::CopySelection() const
{
	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	TOptional<double> KeyOffset;

	UCurveEditorCopyBuffer* CopyableBuffer = NewObject<UCurveEditorCopyBuffer>(GetTransientPackage(), UCurveEditorCopyBuffer::StaticClass(), NAME_None, RF_Transient);

	if (Selection.Count() > 0)
	{
		for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
		{
			if (FCurveModel* Curve = FindCurve(Pair.Key))
			{
				int32 NumKeys = Pair.Value.Num();

				if (NumKeys > 0)
				{
					UCurveEditorCopyableCurveKeys *CopyableCurveKeys = NewObject<UCurveEditorCopyableCurveKeys>(CopyableBuffer, UCurveEditorCopyableCurveKeys::StaticClass(), NAME_None, RF_Transient);

					CopyableCurveKeys->ShortDisplayName = Curve->GetShortDisplayName().ToString();
					CopyableCurveKeys->LongDisplayName = Curve->GetLongDisplayName().ToString();
					CopyableCurveKeys->LongIntentionName = Curve->GetLongIntentionName();
					CopyableCurveKeys->IntentionName = Curve->GetIntentionName();
					CopyableCurveKeys->KeyPositions.SetNum(NumKeys, EAllowShrinking::No);
					CopyableCurveKeys->KeyAttributes.SetNum(NumKeys, EAllowShrinking::No);

					TArrayView<const FKeyHandle> KeyHandles = Pair.Value.AsArray();

					Curve->GetKeyPositions(KeyHandles, CopyableCurveKeys->KeyPositions);
					Curve->GetKeyAttributes(KeyHandles, CopyableCurveKeys->KeyAttributes);

					for (int KeyIndex = 0; KeyIndex < CopyableCurveKeys->KeyPositions.Num(); ++KeyIndex)
					{
						if (!KeyOffset.IsSet() || CopyableCurveKeys->KeyPositions[KeyIndex].InputValue < KeyOffset.GetValue())
						{
							KeyOffset = CopyableCurveKeys->KeyPositions[KeyIndex].InputValue;
						}
					}

					CopyableBuffer->Curves.Add(CopyableCurveKeys);
				}
			}
		}
	}
	else
	{
		TSet<FCurveModelID> CurveModelIDs;

		for (const TTuple<FCurveEditorTreeItemID, ECurveEditorTreeSelectionState>& Pair : GetTreeSelection())
		{
			if (Pair.Value == ECurveEditorTreeSelectionState::Explicit)
			{
				GetChildCurveModelIDs(Pair.Key, CurveModelIDs);
			}
		}

		for(const FCurveModelID& CurveModelID : CurveModelIDs)
		{
			if (FCurveModel* Curve = FindCurve(CurveModelID))
			{
				TUniquePtr<IBufferedCurveModel> CurveModelCopy = Curve->CreateBufferedCurveCopy();
				if (CurveModelCopy)
				{
					TArray<FKeyPosition> KeyPositions;
					CurveModelCopy->GetKeyPositions(KeyPositions);
					if (KeyPositions.Num() > 0)
					{
						UCurveEditorCopyableCurveKeys *CopyableCurveKeys = NewObject<UCurveEditorCopyableCurveKeys>(CopyableBuffer, UCurveEditorCopyableCurveKeys::StaticClass(), NAME_None, RF_Transient);

						CopyableCurveKeys->ShortDisplayName = Curve->GetShortDisplayName().ToString();
						CopyableCurveKeys->LongDisplayName = Curve->GetLongDisplayName().ToString();
						CopyableCurveKeys->IntentionName = Curve->GetIntentionName();

						CopyableCurveKeys->KeyPositions = KeyPositions;
						CurveModelCopy->GetKeyAttributes(CopyableCurveKeys->KeyAttributes);

						CopyableBuffer->Curves.Add(CopyableCurveKeys);
					}
				}
			}
		}

		// When copying entire curve objects we want absolute positions, so reset the detected offset
		KeyOffset.Reset();
	}

	if (KeyOffset.IsSet())
	{
		for (UCurveEditorCopyableCurveKeys* Curve : CopyableBuffer->Curves)
		{
			for (int Index = 0; Index < Curve->KeyPositions.Num(); ++Index)
			{
				Curve->KeyPositions[Index].InputValue -= KeyOffset.GetValue();
			}
		}

		CopyableBuffer->TimeOffset = KeyOffset.GetValue();
	}
	else
	{
		CopyableBuffer->bAbsolutePosition = true;
	}


	UExporter::ExportToOutputDevice(&Context, CopyableBuffer, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, CopyableBuffer);
	FPlatformApplicationMisc::ClipboardCopy(*Archive);
}

class FCurveEditorCopyableCurveKeysObjectTextFactory : public FCustomizableTextObjectFactory
{
public:
	FCurveEditorCopyableCurveKeysObjectTextFactory()
		: FCustomizableTextObjectFactory(GWarn)
	{
	}

	// FCustomizableTextObjectFactory implementation
	virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
	{
		if (InObjectClass->IsChildOf(UCurveEditorCopyBuffer::StaticClass()))
		{
			return true;
		}
		return false;
	}


	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		NewCopyBuffers.Add(Cast<UCurveEditorCopyBuffer>(NewObject));
	}

public:
	TArray<UCurveEditorCopyBuffer*> NewCopyBuffers;
};


bool FCurveEditor::CanPaste(const FString& TextToImport) const
{
	FCurveEditorCopyableCurveKeysObjectTextFactory CopyableCurveKeysFactory;
	if (CopyableCurveKeysFactory.CanCreateObjectsFromText(TextToImport))
	{
		return true;
	}
	return false;
}

void FCurveEditor::ImportCopyBufferFromText(const FString& TextToImport, /*out*/ TArray<UCurveEditorCopyBuffer*>& ImportedCopyBuffers) const
{
	UPackage* TempPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/Editor/CurveEditor/Transient"), RF_Transient);
	TempPackage->AddToRoot();

	// Turn the text buffer into objects
	FCurveEditorCopyableCurveKeysObjectTextFactory Factory;
	Factory.ProcessBuffer(TempPackage, RF_Transactional, TextToImport);

	ImportedCopyBuffers = Factory.NewCopyBuffers;

	// Remove the temp package from the root now that it has served its purpose
	TempPackage->RemoveFromRoot();
}

TSet<FCurveModelID> FCurveEditor::GetTargetCurvesForPaste() const
{
	TSet<FCurveModelID> TargetCurves;

	TArray<FCurveEditorTreeItemID> NodesToSearch;

	// Try nodes with selected keys
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		TargetCurves.Add(Pair.Key);
	}

	// Try selected nodes
	if (TargetCurves.Num() == 0)
	{
		for (const TTuple<FCurveEditorTreeItemID, ECurveEditorTreeSelectionState>& Pair : GetTreeSelection())
		{
			NodesToSearch.Add(Pair.Key);
		}
	}

	for (const FCurveEditorTreeItemID& TreeItemID : NodesToSearch)
	{
		const FCurveEditorTreeItem& TreeItem = GetTreeItem(TreeItemID);
		for (const FCurveModelID& CurveModelID : TreeItem.GetCurves())
		{
			TargetCurves.Add(CurveModelID);
		}
	}

	return TargetCurves;
}

bool FCurveEditor::CopyBufferCurveToCurveID(const UCurveEditorCopyableCurveKeys* InSourceCurve, const FCurveModelID InTargetCurve, TOptional<double> InTimeOffset, const bool bInAddToSelection, const bool bInOverwriteRange)
{
	FCurveModel* TargetCurve = FindCurve(InTargetCurve);
	if (!InSourceCurve || !TargetCurve)
	{
		return false;
	}

	// Sometimes when you paste you want to delete any keys that already exist in the timerange you'll be replacing
	// because mixing the pasted results with the original results wouldn't make any sense.
	if (bInOverwriteRange)
	{
		TArray<FKeyHandle> KeysToRemove;
		double MinKeyTime = TNumericLimits<double>::Max();
		double MaxKeyTime = TNumericLimits<double>::Lowest(); 
		for (int32 Index = 0; Index < InSourceCurve->KeyPositions.Num(); ++Index)
		{
			FKeyPosition KeyPosition = InSourceCurve->KeyPositions[Index];
			if (InTimeOffset.IsSet())
			{
				KeyPosition.InputValue += InTimeOffset.GetValue();
			}
			if (KeyPosition.InputValue < MinKeyTime)
			{
				MinKeyTime = KeyPosition.InputValue;
			}
			if (KeyPosition.InputValue > MaxKeyTime)
			{
				MaxKeyTime = KeyPosition.InputValue;
			}
		}

		// Just double checking we actually set a Min/Max time so we don't wipe out every key to infinity.
		if (InSourceCurve->KeyPositions.Num() > 0)
		{
			TargetCurve->GetKeys(*this, MinKeyTime, MaxKeyTime, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeysToRemove);
		}

		TargetCurve->RemoveKeys(KeysToRemove);
	}

	for (int32 Index = 0; Index < InSourceCurve->KeyPositions.Num(); ++Index)
	{
		FKeyPosition KeyPosition = InSourceCurve->KeyPositions[Index];
		if (InTimeOffset.IsSet())
		{
			KeyPosition.InputValue += InTimeOffset.GetValue();
		}

		TOptional<FKeyHandle> KeyHandle = TargetCurve->AddKey(KeyPosition, InSourceCurve->KeyAttributes[Index]);
		if (KeyHandle.IsSet() && bInAddToSelection)
		{
			Selection.Add(FCurvePointHandle(InTargetCurve, ECurvePointType::Key, KeyHandle.GetValue()));
		}
	}

	return true;
}

void FCurveEditor::PasteKeys(TSet<FCurveModelID> CurveModelIDs, const bool bInOverwriteRange)
{
	// Grab the text to paste from the clipboard
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	TArray<UCurveEditorCopyBuffer*> ImportedCopyBuffers;
	ImportCopyBufferFromText(TextToImport, ImportedCopyBuffers);

	if (ImportedCopyBuffers.Num() == 0)
	{
		return;
	}

	// There are numerous scenarios that Copy/Paste needs to handle.
	// 1:1				 - Copying a single curve to another single curve should always work.
	// 1:Multiple		 - Copying a single curve with multiple target curves should always work, the value will just be written into each one.
	// Multiple (Related): Multiple (Related) 
	//					 - Copying multiple curves between related controls, ie: fk_foot_l and fk_foot_r from one rig to another.
	//					 - If their long intent name matches, we consider them to be related controls. If their intent name doesn't match
	//					 - then we consider them unrelated controls.
	// Multiple (Unrelated):Multiple (Unrelated)
	//					 - If the long name doesn't match then we fall back to just the intent name. We want to handle copying both from one
	//					 - group of controls to multiple groups of controls, matching each by short intent name. This lets you copy fk_foot_l
	//					 - onto fk_foot_r and fk_spine_1 at the same time. We also handle trying to copy from multiple groups of controls
	//					 - onto multiple groups of controls - this falls back to a index-in-array order based copy and tries to ensure that
	//					 - the intent for each one (ie: transform.x) copies onto the first target transform.x, and then the next source that
	//					 - has a transform.x intent gets copied onto the *second* target transform.x.
	// Multiple (Unrelated):1
	//					 - This one is mostly an unhandled case and the last source intent will win on the target group, so fk_foot_l and fk_foot_r
	// 					 - pasted onto fk_spine_1, fk_spine_1 will just get the intents from fk_foot_r and fk_foot_l is ignored. This order isn't
	//					 - guranteed though because it's using the order the curves are in the internal arrays.
	
	// There should only be one copy buffer, but the way the import works returns an array.
	ensureMsgf(ImportedCopyBuffers.Num() == 1, TEXT("Multiple copy buffers pasted at one time, only the first one will be used!"));
	UCurveEditorCopyBuffer* SourceBuffer = ImportedCopyBuffers[0];

	// Figure out which CurveModelIDs we're trying to paste to. If they're not already specified, we try to find hovered curves,
	// and failing that we try to find all curves.
	TSet<FCurveModelID> TargetCurves = CurveModelIDs.Num() > 0 ? CurveModelIDs : GetTargetCurvesForPaste();
	
	if (TargetCurves.Num() == 0)
	{
		return;
	}

	// When we're pasting keys, we want the first key to paste where the timeslider is
	TOptional<double> TimeOffset;
	bool bApplyOffset = !SourceBuffer->bAbsolutePosition;

	if (bApplyOffset)
	{
		TSharedPtr<ITimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
		if (TimeSliderController.IsValid())
		{
			FFrameRate TickResolution = TimeSliderController->GetTickResolution();

			TimeOffset = TimeSliderController->GetScrubPosition() / TickResolution;
		}
		else
		{
			TimeOffset = SourceBuffer->TimeOffset;
		}
	}

	FScopedTransaction Transaction(LOCTEXT("PasteKeys", "Paste Keys"));
	Selection.Clear();


	// Two simple cases, 1 to 1 and 1 to many.
	TArray<TPair<UCurveEditorCopyableCurveKeys*, FCurveModelID>> CopyPairs;

	if (SourceBuffer->Curves.Num() == 1)
	{
		for (FCurveModelID TargetCurveID : TargetCurves)
		{
			CopyPairs.Add(TPair<UCurveEditorCopyableCurveKeys*, FCurveModelID>(SourceBuffer->Curves[0], TargetCurveID));
		}
	}
	else
	{
		// The more complicated is the Multiple:Multiple / Multiple:1 (which is really just the same). We want to
		// prioritize matching up longer names if possible - this allows us to copy multiple controls to multiple
		// controls, such as starting with fk_foot_l and fk_foot_r and pasting to fk_foot_l, fk_foot_r, fk_neck_01.
		// We will match up the transform/scale/rotation for the fk_foot_l/fk_foot_r and don't touch fk_neck_01 in this
		// example. If no matches are made, then we fall back to the shorter intent string - where we just copy
		// transform.xyz to transform.xyz even though the source may be fk_foot_l and the target is fk_foot_r.

		// If any of the long names match (ie: fk_foot_l.transform.x) then we'll use long name matching for all.
		bool bUseLongNameForMatches = false;
		for (const UCurveEditorCopyableCurveKeys* SourceCurveKeys : SourceBuffer->Curves)
		{
			for (const FCurveModelID& TargetCurveID : TargetCurves)
			{
				FCurveModel* TargetCurve = FindCurve(TargetCurveID);
				if (TargetCurve)
				{
					if (SourceCurveKeys->LongIntentionName == TargetCurve->GetLongIntentionName())
					{
						bUseLongNameForMatches = true;
						break;
					}
				}
			}

			// Exit out of the outer loop too if we've got a match.
			if (bUseLongNameForMatches)
			{
				break;
			}
		}

		// Multiple to Multiple curve copying can get complicated when we only have the short intent name to deal with it, so
		// this creates an edge case where you're copying one set of intents (ie: transform.x, transform.y, transform.z) onto
		// multiple objects with those intents... we want to support this, but we don't support copying from multiple objects
		// onto multiple objects unless their LongIntentionName matches as it gets too confusing to match up.
		bool bOnlyOneSetOfSourceIntentions = true;
		{
			TMap<FString, int32> IntentionUseCounts;
			for (UCurveEditorCopyableCurveKeys* SourceCurveKeys : SourceBuffer->Curves)
			{
				IntentionUseCounts.FindOrAdd(SourceCurveKeys->IntentionName)++;
			}

			for (TPair<FString, int32>& Pair : IntentionUseCounts)
			{
				if (Pair.Value > 1)
				{
					bOnlyOneSetOfSourceIntentions = false;
					break;
				}
			}
		}
		
		TSet<FCurveModelID> CurvesToMatchTo = TargetCurves;
		for (UCurveEditorCopyableCurveKeys* SourceCurveKeys : SourceBuffer->Curves)
		{
			TArray<FCurveModelID> CurvesToRemove;
			for (const FCurveModelID& TargetCurveID : CurvesToMatchTo)
			{
				FCurveModel* TargetCurve = FindCurve(TargetCurveID);
				if (TargetCurve)
				{
					const bool bNameMatches = bUseLongNameForMatches ?
						SourceCurveKeys->LongIntentionName == TargetCurve->GetLongIntentionName() :
						SourceCurveKeys->IntentionName == TargetCurve->GetIntentionName();

					if (bNameMatches)
					{
						CopyPairs.Add(TPair<UCurveEditorCopyableCurveKeys*, FCurveModelID>(SourceCurveKeys, TargetCurveID));

						// Don't try to match to this curve again. This lets us try to handle the case where we have
						// multiple source objects (fk_foot_l, fk_foot_r) trying to copy to unrelated objects (cube1, cube2).
						// They will fail the LongDisplayName check but get the IntentionName check, but we need to remove
						// cube1 after the first time we match it so that fk_foot_r has a chance to paste into cube2 instead of cube1.
						CurvesToRemove.Add(TargetCurveID);

						// If we're copying from one object with multiple curves (ie: fk_foot_l) but we have multiple destination
						// objects, we loop through all of the target curves and apply them using the IntentionName matches check.
						// This only happens when using short intention names (as it's the more vague logic case), and we only
						// do this when you have multiple source curves, but only one of each kind. If you have multiple source
						// curves with multiple copies of the same intention, then we only apply it once to the first curve
						// who's intention matches and then remove it from the pool so that the next source with the same
						// intention (such as the second foot in the above example) gets a chance to write to the second
						// target curve with the same destination.
						bool bCopyToMultipleDestCurves = bOnlyOneSetOfSourceIntentions && !bUseLongNameForMatches;
						if (!bCopyToMultipleDestCurves)
						{
							break;
						}
					}
				}
			}

			for (FCurveModelID Curve : CurvesToRemove)
			{
				CurvesToMatchTo.Remove(Curve);
			}
		}
	}

	// Now that we've calculated the source curve for each destination curve, copy them over.
	for (const TPair<UCurveEditorCopyableCurveKeys*, FCurveModelID>& Pair : CopyPairs)
	{
		const bool bAddToSelection = true;
		CopyBufferCurveToCurveID(Pair.Key, Pair.Value, TimeOffset, bAddToSelection, bInOverwriteRange);
	}

	if (ShouldAutoFrame())
	{
		ZoomToFitSelection();
	}
}

void FCurveEditor::DeleteSelection()
{
	FScopedTransaction Transaction(LOCTEXT("DeleteKeys", "Delete Keys"));

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			Curve->Modify();
			Curve->RemoveKeys(Pair.Value.AsArray());
		}
	}

	Selection.Clear();
}

void FCurveEditor::FlattenSelection()
{
	FScopedTransaction Transaction(LOCTEXT("FlattenTangents", "Flatten Tangents"));
	bool bFoundAnyTangents = false;

	TArray<FKeyHandle> KeyHandles;
	TArray<FKeyAttributes> AllKeyPositions;
	//Since we don't have access here to the Section to get Tick Resolution if we flatten a weighted tangent we
	//do so by converting it to non-weighted and then back again.
	TArray<FKeyHandle>  KeyHandlesWeighted;
	TArray<FKeyAttributes> KeyAttributesWeighted;
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			KeyHandles.Reset(Pair.Value.Num());
			KeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			AllKeyPositions.SetNum(KeyHandles.Num());
			Curve->GetKeyAttributes(KeyHandles, AllKeyPositions);

			KeyHandlesWeighted.Reset(Pair.Value.Num());
			KeyHandlesWeighted.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			KeyAttributesWeighted.SetNum(KeyHandlesWeighted.Num());
			Curve->GetKeyAttributes(KeyHandlesWeighted, KeyAttributesWeighted);


			// Straighten tangents, ignoring any keys that we can't set tangents on
			for (int32 Index = AllKeyPositions.Num()-1 ; Index >= 0; --Index)
			{
				FKeyAttributes& Attributes = AllKeyPositions[Index];
				if (Attributes.HasTangentMode() && (Attributes.HasArriveTangent() || Attributes.HasLeaveTangent()))
				{
					Attributes.SetArriveTangent(0.f).SetLeaveTangent(0.f);
					if (Attributes.GetTangentMode() == RCTM_Auto || Attributes.GetTangentMode() == RCTM_SmartAuto)
					{
						Attributes.SetTangentMode(RCTM_User);
					}
					//if any weighted convert and convert back to both (which is what only support other modes are not really used).,
					if (Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedArrive
						|| Attributes.GetTangentWeightMode() == RCTWM_WeightedLeave)
					{
						Attributes.SetTangentWeightMode(RCTWM_WeightedNone);
						FKeyAttributes& WeightedAttributes = KeyAttributesWeighted[Index];
						WeightedAttributes.UnsetArriveTangent();
						WeightedAttributes.UnsetLeaveTangent();
						WeightedAttributes.UnsetArriveTangentWeight();
						WeightedAttributes.UnsetLeaveTangentWeight();
						WeightedAttributes.SetTangentWeightMode(RCTWM_WeightedBoth);

					}
					else
					{
						KeyAttributesWeighted.RemoveAtSwap(Index, 1, EAllowShrinking::No);
						KeyHandlesWeighted.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					}
				}
				else
				{
					AllKeyPositions.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					KeyHandles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					KeyAttributesWeighted.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					KeyHandlesWeighted.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				}
			}

			if (AllKeyPositions.Num() > 0)
			{
				Curve->Modify();
				Curve->SetKeyAttributes(KeyHandles, AllKeyPositions);
				if (KeyAttributesWeighted.Num() > 0)
				{
					Curve->SetKeyAttributes(KeyHandlesWeighted, KeyAttributesWeighted);
				}
				bFoundAnyTangents = true;
			}
		}
	}

	if (!bFoundAnyTangents)
	{
		Transaction.Cancel();
	}
}

void FCurveEditor::StraightenSelection()
{
	FScopedTransaction Transaction(LOCTEXT("StraightenTangents", "Straighten Tangents"));
	bool bFoundAnyTangents = false;

	TArray<FKeyHandle> KeyHandles;
	TArray<FKeyAttributes> AllKeyPositions;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			KeyHandles.Reset(Pair.Value.Num());
			KeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			AllKeyPositions.SetNum(KeyHandles.Num());
			Curve->GetKeyAttributes(KeyHandles, AllKeyPositions);

			// Straighten tangents, ignoring any keys that we can't set tangents on
			for (int32 Index = AllKeyPositions.Num()-1 ; Index >= 0; --Index)
			{
				FKeyAttributes& Attributes = AllKeyPositions[Index];
				if (Attributes.HasTangentMode() && Attributes.HasArriveTangent() && Attributes.HasLeaveTangent())
				{
					float NewTangent = (Attributes.GetLeaveTangent() + Attributes.GetArriveTangent()) * 0.5f;
					Attributes.SetArriveTangent(NewTangent).SetLeaveTangent(NewTangent);
					if (Attributes.GetTangentMode() == RCTM_Auto || Attributes.GetTangentMode() == RCTM_SmartAuto)
					{
						Attributes.SetTangentMode(RCTM_User);
					}
				}
				else
				{
					AllKeyPositions.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					KeyHandles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				}
			}

			if (AllKeyPositions.Num() > 0)
			{
				Curve->Modify();
				Curve->SetKeyAttributes(KeyHandles, AllKeyPositions);
				bFoundAnyTangents = true;
			}
		}
	}

	if (!bFoundAnyTangents)
	{
		Transaction.Cancel();
	}
}

bool FCurveEditor::CanFlattenOrStraightenSelection() const
{
	return Selection.Count() > 0;
}

void FCurveEditor::SetRandomCurveColorsForSelected()
{
	TSet<FCurveModelID> CurveModelIDs = GetSelectionFromTreeAndKeys();
	if (CurveModelIDs.Num() == 0)
	{
		return;
	}

	for (const FCurveModelID& CurveModelID : CurveModelIDs)
	{
		if (FCurveModel* Curve = FindCurve(CurveModelID))
		{
			UObject* Object = nullptr;
			FString Name;
			Curve->GetCurveColorObjectAndName(&Object, Name);
			if (Object)
			{
				FLinearColor Color = UCurveEditorSettings::GetNextRandomColor();
				Settings->SetCustomColor(Object->GetClass(), Name, Color);
				Curve->SetColor(Color);
			}
		}
	}
}

void FCurveEditor::SetCurveColorsForSelected()
{
	TSet<FCurveModelID> CurveModelIDs = GetSelectionFromTreeAndKeys();
	if (CurveModelIDs.Num() == 0)
	{
		return;
	}

	TWeakPtr<FCurveEditor> WeakSelf = AsShared();

	FColorPickerArgs PickerArgs;
	PickerArgs.bUseAlpha = false;
	PickerArgs.InitialColor = FindCurve(*CurveModelIDs.CreateIterator())->GetColor();
	PickerArgs.OnColorCommitted.BindLambda([WeakSelf, CurveModelIDs](FLinearColor NewColor)
	{
		if (TSharedPtr<FCurveEditor> Self = WeakSelf.Pin())
		{
			for (const FCurveModelID& CurveModelID : CurveModelIDs)
			{
				if (FCurveModel* Curve = Self->FindCurve(CurveModelID))
				{
					UObject* Object = nullptr;
					FString Name;
					Curve->GetCurveColorObjectAndName(&Object, Name);
					if (Object)
					{
						Self->Settings->SetCustomColor(Object->GetClass(), Name, NewColor);
						Curve->SetColor(NewColor);
					}
				}
			}
		}
	});
		
	OpenColorPicker(PickerArgs);
}

bool FCurveEditor::IsToolActive(const FCurveEditorToolID InToolID) const
{
	if (ActiveTool.IsSet())
	{
		return ActiveTool == InToolID;
	}

	return false;
}

void FCurveEditor::MakeToolActive(const FCurveEditorToolID InToolID)
{
	if (ActiveTool.IsSet())
	{
		// Early out in the event that they're trying to switch to the same tool. This avoids
		// unwanted activation/deactivation calls.
		if (ActiveTool == InToolID)
		{
			return;
		}

		// Deactivate the current tool before we activate the new one.
		ToolExtensions[ActiveTool.GetValue()]->OnToolDeactivated();
	}

	ActiveTool.Reset();

	// Notify anyone listening that we've switched tools (possibly to an inactive one)
	OnActiveToolChangedDelegate.Broadcast(InToolID);

	if (InToolID != FCurveEditorToolID::Unset())
	{
		ActiveTool = InToolID;
		ToolExtensions[ActiveTool.GetValue()]->OnToolActivated();
	}
}

ICurveEditorToolExtension* FCurveEditor::GetCurrentTool() const
{
	if (ActiveTool.IsSet())
	{
		return ToolExtensions[ActiveTool.GetValue()].Get();
	}

	// If there is no active tool we return nullptr.
	return nullptr;
}

TSet<FCurveModelID> FCurveEditor::GetEditedCurves() const
{
	TArray<FCurveModelID> AllCurves;
	GetCurves().GenerateKeyArray(AllCurves);
	return TSet<FCurveModelID>(AllCurves);
}

void FCurveEditor::AddBufferedCurves(const TSet<FCurveModelID>& InCurves)
{
	// We make a copy of the curve data and store it.
	for (FCurveModelID CurveID : InCurves)
	{
		FCurveModel* CurveModel = FindCurve(CurveID);
		check(CurveModel);

		// Add a buffered curve copy if the curve model supports buffered curves
		TUniquePtr<IBufferedCurveModel> CurveModelCopy = CurveModel->CreateBufferedCurveCopy();
		if (CurveModelCopy) 
		{
			// Remove any existing buffered curves
			for (int32 BufferedCurveIndex = 0; BufferedCurveIndex < BufferedCurves.Num(); )
			{
				if (BufferedCurves[BufferedCurveIndex]->GetLongDisplayName() == CurveModel->GetLongDisplayName().ToString())
				{
					BufferedCurves.RemoveAt(BufferedCurveIndex);
				}
				else
				{
					++BufferedCurveIndex;
				}
			}

			BufferedCurves.Add(MoveTemp(CurveModelCopy)); 
		}
		else
		{
			UE_LOG(LogCurveEditor, Warning, TEXT("Failed to buffer curve, curve model did not provide a copy."))
		}
	}
}


void FCurveEditor::ApplyBufferedCurveToTarget(const IBufferedCurveModel* BufferedCurve, FCurveModel* TargetCurve)
{
	check(TargetCurve);
	check(BufferedCurve);

	TArray<FKeyPosition> KeyPositions;
	TArray<FKeyAttributes> KeyAttributes;
	BufferedCurve->GetKeyPositions(KeyPositions);
	BufferedCurve->GetKeyAttributes(KeyAttributes);


	// Copy the data from the Buffered curve into the target curve. This just does wholesale replacement.
	TArray<FKeyHandle> TargetKeyHandles;
	TargetCurve->GetKeys(*this, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TargetKeyHandles);

	// Clear our current keys from the target curve
	TargetCurve->RemoveKeys(TargetKeyHandles);

	// Now put our buffered keys into the target curve
	TargetCurve->AddKeys(KeyPositions, KeyAttributes);
}

bool FCurveEditor::ApplyBufferedCurves(const TSet<FCurveModelID>& InCurvesToApplyTo, const bool bSwapBufferCurves)
{
	FScopedTransaction Transaction(bSwapBufferCurves ? LOCTEXT("SwapBufferedCurves", "Swap Buffered Curves") : LOCTEXT("ApplyBufferedCurves", "Apply Buffered Curves"));

	// Each curve can specify an "Intention" name. This gives a little bit of context about how the curve is intended to be used,
	// without locking anyone into a specific set of intentions. When you go to apply the buffered curves, for each curve that you
	// want to apply it to, we can look in our stored curves to see if someone has the same intention. If there isn't a matching intention
	// then we skip and consider a fallback method (such as 1:1 copy). There is a lot of guessing still involved as there are complex
	// situations that users may try to use it in (such as buffering two sets of transform curves and applying it to two destination transform curves)
	// or trying to copy something with a name like "Focal Length" and pasting it onto a different track. We don't handle these cases for now,
	// but attempt to communicate it to the user via  toast notification when pasting fails for whatever reason.
	int32 NumCurvesMatchedByIntent = 0;
	int32 NumCurvesNoMatchedIntent = 0;
	bool bFoundAnyMatchedIntent = false;

	TMap<FString, int32> IntentMatchIndexes;

	for (const FCurveModelID& CurveModelID : InCurvesToApplyTo)
	{
		FCurveModel* TargetCurve = FindCurve(CurveModelID);
		check(TargetCurve);

		// Figure out what our destination thinks it's supposed to be used for, ie "Location.X"
		FString TargetIntent = TargetCurve->GetLongDisplayName().ToString();
		if (TargetIntent.IsEmpty())
		{
			// We don't try to match curves with no intent as that's just chaos.
			NumCurvesNoMatchedIntent++;
			continue;
		}

		TargetCurve->Modify();

		// In an attempt to support buffering multiple curves with the same intention, we'll try to match them up in pairs. This means
		// for the first curve that we're trying to apply to, if the intention is "Location.X" we will search the buffered curves for a
		// "Location.X". Upon finding one, we store the index that it was found at, so the next time we try to find a curve with the same
		// intention, we look for the second "Location.X" and so forth. If we don't find a second "Location.X" in our buffered curves we'll
		// fall back to the first buffered one so you can 1:Many copy a curve.
		int32 BufferedCurveSearchIndexStart = 0;
		const int32* PreviouslyFoundIntent = IntentMatchIndexes.Find(TargetIntent);
		if (PreviouslyFoundIntent)
		{
			// Start our search on the next item in the array. If we don't find one, we'll fall back to the last one.
			BufferedCurveSearchIndexStart = IntentMatchIndexes[TargetIntent] + 1;
		}

		int32 MatchedBufferedCurveIndex = -1;
		for (int32 BufferedCurveIndex = BufferedCurveSearchIndexStart; BufferedCurveIndex < BufferedCurves.Num(); BufferedCurveIndex++)
		{
			if (BufferedCurves[BufferedCurveIndex]->GetLongDisplayName() == TargetIntent)
			{
				MatchedBufferedCurveIndex = BufferedCurveIndex;

				// Update our previously found intent to the latest one.
				IntentMatchIndexes.FindOrAdd(TargetIntent) = MatchedBufferedCurveIndex;
				break;
			}
		}

		// The Intent Match Indexes stores the latest index to find a valid curve, or the last one if no new valid one was found.
		// If there is an entry in the match indexes now, we can use that to figure out which buffered curve we'll pull from.
		// If we didn't find any more with the same intention, we fall back to the existing one (if it exists!)
		if (IntentMatchIndexes.Find(TargetIntent))
		{
			MatchedBufferedCurveIndex = IntentMatchIndexes[TargetIntent];
		}

		// Finally, we can try to use the matched curve if one was found.
		if (MatchedBufferedCurveIndex >= 0)
		{
			// We successfully matched, so count that one up!
			NumCurvesMatchedByIntent++;
			bFoundAnyMatchedIntent = true;

			const IBufferedCurveModel* BufferedCurve = BufferedCurves[MatchedBufferedCurveIndex].Get();

			TUniquePtr<IBufferedCurveModel> CurveModelCopy;
			if (bSwapBufferCurves)
			{
				CurveModelCopy = TargetCurve->CreateBufferedCurveCopy();
			}

			ApplyBufferedCurveToTarget(BufferedCurve, TargetCurve);

			if (bSwapBufferCurves)
			{
				BufferedCurves[MatchedBufferedCurveIndex] = MoveTemp(CurveModelCopy);
			}
		}
		else
		{
			// We couldn't find a match despite our best efforts
			NumCurvesNoMatchedIntent++;
		}
	}

	// If we managed to match any by intent, we're going to early out and assume that's what their intent was.
	if (bFoundAnyMatchedIntent)
	{
		const FText NotificationText = FText::Format(LOCTEXT("MatchedBufferedCurvesByIntent", "Applied {0}/{1} buffered curves to {2}/{3} target curves."),
			FText::AsNumber(IntentMatchIndexes.Num()), FText::AsNumber(BufferedCurves.Num()),		// We used X of Y total buffered curves
			FText::AsNumber(NumCurvesMatchedByIntent), FText::AsNumber(InCurvesToApplyTo.Num()));	// To apply to Z of W target curves,

		FNotificationInfo Info(NotificationText);
		Info.ExpireDuration = 6.f;
		Info.bUseLargeFont = false;
		Info.bUseSuccessFailIcons = false;
		FSlateNotificationManager::Get().AddNotification(Info);

		if (NumCurvesNoMatchedIntent > 0)
		{
			const FText FailedNotificationText = FText::Format(LOCTEXT("NumCurvesNotMatchedByIntent", "Failed to find a buffered curve with the same intent for {0} target curves, skipping..."),
				FText::AsNumber(NumCurvesNoMatchedIntent));												// Leaving V many target curves unaffected due to no intent match.

			FNotificationInfo FailInfo(FailedNotificationText);
			FailInfo.ExpireDuration = 6.f;
			FailInfo.bUseLargeFont = false;
			FailInfo.bUseSuccessFailIcons = true;
			FSlateNotificationManager::Get().AddNotification(FailInfo);
		}

		// Early out
		return true;
	}

	// If we got this far, it means that the buffered curves have no recognizable relation to the target curves.
	// If the number of curves match, we'll just do a 1:1 mapping. This works for most cases where you're trying
	// to paste an unrelated curve onto another as it's likely that there's only one curve. We don't limit it to
	// one curve though, we'll just warn...
	if (InCurvesToApplyTo.Num() == BufferedCurves.Num())
	{
		// This will work great in the case there's only one curve. It'll guess if there's more than one, relying on
		// sets with no guaranteed order.
		TArray<FCurveModelID> CurvesToApplyTo = InCurvesToApplyTo.Array();
		
		for (int32 CurveIndex = 0; CurveIndex < InCurvesToApplyTo.Num(); CurveIndex++)
		{
			FCurveModel* TargetCurve = FindCurve(CurvesToApplyTo[CurveIndex]);

			TUniquePtr<IBufferedCurveModel> CurveModelCopy;
			if (bSwapBufferCurves)
			{
				CurveModelCopy = TargetCurve->CreateBufferedCurveCopy();
			}

			ApplyBufferedCurveToTarget(BufferedCurves[CurveIndex].Get(), TargetCurve);

			if (bSwapBufferCurves)
			{
				BufferedCurves[CurveIndex] = MoveTemp(CurveModelCopy);
			}
		}

		FText NotificationText;
		if (InCurvesToApplyTo.Num() == 1)
		{
			NotificationText = LOCTEXT("MatchedBufferedCurvesBySolo", "Applied buffered curve to target curve with no intention matching.");
		}
		else
		{
			NotificationText = LOCTEXT("MatchedBufferedCurvesByIndex", "Applied buffered curves with no intention matching. Order not guranteed.");
		}

		FNotificationInfo Info(NotificationText);
		Info.ExpireDuration = 6.f;
		Info.bUseLargeFont = false;
		Info.bUseSuccessFailIcons = false;
		FSlateNotificationManager::Get().AddNotification(Info);

		// Early out
		return true;
	}

	// If we got this far, we have no idea what to do. They're trying to match a bunch of curves with no intention and different amounts. 
	// Warn of failure and give up.
	{
		const FText FailedNotificationText = LOCTEXT("NoBufferedCurvesMatched", "Failed to apply buffered curves, apply them one at a time instead.");

		FNotificationInfo FailInfo(FailedNotificationText);
		FailInfo.ExpireDuration = 6.f;
		FailInfo.bUseLargeFont = false;
		FailInfo.bUseSuccessFailIcons = true;
		FSlateNotificationManager::Get().AddNotification(FailInfo);
	}

	// No need to make a entry in the Undo/Redo buffer if it didn't apply anything.
	Transaction.Cancel();
	return false;
}

TSet<FCurveModelID> FCurveEditor::GetSelectionFromTreeAndKeys() const
{
	TSet<FCurveModelID> CurveModelIDs;

	// Buffer curves operates on the selected curves (tree selection or key selection)
	for (const TTuple<FCurveEditorTreeItemID, ECurveEditorTreeSelectionState>& Pair : GetTreeSelection())
	{
		if (Pair.Value == ECurveEditorTreeSelectionState::Explicit)
		{
			const FCurveEditorTreeItem& TreeItem = GetTreeItem(Pair.Key);
			for (const FCurveModelID& CurveModelID : TreeItem.GetCurves())
			{
				CurveModelIDs.Add(CurveModelID);
			}
		}
	}

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		CurveModelIDs.Add(Pair.Key);
	}

	return CurveModelIDs;
}

bool FCurveEditor::IsActiveBufferedCurve(const TUniquePtr<IBufferedCurveModel>& BufferedCurve) const
{
	TSet<FCurveModelID> CurveModelIDs = GetSelectionFromTreeAndKeys();
	for (const FCurveModelID& CurveModelID : CurveModelIDs)
	{
		if (FCurveModel* Curve = FindCurve(CurveModelID))
		{
			if (Curve->GetLongDisplayName().ToString() == BufferedCurve.Get()->GetLongDisplayName())
			{
				return true;
			}
		}
	}

	return false;
}

void FCurveEditor::PostUndo(bool bSuccess)
{
	if (WeakPanel.IsValid())
	{
		WeakPanel.Pin()->PostUndo();
	}

	// If you create keys and then undo them the selection set still thinks there's keys selected.
	// This presents issues with context menus and other things that are activated when there is a selection set.
	// To fix this, we have to loop through all of our curve models, and re-select only the key handles that were
	// previously selected that still exist. Ugly, but reasonably functional.
	TMap<FCurveModelID, FKeyHandleSet> SelectionSet = Selection.GetAll();
	for (const TPair<FCurveModelID, FKeyHandleSet>& Set : SelectionSet)
	{
		FCurveModel* CurveModel = FindCurve(Set.Key);

		// If the entire curve was removed, just dump that out of the selection set.
		if (!CurveModel)
		{
			Selection.Remove(Set.Key);
			continue;
		}
		// Get all of the key handles from this curve.
		TArray<FKeyHandle> KeyHandles;
		CurveModel->GetKeys(*this, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);

		// The set handles will be mutated as we remove things so we need a copy that we can iterate through.
		TArrayView<const FKeyHandle> SelectedHandles = Set.Value.AsArray();
		TArray<FKeyHandle> NonMutableArray = TArray<FKeyHandle>(SelectedHandles.GetData(), SelectedHandles.Num());
		
		for (const FKeyHandle& Handle : NonMutableArray)
		{
			// Check to see if our curve model contains this handle still.
			if (!KeyHandles.Contains(Handle))
			{
				Selection.Remove(Set.Key, ECurvePointType::Key, Handle);
			}
		}
	}
}

void FCurveEditor::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

void FCurveEditor::OnCustomColorsChanged()
{
	for (TPair<FCurveModelID, TUniquePtr<FCurveModel>>& CurvePair : CurveData)
	{
		if (FCurveModel* Curve = CurvePair.Value.Get())
		{
			UObject* Object = nullptr;
			FString Name;
			Curve->GetCurveColorObjectAndName(&Object, Name);

			TOptional<FLinearColor> Color = Settings->GetCustomColor(Object->GetClass(), Name);
			if (Color.IsSet())
			{
				Curve->SetColor(Color.GetValue());
			}
			else
			{
				// Note: If the color is no longer defined, there's no way to update with the previously defined 
				// default color. The curve models would need to be rebuilt, but would cause selection/framing and 
				// other things to change. So, this is intentionally not implemented.
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
