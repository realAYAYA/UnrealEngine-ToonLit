// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/DynamicMaterialInstance.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModel.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#endif

UDynamicMaterialInstance::UDynamicMaterialInstance()
{
	BaseMaterial = nullptr;
	MaterialModel = nullptr;

	bOutputTranslucentVelocity = true;
}

#if WITH_EDITOR
void UDynamicMaterialInstance::InitializeMIDPublic()
{
	check(MaterialModel);

	BaseMaterial = MaterialModel->GetGeneratedMaterial();
	SetParentInternal(BaseMaterial, false);
	ClearParameterValues();
	UpdateCachedData();
}

void UDynamicMaterialInstance::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (MaterialModel)
	{
		MaterialModel->SetDynamicMaterialInstance(this);

		if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = MaterialModel->GetEditorOnlyData())
		{
			ModelEditorOnlyData->RequestMaterialBuild();
		}
	}
}

void UDynamicMaterialInstance::PostEditImport()
{
	Super::PostEditImport();

	if (MaterialModel)
	{
		MaterialModel->SetDynamicMaterialInstance(this);

		if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = MaterialModel->GetEditorOnlyData())
		{
			ModelEditorOnlyData->RequestMaterialBuild();
		}
	}
}

void UDynamicMaterialInstance::OnMaterialBuilt(UDynamicMaterialModel* InMaterialModel)
{
	if (MaterialModel != InMaterialModel)
	{
		return;
	}

	InitializeMIDPublic();
}
#endif