// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scripting/SequencerScriptingLayer.h"
#include "Scripting/OutlinerScriptingObject.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/Selection/SequencerCoreSelection.h"


void USequencerScriptingLayer::Initialize(TSharedPtr<UE::Sequencer::FEditorViewModel> InViewModel)
{
	Outliner = NewObject<USequencerOutlinerScriptingObject>(this, "Outliner");
	Outliner->Initialize(InViewModel->GetOutliner());
}