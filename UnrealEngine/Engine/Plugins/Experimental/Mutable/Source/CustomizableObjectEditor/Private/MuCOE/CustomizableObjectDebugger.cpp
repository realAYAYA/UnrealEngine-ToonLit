// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectDebugger.h"

#include "Framework/Docking/TabManager.h"
#include "Internationalization/Internationalization.h"
#include "Logging/LogMacros.h"
#include "Misc/Attribute.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SMutableObjectViewer.h"
#include "Styling/ISlateStyle.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Types/SlateEnums.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"

struct FSlateBrush;


#define LOCTEXT_NAMESPACE "CustomizableObjectDebugger"

DEFINE_LOG_CATEGORY_STATIC(LogCustomizableObjectDebugger, Log, All);
const FName FCustomizableObjectDebugger::MutableNewTabId(TEXT("CustomizableObjectDebugger_NewTab"));


void FCustomizableObjectDebugger::InitCustomizableObjectDebugger( 
	const EToolkitMode::Type Mode, 
	const TSharedPtr< class IToolkitHost >& InitToolkitHost, 
	UCustomizableObject* ObjectToEdit )
{
	CustomizableObject = ObjectToEdit;

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_CustomizableObjectDebugger_Layout_v3" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Horizontal)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.6f)
			->AddTab(MutableNewTabId, ETabState::ClosedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = false;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, CustomizableObjectDebuggerAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit );

	// Open a tab for the object being debugged.
	TSharedPtr<SDockTab> NewMutableObjectTab = SNew(SDockTab)
		.Label(FText::FromString(FString::Printf(TEXT("Object [%s]"),*CustomizableObject->GetName())))
		[
			SNew(SMutableObjectViewer, CustomizableObject, TabManager, MutableNewTabId)
		];

	TabManager->InsertNewDocumentTab(MutableNewTabId, FTabManager::ESearchPreference::PreferLiveTab, NewMutableObjectTab.ToSharedRef());

}


const FSlateBrush* FCustomizableObjectDebugger::GetDefaultTabIcon() const
{
	return FCustomizableObjectEditorStyle::Get().GetBrush("CustomizableObjectEditor.Debug");
}


FName FCustomizableObjectDebugger::GetToolkitFName() const
{
	return FName("CustomizableObjectDebugger");
}


FText FCustomizableObjectDebugger::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Customizable Object Editor");
}


void FCustomizableObjectDebugger::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CustomizableObject );
}


FText FCustomizableObjectDebugger::GetToolkitName() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ObjectName"), FText::FromString(GetEditingObject()->GetName()));
	Args.Add(TEXT("ToolkitName"), GetBaseToolkitName());
	return FText::Format(LOCTEXT("AppLabelWithAssetName", "Debug {ObjectName} - {ToolkitName}"), Args);
}


FString FCustomizableObjectDebugger::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("DebuggerWorldCentricTabPrefix", "CustomizableObjectDebugger ").ToString();
}


FLinearColor FCustomizableObjectDebugger::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}


#undef LOCTEXT_NAMESPACE
