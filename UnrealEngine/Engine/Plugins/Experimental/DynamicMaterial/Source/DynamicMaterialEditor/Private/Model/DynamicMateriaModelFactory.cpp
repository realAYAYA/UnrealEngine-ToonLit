// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialComponent.h"
#include "Model/DynamicMaterialModelFactory.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerModelFactory"

const FString UDynamicMaterialModelFactory::BaseDirectory = FString("/Game/DynamicMaterials");
const FString UDynamicMaterialModelFactory::BaseName = FString("M_DynMat_");

UDynamicMaterialModelFactory::UDynamicMaterialModelFactory()
{
	SupportedClass = UDynamicMaterialModelEditorOnlyData::StaticClass();

	bCreateNew = false;
	bEditAfterNew = true;
	bEditorImport = false;
	bText = false;
}

UObject* UDynamicMaterialModelFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, 
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UDynamicMaterialModel::StaticClass()));

	UDynamicMaterialModel* NewModel = NewObject<UDynamicMaterialModel>(InParent, Class, Name, Flags | RF_Transactional);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = NewObject<UDynamicMaterialModelEditorOnlyData>(
		NewModel,
		UDynamicMaterialModelEditorOnlyData::StaticClass(),
		"EditorOnlyDataSI",
		RF_Transactional
	);

	NewModel->EditorOnlyDataSI = TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>(ModelEditorOnlyData);
	ModelEditorOnlyData->MaterialModel = NewModel;

	const FDMInitializationGuard InitGuard;
	ModelEditorOnlyData->AddSlot();

	return NewModel;
}

FText UDynamicMaterialModelFactory::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("MaterialDesignerModel", "Material Designer Model");
	return DisplayName;
}

FText UDynamicMaterialModelFactory::GetToolTip() const
{
	static const FText Tooltip = LOCTEXT("MaterialDesignerModelTooltip", "The Material Designer is a more intuitive way to create materials for people coming from other software.");
	return Tooltip;
}

#undef LOCTEXT_NAMESPACE
