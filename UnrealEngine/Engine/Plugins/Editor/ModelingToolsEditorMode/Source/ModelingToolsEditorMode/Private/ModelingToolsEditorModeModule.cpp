// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingToolsEditorModeModule.h"

#include "ModelingToolsActions.h"
#include "ModelingToolsManagerActions.h"
#include "ModelingToolsEditorModeStyle.h"
#include "ModelingToolsEditorModeSettings.h"

#include "Misc/CoreDelegates.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "DetailsCustomizations/ModelingToolPropertyCustomizations.h"
#include "DetailsCustomizations/ModelingToolsBrushSizeCustomization.h"
#include "DetailsCustomizations/MeshVertexSculptToolCustomizations.h"
#include "DetailsCustomizations/BakeMeshAttributeToolCustomizations.h"
#include "DetailsCustomizations/BakeTransformToolCustomizations.h"
#include "DetailsCustomizations/PolygonSelectionMechanicCustomization.h"

#include "PropertySets/AxisFilterPropertyType.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "MeshVertexSculptTool.h"
#include "BakeMeshAttributeMapsTool.h"
#include "BakeMultiMeshAttributeMapsTool.h"
#include "BakeMeshAttributeVertexTool.h"
#include "BakeTransformTool.h"
#include "DynamicMeshActor.h"

#include "LevelEditor.h"
#include "Filters/CustomClassFilterData.h"


#define LOCTEXT_NAMESPACE "FModelingToolsEditorModeModule"

void FModelingToolsEditorModeModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FModelingToolsEditorModeModule::OnPostEngineInit);
}

void FModelingToolsEditorModeModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	FModelingToolActionCommands::UnregisterAllToolActions();
	FModelingToolsManagerCommands::Unregister();
	FModelingModeActionCommands::Unregister();

	// Unregister customizations
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (FName ClassName : ClassesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassName);
		}
		for (FName PropertyName : PropertiesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(PropertyName);
		}
	}

	// Unregister slate style overrides
	FModelingToolsEditorModeStyle::Shutdown();
}


void FModelingToolsEditorModeModule::OnPostEngineInit()
{
	// Register slate style overrides
	FModelingToolsEditorModeStyle::Initialize();

	FModelingToolActionCommands::RegisterAllToolActions();
	FModelingToolsManagerCommands::Register();
	FModelingModeActionCommands::Register();

	// same as ClassesToUnregisterOnShutdown but for properties, there is none right now
	PropertiesToUnregisterOnShutdown.Reset();
	ClassesToUnregisterOnShutdown.Reset();


	// Register details view customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	/// Sculpt
	PropertyModule.RegisterCustomPropertyTypeLayout("ModelingToolsAxisFilter", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FModelingToolsAxisFilterCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FModelingToolsAxisFilter::StaticStruct()->GetFName());
	PropertyModule.RegisterCustomPropertyTypeLayout("BrushToolRadius", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FModelingToolsBrushSizeCustomization::MakeInstance));
	PropertiesToUnregisterOnShutdown.Add(FBrushToolRadius::StaticStruct()->GetFName());
	PropertyModule.RegisterCustomClassLayout("SculptBrushProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FSculptBrushPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(USculptBrushProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("VertexBrushSculptProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FVertexBrushSculptPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UVertexBrushSculptProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("VertexBrushAlphaProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FVertexBrushAlphaPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UVertexBrushAlphaProperties::StaticClass()->GetFName());
	/// Bake
	PropertyModule.RegisterCustomClassLayout("BakeMeshAttributeMapsToolProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FBakeMeshAttributeMapsToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UBakeMeshAttributeMapsToolProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("BakeMultiMeshAttributeMapsToolProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FBakeMultiMeshAttributeMapsToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UBakeMultiMeshAttributeMapsToolProperties::StaticClass()->GetFName());
	PropertyModule.RegisterCustomClassLayout("BakeMeshAttributeVertexToolProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FBakeMeshAttributeVertexToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UBakeMeshAttributeVertexToolProperties::StaticClass()->GetFName());
	// PolyEd
	PropertyModule.RegisterCustomClassLayout("PolygonSelectionMechanicProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FPolygonSelectionMechanicPropertiesDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UPolygonSelectionMechanicProperties::StaticClass()->GetFName());

	PropertyModule.RegisterCustomClassLayout("BakeTransformToolProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FBakeTransformToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UBakeTransformToolProperties::StaticClass()->GetFName());

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// Register Level Editor Outliner filter
	if(TSharedPtr<FFilterCategory> GeometryFilterCategory = LevelEditorModule.GetOutlinerFilterCategory(FLevelEditorOutlinerBuiltInCategories::Geometry()))
	{
		TSharedRef<FCustomClassFilterData> DynamicMeshActorClassData = MakeShared<FCustomClassFilterData>(ADynamicMeshActor::StaticClass(), GeometryFilterCategory, FLinearColor::White);
		LevelEditorModule.AddCustomClassFilterToOutliner(DynamicMeshActorClassData);
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModelingToolsEditorModeModule, ModelingToolsEditorMode)