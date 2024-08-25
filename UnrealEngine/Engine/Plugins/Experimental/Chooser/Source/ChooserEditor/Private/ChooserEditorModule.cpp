// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserEditorModule.h"

#include "AnimNode_ChooserPlayer.h"
#include "BoolColumnEditor.h"
#include "ChooserEditorStyle.h"
#include "ChooserPropertyAccess.h"
#include "ChooserTableEditor.h"
#include "ChooserTableEditorCommands.h"
#include "CurveOverrideCustomization.h"
#include "EnumColumnEditor.h"
#include "Features/IModularFeatures.h"
#include "FloatRangeColumnEditor.h"
#include "FrameTimeCustomization.h"
#include "GameplayTagColumnEditor.h"
#include "IAssetTools.h"
#include "ObjectColumnEditor.h"
#include "OutputFloatColumnEditor.h"
#include "OutputStructColumnEditor.h"
#include "PropertyAccessChainCustomization.h"
#include "PropertyEditorModule.h"
#include "RandomizeColumnEditor.h"
#include "ChooserTrack.h"

#define LOCTEXT_NAMESPACE "ChooserEditorModule"

namespace UE::ChooserEditor
{
	
FChoosersTrackCreator GChoosersTrackCreator;

void FModule::StartupModule()
{
	FChooserEditorStyle::Initialize();
	
	FChooserTableEditor::RegisterWidgets();
	RegisterGameplayTagWidgets();
	RegisterFloatRangeWidgets();
	RegisterOutputFloatWidgets();
	RegisterBoolWidgets();
	RegisterEnumWidgets();
	RegisterObjectWidgets();
	RegisterStructWidgets();
	RegisterRandomizeWidgets();
	
	FChooserTableEditorCommands::Register();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	PropertyModule.RegisterCustomPropertyTypeLayout("FloatProperty", FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FFrameTimeCustomization>(); }), MakeShared<FFrameTimePropertyTypeIdentifier>());
	PropertyModule.RegisterCustomPropertyTypeLayout(FAnimCurveOverride::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FCurveOverrideCustomization>(); }));
	PropertyModule.RegisterCustomPropertyTypeLayout(FAnimCurveOverrideList::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FCurveOverrideListCustomization>(); }));
	PropertyModule.RegisterCustomPropertyTypeLayout(FChooserPropertyBinding::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FPropertyAccessChainCustomization>(); }));
	PropertyModule.RegisterCustomPropertyTypeLayout(FChooserEnumPropertyBinding::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FPropertyAccessChainCustomization>(); }));
	PropertyModule.RegisterCustomPropertyTypeLayout(FChooserObjectPropertyBinding::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FPropertyAccessChainCustomization>(); }));
	PropertyModule.RegisterCustomPropertyTypeLayout(FChooserStructPropertyBinding::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FPropertyAccessChainCustomization>(); }));

	IModularFeatures::Get().RegisterModularFeature( IRewindDebuggerExtension::ModularFeatureName, &RewindDebuggerChooser);
	IModularFeatures::Get().RegisterModularFeature(RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName, &GChoosersTrackCreator);
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, &ChooserTraceModule);
}

void FModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature( IRewindDebuggerExtension::ModularFeatureName, &RewindDebuggerChooser);
	IModularFeatures::Get().UnregisterModularFeature(RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName, &GChoosersTrackCreator);
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, &ChooserTraceModule);
	FChooserTableEditorCommands::Unregister();
	
	FChooserEditorStyle::Shutdown();
}

}

IMPLEMENT_MODULE(UE::ChooserEditor::FModule, ChooserEditor);

#undef LOCTEXT_NAMESPACE