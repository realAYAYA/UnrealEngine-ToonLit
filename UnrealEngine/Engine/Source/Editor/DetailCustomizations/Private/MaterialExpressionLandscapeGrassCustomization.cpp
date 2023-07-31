// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialExpressionLandscapeGrassCustomization.h"

#include "AssetRegistry/AssetData.h"
#include "Containers/Array.h"
#include "DetailWidgetRow.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "LandscapeGrassType.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Misc/AssertionMacros.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SlotBase.h"
#include "Templates/Casts.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"

class SWidget;
class UObject;

#define LOCTEXT_NAMESPACE "MaterialExpressionLandscapeGrassCustomization"

TSharedRef<IPropertyTypeCustomization> FMaterialExpressionLandscapeGrassInputCustomization::MakeInstance()
{
	return MakeShareable(new FMaterialExpressionLandscapeGrassInputCustomization);
}

void FMaterialExpressionLandscapeGrassInputCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FMaterialExpressionLandscapeGrassInputCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	TSharedPtr<IPropertyHandle> GrassTypeHandle;

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = StructPropertyHandle->GetChildHandle(i);

		if (PropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FGrassInput, GrassType))
		{
			GrassTypeHandle = PropertyHandle;
		}
		else
		{
			StructBuilder.AddProperty(PropertyHandle.ToSharedRef());
		}
	}

	TArray<UObject*> OwningObjects;
	StructPropertyHandle->GetOuterObjects(OwningObjects);

	if (OwningObjects.Num() == 1)
	{
		MaterialNode = CastChecked<UMaterialExpressionLandscapeGrassOutput>(OwningObjects[0]);
	}

	IDetailPropertyRow& GrassTypeRow = StructBuilder.AddProperty(GrassTypeHandle.ToSharedRef());
	
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	FDetailWidgetRow Row;
	GrassTypeRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

	FDetailWidgetRow &DetailWidgetRow = GrassTypeRow.CustomWidget();
	DetailWidgetRow.NameContent()
		.MinDesiredWidth(Row.NameWidget.MinWidth)
		.MaxDesiredWidth(Row.NameWidget.MaxWidth)
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(Row.ValueContent().MaxWidth)
		.MaxDesiredWidth(Row.ValueContent().MaxWidth)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(ULandscapeGrassType::StaticClass())
				.PropertyHandle(GrassTypeHandle)
				.ThumbnailPool(StructCustomizationUtils.GetThumbnailPool())
				.OnShouldFilterAsset(this, &FMaterialExpressionLandscapeGrassInputCustomization::OnShouldFilterAsset)
			]
		];
}

bool FMaterialExpressionLandscapeGrassInputCustomization::OnShouldFilterAsset(const FAssetData& InAssetData)
{
	if (MaterialNode != nullptr)
	{
		for (const FGrassInput& GrassInput : MaterialNode->GrassTypes)
		{
			if (GrassInput.GrassType != nullptr)
			{
				if (GrassInput.GrassType->GetFName() == InAssetData.AssetName) // already used
				{
					return true;
				}
			}
		}		
	}

	return false;
}

#undef LOCTEXT_NAMESPACE 
