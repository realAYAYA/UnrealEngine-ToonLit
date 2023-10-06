// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRewindDebuggerDetails.h"
#include "ActorPickerMode.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Modules/ModuleManager.h"
#include "ObjectTrace.h"
#include "RewindDebuggerStyle.h"
#include "RewindDebuggerCommands.h"
#include "SSimpleTimeSlider.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerModule.h"
#include "Selection.h"
#include "Styling/SlateIconFinder.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "IRewindDebuggerViewCreator.h"
#include "RewindDebuggerViewCreators.h"
#include "IRewindDebuggerDoubleClickHandler.h"

#define LOCTEXT_NAMESPACE "SRewindDebuggerDetails"

SRewindDebuggerDetails::SRewindDebuggerDetails() 
	: SCompoundWidget()
{ 
}

SRewindDebuggerDetails::~SRewindDebuggerDetails() 
{
}

void SRewindDebuggerDetails::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
}


#undef LOCTEXT_NAMESPACE
