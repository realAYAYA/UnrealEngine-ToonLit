// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialPropertyHelpers.h"
#include "Misc/MessageDialog.h"
#include "Misc/Guid.h"
#include "UObject/UnrealType.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SToolTip.h"
#include "Styling/AppStyle.h"
#include "Materials/MaterialInterface.h"
#include "MaterialEditor/DEditorFontParameterValue.h"
#include "MaterialEditor/DEditorMaterialLayersParameterValue.h"
#include "MaterialEditor/DEditorRuntimeVirtualTextureParameterValue.h"
#include "MaterialEditor/DEditorScalarParameterValue.h"
#include "MaterialEditor/DEditorStaticComponentMaskParameterValue.h"
#include "MaterialEditor/DEditorStaticSwitchParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/DEditorVectorParameterValue.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionChannelMaskParameter.h"
#include "EditorSupportDelegates.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "StaticParameterSet.h"
#include "MaterialEditor/MaterialEditorPreviewParameters.h"
#include "Factories/MaterialFunctionInstanceFactory.h"
#include "SMaterialLayersFunctionsTree.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Factories/MaterialFunctionMaterialLayerFactory.h"
#include "Factories/MaterialFunctionMaterialLayerBlendFactory.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Curves/CurveLinearColor.h"

#define LOCTEXT_NAMESPACE "MaterialPropertyHelper"

FText FMaterialPropertyHelpers::LayerID = LOCTEXT("LayerID", "Layer Asset");
FText FMaterialPropertyHelpers::BlendID = LOCTEXT("BlendID", "Blend Asset");
FName FMaterialPropertyHelpers::LayerParamName = FName("Global Material Layers Parameter Values");


void SLayerHandle::Construct(const FArguments& InArgs)
{
	OwningStack = InArgs._OwningStack;

	ChildSlot
		[
			InArgs._Content.Widget
		];
}

FReply SLayerHandle::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		{
			TSharedPtr<FDragDropOperation> DragDropOp = CreateDragDropOperation(OwningStack.Pin());
			if (DragDropOp.IsValid())
			{
				OwningStack.Pin()->OnLayerDragDetected();
				return FReply::Handled().BeginDragDrop(DragDropOp.ToSharedRef());
			}
		}
	}

	return FReply::Unhandled();

}

TSharedPtr<FLayerDragDropOp> SLayerHandle::CreateDragDropOperation(TSharedPtr<SMaterialLayersFunctionsInstanceTreeItem> InOwningStack)
{
	TSharedPtr<FLayerDragDropOp> Operation = MakeShareable(new FLayerDragDropOp(InOwningStack));

	return Operation;
}

EVisibility FMaterialPropertyHelpers::ShouldShowExpression(UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance, FGetShowHiddenParameters ShowHiddenDelegate)
{
	bool bShowHidden = true;

	ShowHiddenDelegate.ExecuteIfBound(bShowHidden);

	bool bIsCooked = false;
	if (MaterialEditorInstance->SourceInstance)
	{
		if (UMaterial* Material = MaterialEditorInstance->SourceInstance->GetMaterial())
		{
			bIsCooked = Material->GetPackage()->bIsCookedForEditor;
		}
	}

	const bool bShouldShowExpression = bShowHidden || MaterialEditorInstance->VisibleExpressions.Contains(Parameter->ParameterInfo) || bIsCooked;

	if (MaterialEditorInstance->bShowOnlyOverrides)
	{
		return (IsOverriddenExpression(Parameter) && bShouldShowExpression) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return bShouldShowExpression ? EVisibility::Visible: EVisibility::Collapsed;
}

void FMaterialPropertyHelpers::OnMaterialLayerAssetChanged(const struct FAssetData& InAssetData, int32 Index, EMaterialParameterAssociation MaterialType, TSharedPtr<class IPropertyHandle> InHandle, FMaterialLayersFunctions* InMaterialFunction)
{
	const FScopedTransaction Transaction(LOCTEXT("SetLayerorBlendAsset", "Set Layer or Blend Asset"));
	InHandle->NotifyPreChange();
	const FName FilterTag = FName(TEXT("MaterialFunctionUsage"));
	if (InAssetData.TagsAndValues.Contains(FilterTag) || InAssetData.AssetName == NAME_None)
	{
		UMaterialFunctionInterface* LayerFunction = Cast<UMaterialFunctionInterface>(InAssetData.GetAsset());
		switch (MaterialType)
		{
		case EMaterialParameterAssociation::LayerParameter:
			if (InMaterialFunction->Layers[Index] != LayerFunction)
			{
				InMaterialFunction->Layers[Index] = LayerFunction;
				InMaterialFunction->UnlinkLayerFromParent(Index);
			}
			break;
		case EMaterialParameterAssociation::BlendParameter:
			if (InMaterialFunction->Blends[Index] != LayerFunction)
			{
				InMaterialFunction->Blends[Index] = LayerFunction;
				InMaterialFunction->UnlinkLayerFromParent(Index + 1); // Blend indices are offset by 1, no blend for base layer
			}
			break;
		default:
			break;
		}
	}
	InHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

bool FMaterialPropertyHelpers::FilterLayerAssets(const struct FAssetData& InAssetData, FMaterialLayersFunctions* LayerFunction, EMaterialParameterAssociation MaterialType, int32 Index)
{
	bool ShouldAssetBeFilteredOut = false;
	const FName FilterTag = FName(TEXT("MaterialFunctionUsage"));
	const FName BaseTag = FName(TEXT("Base"));
	FAssetDataTagMapSharedView::FFindTagResult MaterialFunctionUsage = InAssetData.TagsAndValues.FindTag(FilterTag);

	FName BaseClassName;
	FName InstanceClassName;

	FString CompareString;
	if (MaterialFunctionUsage.IsSet())
	{
		FAssetDataTagMapSharedView::FFindTagResult Base = InAssetData.TagsAndValues.FindTag(BaseTag);

		FString BaseString;
		FString DiscardString;
		FString CleanString;
		if (Base.IsSet())
		{
			BaseString = Base.GetValue();
			BaseString.Split(".", &DiscardString, &CleanString);
			CleanString.Split("'", &CleanString, &DiscardString);
		}
		else
		{
			CleanString = InAssetData.AssetName.ToString();
		}

		FString LeftPath;
		FString RightPath;
		bool bShouldFilter = false;
		switch (MaterialType)
		{
		case EMaterialParameterAssociation::LayerParameter:
		{
			CompareString = "MaterialLayer";
			if (LayerFunction->Layers[Index] != nullptr)
			{
				RightPath = LayerFunction->Layers[Index]->GetBaseFunction()->GetFName().ToString();
				if (RightPath.IsEmpty())
				{
					RightPath = LayerFunction->Layers[Index]->GetFName().ToString();
				}
			}
			bShouldFilter = LayerFunction->EditorOnly.RestrictToLayerRelatives[Index];
			BaseClassName = UMaterialFunctionMaterialLayer::StaticClass()->GetFName();
			InstanceClassName = UMaterialFunctionMaterialLayerInstance::StaticClass()->GetFName();
		}
		break;
		case EMaterialParameterAssociation::BlendParameter:
		{
			CompareString = "MaterialLayerBlend";
			if (LayerFunction->Blends[Index] != nullptr)
			{
				RightPath = LayerFunction->Blends[Index]->GetBaseFunction()->GetFName().ToString();
				if (RightPath.IsEmpty())
				{
					RightPath = LayerFunction->Blends[Index]->GetFName().ToString();
				}
			}
			bShouldFilter = LayerFunction->EditorOnly.RestrictToBlendRelatives[Index];
			BaseClassName = UMaterialFunctionMaterialLayerBlend::StaticClass()->GetFName();
			InstanceClassName = UMaterialFunctionMaterialLayerBlendInstance::StaticClass()->GetFName();
		}
		break;
		default:
		break;
		}

		if (MaterialFunctionUsage.GetValue() != CompareString)
		{

			ShouldAssetBeFilteredOut = true;
		}
		else
		{
			bool bSameBase = CleanString == RightPath;
			if (!RightPath.IsEmpty() && !bSameBase && bShouldFilter)
			{
				ShouldAssetBeFilteredOut = true;
			}
		}
	}
	else
	{
		ShouldAssetBeFilteredOut = true;
	}
	return ShouldAssetBeFilteredOut;
}

FReply FMaterialPropertyHelpers::OnClickedSaveNewMaterialInstance(UMaterialInterface* Parent, UObject* EditorObject)
{
	const FString DefaultSuffix = TEXT("_Inst");
	TArray<FEditorParameterGroup> ParameterGroups;
	UMaterialEditorInstanceConstant* MaterialInstanceEditor = Cast<UMaterialEditorInstanceConstant>(EditorObject);
	if (MaterialInstanceEditor)
	{
		ParameterGroups = MaterialInstanceEditor->ParameterGroups;
	}
	UMaterialEditorPreviewParameters* MaterialEditor = Cast<UMaterialEditorPreviewParameters>(EditorObject);
	if (MaterialEditor)
	{
		ParameterGroups = MaterialEditor->ParameterGroups;
	}
	if (!MaterialEditor && !MaterialInstanceEditor)
	{
		return FReply::Unhandled();
	}

	if (Parent)
	{
		// Create an appropriate and unique name 
		FString Name;
		FString PackageName;
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(Parent->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = Parent;

		UObject* Child = AssetToolsModule.Get().CreateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialInstanceConstant::StaticClass(), Factory);
		UMaterialInstanceConstant* ChildInstance = Cast<UMaterialInstanceConstant>(Child);
		CopyMaterialToInstance(ChildInstance, ParameterGroups);

	}
	return FReply::Handled();
}

void FMaterialPropertyHelpers::CopyMaterialToInstance(UMaterialInstanceConstant* ChildInstance, TArray<FEditorParameterGroup> &ParameterGroups)
{
	if (ChildInstance)
	{
		if (ChildInstance->IsTemplate(RF_ClassDefaultObject) == false)
		{
			ChildInstance->MarkPackageDirty();
			ChildInstance->ClearParameterValuesEditorOnly();

			FMaterialInstanceParameterUpdateContext UpdateContext(ChildInstance);

			//propagate changes to the base material so the instance will be updated if it has a static permutation resource
			for (int32 GroupIdx = 0; GroupIdx < ParameterGroups.Num(); GroupIdx++)
			{
				FEditorParameterGroup& Group = ParameterGroups[GroupIdx];
				for (int32 ParameterIdx = 0; ParameterIdx < Group.Parameters.Num(); ParameterIdx++)
				{
					UDEditorParameterValue* Parameter = Group.Parameters[ParameterIdx];
					if (Parameter && Parameter->bOverride)
					{
						FMaterialParameterMetadata ParameterResult;
						if (Parameter->GetValue(ParameterResult))
						{
							UpdateContext.SetParameterValueEditorOnly(Parameter->ParameterInfo, ParameterResult);
						}
						// This is called to initialize newly created child MIs
						// Don't need to copy layers from parent, since they will not initially be changed from parent values
					}
				}
			}
		}
	}
}


void FMaterialPropertyHelpers::TransitionAndCopyParameters(UMaterialInstanceConstant* ChildInstance, TArray<FEditorParameterGroup> &ParameterGroups, bool bForceCopy)
{
	if (ChildInstance)
	{
		if (ChildInstance->IsTemplate(RF_ClassDefaultObject) == false)
		{
			ChildInstance->MarkPackageDirty();
			ChildInstance->ClearParameterValuesEditorOnly();
			//propagate changes to the base material so the instance will be updated if it has a static permutation resource
			//FStaticParameterSet NewStaticParameters;
			FMaterialInstanceParameterUpdateContext UpdateContext(ChildInstance);
			for (int32 GroupIdx = 0; GroupIdx < ParameterGroups.Num(); GroupIdx++)
			{
				FEditorParameterGroup& Group = ParameterGroups[GroupIdx];
				for (int32 ParameterIdx = 0; ParameterIdx < Group.Parameters.Num(); ParameterIdx++)
				{
					UDEditorParameterValue* Parameter = Group.Parameters[ParameterIdx];
					if (Parameter && (Parameter->bOverride || bForceCopy))
					{
						FMaterialParameterMetadata EditorValue;
						if (Parameter->GetValue(EditorValue))
						{
							FMaterialParameterInfo TransitionedScalarInfo = FMaterialParameterInfo();
							TransitionedScalarInfo.Name = Parameter->ParameterInfo.Name;
							UpdateContext.SetParameterValueEditorOnly(TransitionedScalarInfo, EditorValue);
						}
					}
				}
			}
		}
	}
}

FReply FMaterialPropertyHelpers::OnClickedSaveNewFunctionInstance(class UMaterialFunctionInterface* Object, class UMaterialInterface* PreviewMaterial, UObject* EditorObject)
{
	const FString DefaultSuffix = TEXT("_Inst");
	TArray<FEditorParameterGroup> ParameterGroups;
	UMaterialEditorInstanceConstant* MaterialInstanceEditor = Cast<UMaterialEditorInstanceConstant>(EditorObject);
	UMaterialInterface* FunctionPreviewMaterial = nullptr;
	if (MaterialInstanceEditor)
	{
		ParameterGroups = MaterialInstanceEditor->ParameterGroups;
		FunctionPreviewMaterial = MaterialInstanceEditor->SourceInstance;
	}
	UMaterialEditorPreviewParameters* MaterialEditor = Cast<UMaterialEditorPreviewParameters>(EditorObject);
	if (MaterialEditor)
	{
		ParameterGroups = MaterialEditor->ParameterGroups;
		FunctionPreviewMaterial = PreviewMaterial;
	}
	if (!MaterialEditor && !MaterialInstanceEditor)
	{
		return FReply::Unhandled();
	}

	if (Object)
	{
		UMaterial* EditedMaterial = Cast<UMaterial>(FunctionPreviewMaterial);
		if (EditedMaterial)
		{

			UMaterialInstanceConstant* ProxyMaterial = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transactional);
			ProxyMaterial->SetParentEditorOnly(EditedMaterial);
			ProxyMaterial->PreEditChange(NULL);
			ProxyMaterial->PostEditChange();
			CopyMaterialToInstance(ProxyMaterial, ParameterGroups);
			FunctionPreviewMaterial = ProxyMaterial;
		}
		// Create an appropriate and unique name 
		FString Name;
		FString PackageName;
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

		
		UObject* Child;
		if (Object->GetMaterialFunctionUsage() == EMaterialFunctionUsage::MaterialLayer)
		{
			UMaterialFunctionMaterialLayerInstanceFactory* LayerFactory = NewObject<UMaterialFunctionMaterialLayerInstanceFactory>();
			LayerFactory->InitialParent = Object;
			Child = AssetToolsModule.Get().CreateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerInstance::StaticClass(), LayerFactory);
		}
		else if (Object->GetMaterialFunctionUsage() == EMaterialFunctionUsage::MaterialLayerBlend)
		{
			UMaterialFunctionMaterialLayerBlendInstanceFactory* BlendFactory = NewObject<UMaterialFunctionMaterialLayerBlendInstanceFactory>();
			BlendFactory->InitialParent = Object;
			Child = AssetToolsModule.Get().CreateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerBlendInstance::StaticClass(), BlendFactory);
		}
		else
		{
			UMaterialFunctionInstanceFactory* Factory = NewObject<UMaterialFunctionInstanceFactory>();
			Factory->InitialParent = Object;
			Child = AssetToolsModule.Get().CreateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionInstance::StaticClass(), Factory);
		}

		UMaterialFunctionInstance* ChildInstance = Cast<UMaterialFunctionInstance>(Child);
		if (ChildInstance)
		{
			if (ChildInstance->IsTemplate(RF_ClassDefaultObject) == false)
			{
				ChildInstance->MarkPackageDirty();
				ChildInstance->SetParent(Object);
				UMaterialInstance* EditedInstance = Cast<UMaterialInstance>(FunctionPreviewMaterial);
				if (EditedInstance)
				{
					ChildInstance->ScalarParameterValues = EditedInstance->ScalarParameterValues;
					ChildInstance->VectorParameterValues = EditedInstance->VectorParameterValues;
					ChildInstance->DoubleVectorParameterValues = EditedInstance->DoubleVectorParameterValues;
					ChildInstance->TextureParameterValues = EditedInstance->TextureParameterValues;
					ChildInstance->RuntimeVirtualTextureParameterValues = EditedInstance->RuntimeVirtualTextureParameterValues;
					ChildInstance->SparseVolumeTextureParameterValues = EditedInstance->SparseVolumeTextureParameterValues;
					ChildInstance->FontParameterValues = EditedInstance->FontParameterValues;

					const FStaticParameterSet& StaticParameters = EditedInstance->GetStaticParameters();
					ChildInstance->StaticSwitchParameterValues = StaticParameters.StaticSwitchParameters;
					ChildInstance->StaticComponentMaskParameterValues = StaticParameters.EditorOnly.StaticComponentMaskParameters;
				}
			}
		}
	}
	return FReply::Handled();
}


FReply FMaterialPropertyHelpers::OnClickedSaveNewLayerInstance(class UMaterialFunctionInterface* Object, TSharedPtr<FSortedParamData> InSortedData)
{
	const FString DefaultSuffix = TEXT("_Inst");
	TArray<FEditorParameterGroup> ParameterGroups;
	UMaterialInterface* FunctionPreviewMaterial = nullptr;
	if (Object)
	{
		FunctionPreviewMaterial = Object->GetPreviewMaterial();
	}
	for (TSharedPtr<FSortedParamData> Group : InSortedData->Children)
	{
		FEditorParameterGroup DuplicatedGroup = FEditorParameterGroup();
		DuplicatedGroup.GroupAssociation = Group->Group.GroupAssociation;
		DuplicatedGroup.GroupName = Group->Group.GroupName;
		DuplicatedGroup.GroupSortPriority = Group->Group.GroupSortPriority;
		for (UDEditorParameterValue* Parameter : Group->Group.Parameters)
		{
			if (Parameter->ParameterInfo.Index == InSortedData->ParameterInfo.Index)
			{
				DuplicatedGroup.Parameters.Add(Parameter);
			}
		}
		ParameterGroups.Add(DuplicatedGroup);
	}

	if (Object)
	{
		UMaterialInterface* EditedMaterial = FunctionPreviewMaterial;
		if (EditedMaterial)
		{
			UMaterialInstanceConstant* ProxyMaterial = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transactional);
			ProxyMaterial->SetParentEditorOnly(EditedMaterial);
			ProxyMaterial->PreEditChange(NULL);
			ProxyMaterial->PostEditChange();
			TransitionAndCopyParameters(ProxyMaterial, ParameterGroups);
			FunctionPreviewMaterial = ProxyMaterial;
		}
		// Create an appropriate and unique name 
		FString Name;
		FString PackageName;
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);


		UObject* Child;
		if (Object->GetMaterialFunctionUsage() == EMaterialFunctionUsage::MaterialLayer)
		{
			UMaterialFunctionMaterialLayerInstanceFactory* LayerFactory = NewObject<UMaterialFunctionMaterialLayerInstanceFactory>();
			LayerFactory->InitialParent = Object;
			Child = AssetToolsModule.Get().CreateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerInstance::StaticClass(), LayerFactory);
		}
		else if (Object->GetMaterialFunctionUsage() == EMaterialFunctionUsage::MaterialLayerBlend)
		{
			UMaterialFunctionMaterialLayerBlendInstanceFactory* BlendFactory = NewObject<UMaterialFunctionMaterialLayerBlendInstanceFactory>();
			BlendFactory->InitialParent = Object;
			Child = AssetToolsModule.Get().CreateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerBlendInstance::StaticClass(), BlendFactory);
		}
		else
		{
			UMaterialFunctionInstanceFactory* Factory = NewObject<UMaterialFunctionInstanceFactory>();
			Factory->InitialParent = Object;
			Child = AssetToolsModule.Get().CreateAssetWithDialog(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionInstance::StaticClass(), Factory);
		}

		UMaterialFunctionInstance* ChildInstance = Cast<UMaterialFunctionInstance>(Child);
		if (ChildInstance)
		{
			if (ChildInstance->IsTemplate(RF_ClassDefaultObject) == false)
			{
				ChildInstance->MarkPackageDirty();
				ChildInstance->SetParent(Object);
				UMaterialInstance* EditedInstance = Cast<UMaterialInstance>(FunctionPreviewMaterial);
				if (EditedInstance)
				{
					ChildInstance->ScalarParameterValues = EditedInstance->ScalarParameterValues;
					ChildInstance->VectorParameterValues = EditedInstance->VectorParameterValues;
					ChildInstance->DoubleVectorParameterValues = EditedInstance->DoubleVectorParameterValues;
					ChildInstance->TextureParameterValues = EditedInstance->TextureParameterValues;
					ChildInstance->RuntimeVirtualTextureParameterValues = EditedInstance->RuntimeVirtualTextureParameterValues;
					ChildInstance->SparseVolumeTextureParameterValues = EditedInstance->SparseVolumeTextureParameterValues;
					ChildInstance->FontParameterValues = EditedInstance->FontParameterValues;

					const FStaticParameterSet& StaticParameters = EditedInstance->GetStaticParameters();
					ChildInstance->StaticSwitchParameterValues = StaticParameters.StaticSwitchParameters;
					ChildInstance->StaticComponentMaskParameterValues = StaticParameters.EditorOnly.StaticComponentMaskParameters;
				}
			}
		}
	}
	return FReply::Handled();
}

bool FMaterialPropertyHelpers::IsOverriddenExpression(UDEditorParameterValue* Parameter)
{
	return Parameter && Parameter->bOverride != 0;
}

ECheckBoxState FMaterialPropertyHelpers::IsOverriddenExpressionCheckbox(UDEditorParameterValue* Parameter)
{
	return IsOverriddenExpression(Parameter) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMaterialPropertyHelpers::OnOverrideParameter(bool NewValue, class UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance)
{
	// If the material instance disallows the creation of new shader permutations, prevent overriding the static parameter.
	if (NewValue && Parameter->IsStaticParameter() && MaterialEditorInstance->SourceInstance->bDisallowStaticParameterPermutations)
	{
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT( "OverrideParameter", "Override Parameter" ) );
	Parameter->Modify();
	Parameter->bOverride = NewValue;

	// Fire off a dummy event to the material editor instance, so it knows to update the material, then refresh the viewports.
	FPropertyChangedEvent OverrideEvent(NULL);
	MaterialEditorInstance->PostEditChangeProperty( OverrideEvent );
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
}

FText FMaterialPropertyHelpers::GetParameterExpressionDescription(UDEditorParameterValue* Parameter, UObject* MaterialEditorInstance)
{
	return FText::FromString(Parameter->Description);
}

FText FMaterialPropertyHelpers::GetParameterTooltip(UDEditorParameterValue* Parameter, UObject* MaterialEditorInstance)
{
	const FText AssetPath = FText::FromString(Parameter->AssetPath);
	FText TooltipText;

	// If the material instance disallows the creation of new shader permutations, prevent overriding the static parameter.
	if (Parameter->IsStaticParameter() && static_cast<UMaterialEditorInstanceConstant*>(MaterialEditorInstance)->SourceInstance->bDisallowStaticParameterPermutations)
	{
		return FText::FromString(TEXT("This material instance parent restricts the creation of new shader permutations. Overriding this parameter would result in the generation of additional shader permutations."));
	}
	else if (!Parameter->Description.IsEmpty())
	{
		TooltipText = FText::Format(LOCTEXT("ParameterInfoDescAndLocation", "{0} \nFound in: {1}"), FText::FromString(Parameter->Description), AssetPath);
	}
	else
	{
		TooltipText = FText::Format(LOCTEXT("ParameterInfoLocationOnly", "Found in: {0}"), AssetPath);
	}
	return TooltipText;
}


void FMaterialPropertyHelpers::ResetToDefault(class UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance)
{
	const FScopedTransaction Transaction( LOCTEXT( "ResetToDefault", "Reset To Default" ) );
	Parameter->Modify();

	FMaterialParameterMetadata EditorValue;
	if (Parameter->GetValue(EditorValue))
	{
		FMaterialParameterMetadata DefaultValue;
		if (MaterialEditorInstance->SourceInstance->GetParameterDefaultValue(EditorValue.Value.Type, Parameter->ParameterInfo, DefaultValue))
		{
			Parameter->SetValue(DefaultValue.Value);
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
}

void FMaterialPropertyHelpers::ResetLayerAssetToDefault(UDEditorParameterValue* InParameter, TEnumAsByte<EMaterialParameterAssociation> InAssociation, int32 Index, UMaterialEditorInstanceConstant* MaterialEditorInstance)
{
	const FScopedTransaction Transaction(LOCTEXT("ResetToDefault", "Reset To Default"));
	InParameter->Modify();

	UDEditorMaterialLayersParameterValue* LayersParam = Cast<UDEditorMaterialLayersParameterValue>(InParameter);
	if (LayersParam)
	{
		FMaterialLayersFunctions LayersValue;
		if (MaterialEditorInstance->Parent->GetMaterialLayers(LayersValue))
		{
			FMaterialLayersFunctions StoredValue = LayersParam->ParameterValue;

			if (InAssociation == EMaterialParameterAssociation::BlendParameter)
			{
				if (Index < LayersValue.Blends.Num())
				{
					StoredValue.Blends[Index] = LayersValue.Blends[Index];
				}
				else
				{
					StoredValue.Blends[Index] = nullptr;
					MaterialEditorInstance->StoredBlendPreviews[Index] = nullptr;
				}
			}
			else if (InAssociation == EMaterialParameterAssociation::LayerParameter)
			{
				if (Index < LayersValue.Layers.Num())
				{
					StoredValue.Layers[Index] = LayersValue.Layers[Index];
				}
				else
				{
					StoredValue.Layers[Index] = nullptr;
					MaterialEditorInstance->StoredLayerPreviews[Index] = nullptr;
				}
			}
			LayersParam->ParameterValue = StoredValue;
		}
	}

	FPropertyChangedEvent OverrideEvent(NULL);
	MaterialEditorInstance->PostEditChangeProperty(OverrideEvent);
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	FEditorSupportDelegates::UpdateUI.Broadcast();
	
}

bool FMaterialPropertyHelpers::ShouldLayerAssetShowResetToDefault(TSharedPtr<FSortedParamData> InParameterData, UMaterialInstanceConstant* InMaterialInstance)
{
	if (!InParameterData->Parameter)
	{
		return false;
	}

	TArray<class UMaterialFunctionInterface*> StoredAssets;
	TArray<class UMaterialFunctionInterface*> ParentAssets;

	int32 Index = InParameterData->ParameterInfo.Index;
	UDEditorMaterialLayersParameterValue* LayersParam = Cast<UDEditorMaterialLayersParameterValue>(InParameterData->Parameter);
	if (LayersParam)
	{
		FMaterialLayersFunctions LayersValue;
		UMaterialInterface* Parent = InMaterialInstance->Parent;
		if (Parent && Parent->GetMaterialLayers(LayersValue))
		{
			FMaterialLayersFunctions StoredValue = LayersParam->ParameterValue;

			if (InParameterData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
			{
				StoredAssets = StoredValue.Blends;
				ParentAssets = LayersValue.Blends;

			}
			else if (InParameterData->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter)
			{
				StoredAssets = StoredValue.Layers;
				ParentAssets = LayersValue.Layers;
			}

			// Compare to the parent MaterialFunctionInterface array
			if (Index < ParentAssets.Num())
			{
				if (StoredAssets[Index] == ParentAssets[Index])
				{
					return false;
				}
				else
				{
					return true;
				}
			}
			else if (StoredAssets[Index] != nullptr)
			{
				return true;
			}
		}
	}

	return false;
}

bool FMaterialPropertyHelpers::ShouldShowResetToDefault(UDEditorParameterValue* InParameter, UMaterialEditorInstanceConstant* MaterialEditorInstance)
{
	//const FMaterialParameterInfo& ParameterInfo = InParameter->ParameterInfo;

	FMaterialParameterMetadata EditorValue;
	if (InParameter->GetValue(EditorValue))
	{
		FMaterialParameterMetadata SourceValue;
		MaterialEditorInstance->SourceInstance->GetParameterDefaultValue(EditorValue.Value.Type, InParameter->ParameterInfo, SourceValue);
		if (EditorValue.Value != SourceValue.Value)
		{
			return true;
		}
	}

	return false;
}

FEditorParameterGroup&  FMaterialPropertyHelpers::GetParameterGroup(UMaterial* InMaterial, FName& ParameterGroup, TArray<struct FEditorParameterGroup>& ParameterGroups)
{
	if (ParameterGroup == TEXT(""))
	{
		ParameterGroup = TEXT("None");
	}
	for (int32 i = 0; i < ParameterGroups.Num(); i++)
	{
		FEditorParameterGroup& Group = ParameterGroups[i];
		if (Group.GroupName == ParameterGroup)
		{
			return Group;
		}
	}
	int32 ind = ParameterGroups.AddZeroed(1);
	FEditorParameterGroup& Group = ParameterGroups[ind];
	Group.GroupName = ParameterGroup;
	UMaterial* ParentMaterial = InMaterial;
	int32 NewSortPriority;
	if (ParentMaterial->GetGroupSortPriority(ParameterGroup.ToString(), NewSortPriority))
	{
		Group.GroupSortPriority = NewSortPriority;
	}
	else
	{
		Group.GroupSortPriority = 0;
	}
	Group.GroupAssociation = EMaterialParameterAssociation::GlobalParameter;

	return Group;
}

void FMaterialPropertyHelpers::GetVectorChannelMaskComboBoxStrings(TArray<TSharedPtr<FString>>& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems)
{
	const UEnum* ChannelEnum = StaticEnum<EChannelMaskParameterColor::Type>();
	check(ChannelEnum);

	// Add RGBA string options (Note: Exclude the "::Max" entry)
	const int32 NumEnums = ChannelEnum->NumEnums() - 1;
	for (int32 Entry = 0; Entry < NumEnums; ++Entry)
	{
		FText EnumName = ChannelEnum->GetDisplayNameTextByIndex(Entry);

		OutComboBoxStrings.Add(MakeShared<FString>(EnumName.ToString()));
		OutToolTips.Add(SNew(SToolTip).Text(EnumName));
		OutRestrictedItems.Add(false);
	}
}

FString FMaterialPropertyHelpers::GetVectorChannelMaskValue(UDEditorParameterValue* InParameter)
{
	UDEditorVectorParameterValue* VectorParam = Cast<UDEditorVectorParameterValue>(InParameter);
	check(VectorParam && VectorParam->bIsUsedAsChannelMask);

	const UEnum* ChannelEnum = StaticEnum<EChannelMaskParameterColor::Type>();
	check(ChannelEnum);

	// Convert from vector to RGBA string
	int64 ChannelType = 0;

	if (VectorParam->ParameterValue.R > 0.0f)
	{
		ChannelType = EChannelMaskParameterColor::Red;
	}
	else if (VectorParam->ParameterValue.G > 0.0f)
	{
		ChannelType = EChannelMaskParameterColor::Green;
	}
	else if (VectorParam->ParameterValue.B > 0.0f)
	{
		ChannelType = EChannelMaskParameterColor::Blue;
	}
	else
	{
		ChannelType = EChannelMaskParameterColor::Alpha;
	}

	return ChannelEnum->GetDisplayNameTextByValue(ChannelType).ToString();
}

void FMaterialPropertyHelpers::SetVectorChannelMaskValue(const FString& StringValue, TSharedPtr<IPropertyHandle> PropertyHandle, UDEditorParameterValue* InParameter, UObject* MaterialEditorInstance)
{
	UDEditorVectorParameterValue* VectorParam = Cast<UDEditorVectorParameterValue>(InParameter);
	check(VectorParam && VectorParam->bIsUsedAsChannelMask);

	const UEnum* ChannelEnum = StaticEnum<EChannelMaskParameterColor::Type>();
	check(ChannelEnum);

	// Convert from RGBA string to vector
	int64 ChannelValue = ChannelEnum->GetValueByNameString(StringValue);
	FLinearColor NewValue;

	switch (ChannelValue)
	{
	case EChannelMaskParameterColor::Red:
		NewValue = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f); break;
	case EChannelMaskParameterColor::Green:
		NewValue = FLinearColor(0.0f, 1.0f, 0.0f, 0.0f); break;
	case EChannelMaskParameterColor::Blue:
		NewValue = FLinearColor(0.0f, 0.0f, 1.0f, 0.0f); break;
	default:
		NewValue = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}

	// If changed, propagate the update
	if (VectorParam->ParameterValue != NewValue)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetVectorChannelMaskValue", "Set Vector Channel Mask Value"));
		VectorParam->Modify();

		PropertyHandle->NotifyPreChange();
		VectorParam->ParameterValue = NewValue;

		UMaterialEditorInstanceConstant* MaterialInstanceEditor = Cast<UMaterialEditorInstanceConstant>(MaterialEditorInstance);
		if (MaterialInstanceEditor)
		{
			MaterialInstanceEditor->CopyToSourceInstance();
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

TArray<UFactory*> FMaterialPropertyHelpers::GetAssetFactories(EMaterialParameterAssociation AssetType)
{
	TArray<UFactory*> NewAssetFactories;
	switch (AssetType)
	{
	case LayerParameter:
	{
	//	NewAssetFactories.Add(NewObject<UMaterialFunctionMaterialLayerFactory>());
		break;
	}
	case BlendParameter:
	{
	//	NewAssetFactories.Add(NewObject<UMaterialFunctionMaterialLayerBlendFactory>());
		break;
	}
	case GlobalParameter:
		break;
	default:
		break;
	}
	
	return NewAssetFactories;
}


TSharedRef<SWidget> FMaterialPropertyHelpers::MakeStackReorderHandle(TSharedPtr<SMaterialLayersFunctionsInstanceTreeItem> InOwningStack)
{
	TSharedRef<SLayerHandle> Handle = SNew(SLayerHandle)
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.Padding(5.0f, 0.0f)
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("VerticalBoxDragIndicatorShort"))
		]
		]
	.OwningStack(InOwningStack);
	return Handle;
}

bool FMaterialPropertyHelpers::OnShouldSetCurveAsset(const FAssetData& AssetData, TSoftObjectPtr<class UCurveLinearColorAtlas> InAtlas)
{
	UCurveLinearColorAtlas* Atlas = Cast<UCurveLinearColorAtlas>(InAtlas.Get());
	if (!Atlas)
	{
		return false;
	}

	for (UCurveLinearColor* GradientCurve : Atlas->GradientCurves)
	{
		if (!GradientCurve || !GradientCurve->GetOutermost())
		{
			continue;
		}

		if (GradientCurve->GetOutermost()->GetPathName() == AssetData.PackageName.ToString())
		{
			return true;
		}
	}

	return false;
}

bool FMaterialPropertyHelpers::OnShouldFilterCurveAsset(const FAssetData& AssetData, TSoftObjectPtr<class UCurveLinearColorAtlas> InAtlas)
{
	return !OnShouldSetCurveAsset(AssetData, InAtlas);
}

void FMaterialPropertyHelpers::SetPositionFromCurveAsset(const FAssetData& AssetData, TSoftObjectPtr<class UCurveLinearColorAtlas> InAtlas, UDEditorScalarParameterValue* InParameter, TSharedPtr<IPropertyHandle> PropertyHandle, UObject* MaterialEditorInstance)
{
	UCurveLinearColorAtlas* Atlas = Cast<UCurveLinearColorAtlas>(InAtlas.Get());
	UCurveLinearColor* Curve = Cast<UCurveLinearColor>(AssetData.GetAsset());
	if (!Atlas || !Curve)
	{
		return;
	}
	int32 Index = Atlas->GradientCurves.Find(Curve);
	if (Index == INDEX_NONE)
	{
		return;
	}

	float NewValue = (float)Index;

	// If changed, propagate the update
	if (InParameter->ParameterValue != NewValue)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetScalarAtlasPositionValue", "Set Scalar Atlas Position Value"));
		InParameter->Modify();

		InParameter->AtlasData.Curve = TSoftObjectPtr<UCurveLinearColor>(FSoftObjectPath(Curve->GetPathName()));
		InParameter->ParameterValue = NewValue;
		UMaterialEditorInstanceConstant* MaterialInstanceEditor = Cast<UMaterialEditorInstanceConstant>(MaterialEditorInstance);
		if (MaterialInstanceEditor)
		{
			MaterialInstanceEditor->CopyToSourceInstance();
		}
	}
}

void FMaterialPropertyHelpers::ResetCurveToDefault(UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance)
{
	const FScopedTransaction Transaction(LOCTEXT("ResetToDefault", "Reset To Default"));
	Parameter->Modify();
	const FMaterialParameterInfo& ParameterInfo = Parameter->ParameterInfo;

	UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(Parameter);

	if (ScalarParam)
	{
		float OutValue;
		if (MaterialEditorInstance->SourceInstance->GetScalarParameterDefaultValue(ParameterInfo, OutValue))
		{
			ScalarParam->ParameterValue = OutValue;
			
			// Purge cached values, which will cause non-default values for the atlas data to be returned by IsScalarParameterUsedAsAtlasPosition
			MaterialEditorInstance->SourceInstance->ClearParameterValuesEditorOnly();

			// Update the atlas data from default values
			bool TempBool;
			MaterialEditorInstance->SourceInstance->IsScalarParameterUsedAsAtlasPosition(ParameterInfo, TempBool, ScalarParam->AtlasData.Curve, ScalarParam->AtlasData.Atlas);
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
}


#undef LOCTEXT_NAMESPACE

