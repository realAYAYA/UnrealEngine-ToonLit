// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParameterBlockParameterCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyBagDetails.h"
#include "EdGraphSchema_K2.h"
#include "EditorUtils.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "UncookedOnlyUtils.h"
#include "Param/ParamType.h"
#include "PropertyHandle.h"
#include "Param/AnimNextParameterBlockParameter.h"
#include "Param/AnimNextParameterBlock.h"
#include "ISinglePropertyView.h"
#include "IStructureDataProvider.h"
#include "UncookedOnlyUtils.h"
#include "Param/AnimNextParameterBlock_EditorData.h"

#define LOCTEXT_NAMESPACE "ParamTypePropertyCustomization"

namespace UE::AnimNext::Editor
{

void FParameterBlockParameterCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.IsEmpty() || Objects.Num() > 1)
	{
		return;
	}

	if (UAnimNextParameterBlockParameter* BlockParam = Cast<UAnimNextParameterBlockParameter>(Objects[0].Get()))
	{
		IDetailCategoryBuilder& ParameterCategory = DetailBuilder.EditCategory(TEXT("Parameter"), FText::GetEmpty(), ECategoryPriority::Important);

		IDetailCategoryBuilder& DefaultValueCategory = DetailBuilder.EditCategory(TEXT("DefaultValue"), FText::GetEmpty(), ECategoryPriority::Default);

		TSharedRef< SWidget > ColumnWidget = SNullWidget::NullWidget;

		if (UAnimNextParameterBlock_EditorData* EditorData = Cast<UAnimNextParameterBlock_EditorData>(BlockParam->GetOuter()))
		{
			const FName ParameterName = BlockParam->GetEntryName();
			if (UAnimNextRigVMAssetEntry* BlockEntry = EditorData->FindEntry(ParameterName)) 
			{
				if (UAnimNextParameterBlock* ReferencedBlock = UE::AnimNext::UncookedOnly::FUtils::GetBlock(EditorData))
				{
					FAddPropertyParams AddPropertyParams;
					TArray<IDetailPropertyRow*> DetailPropertyRows;

					if (ReferencedBlock->PropertyBag.FindPropertyDescByName(ParameterName))
					{
						IDetailPropertyRow* DetailPropertyRow = DefaultValueCategory.AddExternalStructureProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(ReferencedBlock->PropertyBag), ParameterName, EPropertyLocation::Default, AddPropertyParams);
						if (TSharedPtr<IPropertyHandle> Handle = DetailPropertyRow->GetPropertyHandle(); Handle.IsValid())
						{
							Handle->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateLambda([this, ReferencedBlock]()
							{
								ReferencedBlock->Modify(); // needed to enable the transaction when we modify the PropertyBag
							}));
							Handle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, ReferencedBlock, ParameterName]()
							{
								if (UAnimNextParameterBlock_EditorData* EditorData = Cast<UAnimNextParameterBlock_EditorData>(ReferencedBlock->EditorData))
								{
									if (UAnimNextRigVMAssetEntry* BlockEntry = EditorData->FindEntry(ParameterName))
									{
										BlockEntry->MarkPackageDirty();
										ReferencedBlock->MarkPackageDirty(); // show the ParameterBlock has changed
									}
								}
							}));
						}
					}
				}
			}
		}
	}
}

void FParameterBlockParameterCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) 
{
	CustomizeDetails(*DetailBuilder);
}

FText FParameterBlockParameterCustomization::GetName() const
{
	return FText();
}

void FParameterBlockParameterCustomization::SetName(const FText& InNewText, ETextCommit::Type InCommitType)
{
}

bool FParameterBlockParameterCustomization::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	return true;
}


}

#undef LOCTEXT_NAMESPACE
