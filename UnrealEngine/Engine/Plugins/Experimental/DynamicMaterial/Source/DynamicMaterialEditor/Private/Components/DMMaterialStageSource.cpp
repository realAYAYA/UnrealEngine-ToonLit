// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialStageSource.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "MaterialValueType.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

TArray<TStrongObjectPtr<UClass>> UDMMaterialStageSource::SourceClasses = TArray<TStrongObjectPtr<UClass>>();

UDMMaterialStageSource::UDMMaterialStageSource()
{
	PreviewMaterial = nullptr;
}

UDMMaterialStage* UDMMaterialStageSource::GetStage() const
{
	return Cast<UDMMaterialStage>(GetOuterSafe());
}

void UDMMaterialStageSource::Update(EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (HasComponentBeenRemoved())
	{
		return;
	}

	if (InUpdateType == EDMUpdateType::Structure)
	{
		MarkComponentDirty();
	}

	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	Stage->Update(InUpdateType);

	Super::Update(InUpdateType);
}

void UDMMaterialStageSource::OnComponentAdded()
{
	if (!IsComponentValid())
	{
		return;
	}

	Super::OnComponentAdded();

	if (FDMUpdateGuard::CanUpdate())
	{
		Update(EDMUpdateType::Structure);
	}
}

void UDMMaterialStageSource::GetMaskAlphaBlendNode(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterialExpression*& OutExpression, int32& OutOutputIndex, int32& OutOutputChannel) const
{
	OutExpression = nullptr;
}

const TArray<TStrongObjectPtr<UClass>>& UDMMaterialStageSource::GetAvailableSourceClasses()
{
	if (SourceClasses.IsEmpty())
	{
		GenerateClassList();
	}

	return SourceClasses;
}

void UDMMaterialStageSource::GenerateClassList()
{
	SourceClasses.Empty();

	for (TObjectIterator<UClass> ClassIT; ClassIT; ++ClassIT)
	{
		TSubclassOf<UDMMaterialStageSource> MSEClass = *ClassIT;

		if (!MSEClass.Get())
		{
			continue;
		}

		if (MSEClass->ClassFlags & (CLASS_Abstract | CLASS_Hidden | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		SourceClasses.Add(TStrongObjectPtr<UClass>(MSEClass));
	}
}

UMaterial* UDMMaterialStageSource::GetPreviewMaterial()
{
	if (!PreviewMaterial)
	{
		CreatePreviewMaterial();

		if (PreviewMaterial)
		{
			MarkComponentDirty();
		}
	}

	return PreviewMaterial;
}

void UDMMaterialStageSource::UpdateOntoPreviewMaterial(UMaterial* ExternalPreviewMaterial)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (!ExternalPreviewMaterial)
	{
		return;
	}

	UpdatePreviewMaterial(ExternalPreviewMaterial);
}

void UDMMaterialStageSource::CreatePreviewMaterial()
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FDynamicMaterialModule::IsMaterialExportEnabled() == false)
	{
		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		check(MaterialFactory);

		PreviewMaterial = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(
			UMaterial::StaticClass(),
			GetTransientPackage(),
			NAME_None,
			RF_Transient,
			nullptr,
			GWarn
		));

		PreviewMaterial->bIsPreviewMaterial = true;
	}
	else
	{
		FString MaterialBaseName = GetName() + "-" + FGuid::NewGuid().ToString();
		const FString FullName = "/Game/DynamicMaterials/" + MaterialBaseName;
		UPackage* Package = CreatePackage(*FullName);

		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		check(MaterialFactory);

		PreviewMaterial = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(
			UMaterial::StaticClass(),
			Package,
			*MaterialBaseName,
			RF_Standalone | RF_Public,
			nullptr,
			GWarn
		));

		FAssetRegistryModule::AssetCreated(PreviewMaterial);
		Package->FullyLoad();
	}
}

void UDMMaterialStageSource::UpdatePreviewMaterial(UMaterial* InPreviewMaterial)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (!InPreviewMaterial)
	{
		if (!PreviewMaterial)
		{
			CreatePreviewMaterial();
		}

		InPreviewMaterial = PreviewMaterial;

		if (!PreviewMaterial)
		{
			return;
		}
	}

	UE_LOG(LogDynamicMaterialEditor, Display, TEXT("Building Material Designer Source Preview (%s)..."), *GetName());

	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	UDMMaterialLayerObject* Layer = Stage->GetLayer();
	check(Layer);

	UDMMaterialSlot* Slot = Layer->GetSlot();
	check(Slot);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	check(ModelEditorOnlyData);

	TSharedRef<FDMMaterialBuildState> BuildState = ModelEditorOnlyData->CreateBuildState(InPreviewMaterial);
	BuildState->SetPreviewMaterial();

	GenerateExpressions(BuildState);
	UMaterialExpression* StageSourceExpression = BuildState->GetLastStageSourceExpression(this);

	BuildState->GetBuildUtils().UpdatePreviewMaterial(StageSourceExpression, 0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 32);
}

int32 UDMMaterialStageSource::GetInnateMaskOutput(int32 OutputIndex, int32 OutputChannels) const
{
	return INDEX_NONE;
}

bool UDMMaterialStageSource::UpdateStagePreviewMaterial(UDMMaterialStage* InStage, UMaterial* InPreviewMaterial, 
	UMaterialExpression*& OutMaterialExpression, int32& OutputIndex)
{
	check(InStage);
	check(InPreviewMaterial);

	UDMMaterialLayerObject* Layer = InStage->GetLayer();
	check(Layer);

	UDMMaterialSlot* Slot = Layer->GetSlot();
	check(Slot);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	check(ModelEditorOnlyData);

	UDMMaterialProperty* PropertyObj = ModelEditorOnlyData->GetMaterialProperty(EDMMaterialPropertyType::EmissiveColor);
	check(PropertyObj);

	TSharedRef<FDMMaterialBuildState> BuildState = ModelEditorOnlyData->CreateBuildState(InPreviewMaterial);
	BuildState->SetPreviewMaterial();

	UDMMaterialStageSource* PreviewSource = InStage->GetSource();

	if (!PreviewSource)
	{
		return false;
	}

	PreviewSource->GenerateExpressions(BuildState);
	const TArray<UMaterialExpression*>& SourceExpressions = BuildState->GetStageSourceExpressions(PreviewSource);

	if (SourceExpressions.IsEmpty())
	{
		return false;
	}

	UMaterialExpression* LastExpression = SourceExpressions.Last();

	bool bIsMaskStage = false;

	if (Layer->GetStageType(InStage) == EDMMaterialLayerStage::Mask)
	{
		bIsMaskStage = true;
	}

	int32 BestMatch = INDEX_NONE;
	int32 OutputCount = 0;
	const int32 FloatsForPropertyType = bIsMaskStage
		? 1
		: UDMValueDefinitionLibrary::GetValueDefinition(PropertyObj->GetInputConnectorType()).GetFloatCount();

	for (int32 OutputIdx = 0; OutputIdx < LastExpression->GetOutputs().Num(); ++OutputIdx)
	{
		EMaterialValueType CurrentOutputType = static_cast<EMaterialValueType>(LastExpression->GetOutputType(OutputIdx));
		int32 CurrentOutputCount = 0;

		switch (CurrentOutputType)
		{
			case MCT_Float:
			case MCT_Float1:
				CurrentOutputCount = 1;
				break;

			case MCT_Float2:
				CurrentOutputCount = 2;
				break;

			case MCT_Float3:
				CurrentOutputCount = 3;
				break;

			case MCT_Float4:
				CurrentOutputCount = 4;
				break;

			default:
				continue; // For loop
		}

		if (CurrentOutputCount > OutputCount)
		{
			BestMatch = OutputIdx;
			OutputCount = CurrentOutputCount;

			if (CurrentOutputCount >= FloatsForPropertyType)
			{
				break;
			}
		}
	}

	if (BestMatch != INDEX_NONE)
	{
		OutMaterialExpression = LastExpression;
		OutputIndex = BestMatch;
		return true;
	}

	return false;
}

void UDMMaterialStageSource::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, class FEditPropertyChain* InPropertyThatChanged)
{
	if (!IsComponentValid())
	{
		return;
	}

	Update(EDMUpdateType::Structure);
}

void UDMMaterialStageSource::PostEditUndo()
{
	Super::PostEditUndo();

	if (!IsComponentValid())
	{
		return;
	}

	MarkComponentDirty();
	Update(EDMUpdateType::Structure);
}

void UDMMaterialStageSource::DoClean()
{
	if (IsComponentValid())
	{
		// Stage Source preview images are currently disabled.
		//UpdatePreviewMaterial();
	}

	Super::DoClean();
}

UDMMaterialComponent* UDMMaterialStageSource::GetParentComponent() const
{
	return GetStage();
}

void UDMMaterialStageSource::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	PreviewMaterial = nullptr;
}
