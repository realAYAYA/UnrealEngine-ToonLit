// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMBlueprintFunctionLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageFunction.h"
#include "Components/DMMaterialStageGradient.h"
#include "Components/DMMaterialStageSource.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSITextureUV.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "Components/PrimitiveComponent.h"
#include "DMEDefs.h"
#include "DMObjectMaterialProperty.h"
#include "DMPrivate.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorModule.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

UDMMaterialStageInputValue* UDMBlueprintFunctionLibrary::FindDefaultStageOpacityInputValue(UDMMaterialStage* InStage)
{
	if (!IsValid(InStage))
	{
		return nullptr;
	}

	int32 OpacityInputIndex = -1;

	if (UDMMaterialStageThroughput* const ThroughputSource = Cast<UDMMaterialStageThroughput>(InStage->GetSource()))
	{
		const TArray<FDMMaterialStageConnector>& Connectors = ThroughputSource->GetInputConnectors();
		for (const FDMMaterialStageConnector& Connector : Connectors)
		{
			if (Connector.Name.ToString() == "Opacity")
			{
				OpacityInputIndex = Connector.Index;
			}
		}
	}

	const TArray<UDMMaterialStageInput*>& StageInputs = InStage->GetInputs();
	const TArray<FDMMaterialStageConnection>& InputConnectionMap = InStage->GetInputConnectionMap();

	if (InputConnectionMap.IsValidIndex(OpacityInputIndex) && InputConnectionMap[OpacityInputIndex].Channels.Num() > 0)
	{
		if (InputConnectionMap[OpacityInputIndex].Channels[0].SourceIndex >= FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT)
		{
			const int32 StageInputIndex = InputConnectionMap[OpacityInputIndex].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

			return Cast<UDMMaterialStageInputValue>(StageInputs[StageInputIndex]);
		}
	}

	return nullptr;
}

void UDMBlueprintFunctionLibrary::SetDefaultStageSourceTexture(UDMMaterialStage* InStage, UTexture* InTexture)
{
	if (!IsValid(InStage) || !IsValid(InTexture))
	{
		return;
	}

	UDMMaterialStageSource* StageSource = InStage->GetSource();
	if (!StageSource)
	{
		return;
	}

	auto SetTextureValue = [InTexture](UDMMaterialStageInputValue* NewInputValue)
	{
		if (UDMMaterialValueTexture* TextureValue = Cast<UDMMaterialValueTexture>(NewInputValue->GetValue()))
		{
			TextureValue->SetValue(InTexture);
		}
	};

	if (GUndo)
	{
		InStage->Modify();
	}

	if (UDMMaterialStageBlend* const Blend = Cast<UDMMaterialStageBlend>(StageSource))
	{
		UDMMaterialStageInputExpression* NewInput = UDMMaterialStageInputExpression::ChangeStageInput_Expression(
			InStage,
			UDMMaterialStageExpressionTextureSample::StaticClass(), 
			UDMMaterialStageBlend::InputB,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			0,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		UDMMaterialSubStage* SubStage = NewInput->GetSubStage();
		check(SubStage);

		UDMMaterialStageInputValue* NewInputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			SubStage,
			0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			EDMValueType::VT_Texture,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		SetTextureValue(NewInputValue);
		return;
	}

	if (UDMMaterialStageThroughputLayerBlend* const LayerBlend = Cast<UDMMaterialStageThroughputLayerBlend>(StageSource))
	{
		const bool bHasAlpha = UE::DynamicMaterial::Private::HasAlpha(InTexture);

		UDMMaterialStageInputExpression* NewInput = UDMMaterialStageInputExpression::ChangeStageInput_Expression(
			InStage,
			UDMMaterialStageExpressionTextureSample::StaticClass(), 
			2, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
			bHasAlpha ? 1 : 0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		UDMMaterialSubStage* SubStage = NewInput->GetSubStage();
		check(IsValid(SubStage));

		UDMMaterialStageInputValue* NewInputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			SubStage,
			0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			EDMValueType::VT_Texture,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		SetTextureValue(NewInputValue);
		return;
	}

	UDMMaterialStageExpression* NewExpression = InStage->ChangeSource<UDMMaterialStageExpressionTextureSample>();

	UDMMaterialStageInputValue* NewInputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
		InStage,
		0, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
		EDMValueType::VT_Texture,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);

	SetTextureValue(NewInputValue);
}

TArray<FDMObjectMaterialProperty> UDMBlueprintFunctionLibrary::GetActorMaterialProperties(AActor* InActor)
{
	TArray<FDMObjectMaterialProperty> ActorProperties;

	if (!IsValid(InActor))
	{
		return ActorProperties;
	}

	FDMGetObjectMaterialPropertiesDelegate PropertyGenerator = FDynamicMaterialEditorModule::GetCustomMaterialPropertyGenerator(InActor->GetClass());

	if (PropertyGenerator.IsBound())
	{
		ActorProperties = PropertyGenerator.Execute(InActor);

		if (!ActorProperties.IsEmpty())
		{
			return ActorProperties;
		}
	}

	InActor->ForEachComponent<UPrimitiveComponent>(false, [&ActorProperties](UPrimitiveComponent* InComp)
		{
			for (int32 MaterialIdx = 0; MaterialIdx < InComp->GetNumMaterials(); ++MaterialIdx)
			{
				ActorProperties.Add({InComp, MaterialIdx});
			}
		});

	return ActorProperties;
}

UDynamicMaterialModel* UDMBlueprintFunctionLibrary::CreateDynamicMaterialInObject(FDMObjectMaterialProperty& InMaterialProperty)
{
	if (!InMaterialProperty.IsValid())
	{
		return nullptr;
	}

	UObject* const Outer = InMaterialProperty.OuterWeak.Get();

	UDynamicMaterialInstanceFactory* const InstanceFactory = NewObject<UDynamicMaterialInstanceFactory>();
	check(InstanceFactory);

	UDynamicMaterialInstance* const NewInstance = Cast<UDynamicMaterialInstance>(InstanceFactory->FactoryCreateNew(UDynamicMaterialInstance::StaticClass(),
		Outer, NAME_None, RF_Transactional, nullptr, GWarn));
	check(NewInstance);

	bool bSubsystemTakenOver = false;

	if (const UWorld* const World = Outer->GetWorld())
	{
		if (IsValid(World))
		{
			UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>();

			if (IsValid(WorldSubsystem))
			{
				if (WorldSubsystem->GetMaterialValueSetterDelegate().IsBound())
				{
					bSubsystemTakenOver = WorldSubsystem->GetMaterialValueSetterDelegate().Execute(InMaterialProperty, NewInstance);
				}
			}
		}
	}

	if (!bSubsystemTakenOver)
	{
		InMaterialProperty.SetMaterial(NewInstance);
	}

	return NewInstance->GetMaterialModel();
}

bool UDMBlueprintFunctionLibrary::ExportMaterialInstance(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath)
{
	if (!IsValid(InMaterialModel))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material to export."));
		return false;
	}

	if (InSavePath.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material save path to export."));
		return false;
	}

	UDynamicMaterialInstance* Instance = InMaterialModel->GetDynamicMaterialInstance();
	if (!Instance)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to find a Material Designer Instance to export."));
		return false;
	}

	const FString PackagePath = FPaths::GetBaseFilename(*InSavePath, false);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE::DynamicMaterialEditor::Private::LogError(FString::Printf(TEXT("Failed to create package for Material Designer Instance (%s)."), *PackagePath));
		return false;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(Instance, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, EDuplicateMode::Normal, EInternalObjectFlags::None);

	UObject* NewAsset = StaticDuplicateObjectEx(Params);
	if (!NewAsset)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new Material Designer Instance asset."));
		return false;
	}

	// Not sure why these need to be set again!
	NewAsset->SetFlags(RF_Public | RF_Standalone);

	if (UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(NewAsset))
	{
		if (UDynamicMaterialModel* NewModel = NewInstance->GetMaterialModel())
		{
			if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = NewModel->GetEditorOnlyData())
			{
				ModelEditorOnlyData->RequestMaterialBuild();
			}
		}
	}

	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->FullyLoad();

	return true;
}

bool UDMBlueprintFunctionLibrary::ExportGeneratedMaterial(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath)
{
	if (!IsValid(InMaterialModel))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material to export."));
		return false;
	}

	if (InSavePath.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material save path to export."));
		return false;
	}

	UMaterial* GeneratedMaterial = InMaterialModel->GetGeneratedMaterial();
	if (!GeneratedMaterial)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to find a generated material to export."));
		return false;
	}

	const FString PackagePath = FPaths::GetBaseFilename(*InSavePath, false);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE::DynamicMaterialEditor::Private::LogError(FString::Printf(TEXT("Failed to create package for exported material (%s)."), *PackagePath));
		return false;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(GeneratedMaterial, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, EDuplicateMode::Normal, EInternalObjectFlags::None);

	UObject* NewAsset = StaticDuplicateObjectEx(Params);
	if (!NewAsset)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new material asset."));
		return false;
	}

	// Not sure why these need to be set again!
	NewAsset->SetFlags(RF_Public | RF_Standalone);

	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->FullyLoad();

	return true;
}
