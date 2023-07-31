// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/EditorViewModel.h"

#include "CoreTypes.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/TrackAreaViewModel.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"

namespace UE
{
namespace Sequencer
{

EViewModelListType FEditorViewModel::GetEditorPanelType()
{
	static EViewModelListType EditorPanelType = RegisterCustomModelListType();
	return EditorPanelType;
}

FEditorViewModel::FEditorViewModel()
	: PanelList(GetEditorPanelType())
{
	RegisterChildList(&PanelList);
}

FEditorViewModel::~FEditorViewModel()
{
}

void FEditorViewModel::InitializeEditor()
{
	ensureMsgf(!Outliner && !TrackArea, TEXT("This editor view-model has already been initialized"));

	PreInitializeEditorImpl();

	FViewModelChildren PanelChildren = GetEditorPanels();

	Outliner = CreateOutlinerImpl();
	PanelChildren.AddChild(Outliner);

	TrackArea = CreateTrackAreaImpl();
	PanelChildren.AddChild(TrackArea);

	RootDataModel = CreateRootModelImpl();
	TWeakPtr<FViewModel> WeakRootDataModel(RootDataModel);
	Outliner->Initialize(FWeakViewModelPtr(WeakRootDataModel));

	InitializeEditorImpl();
}

FViewModelPtr FEditorViewModel::GetRootModel() const
{
	return FViewModelPtr(RootDataModel);
}

FViewModelChildren FEditorViewModel::GetEditorPanels()
{
	return GetChildrenForList(&PanelList);
}

TSharedPtr<FOutlinerViewModel> FEditorViewModel::GetOutliner() const
{
	return Outliner;
}

TSharedPtr<FTrackAreaViewModel> FEditorViewModel::GetTrackArea() const
{
	return TrackArea;
}

TSharedPtr<FViewModel> FEditorViewModel::CreateRootModelImpl()
{
	return MakeShared<FViewModel>();
}

TSharedPtr<FOutlinerViewModel> FEditorViewModel::CreateOutlinerImpl()
{
	return MakeShared<FOutlinerViewModel>();
}

TSharedPtr<FTrackAreaViewModel> FEditorViewModel::CreateTrackAreaImpl()
{
	return MakeShared<FTrackAreaViewModel>();
}

bool FEditorViewModel::IsReadOnly() const
{
	return false;
}

} // namespace Sequencer
} // namespace UE

