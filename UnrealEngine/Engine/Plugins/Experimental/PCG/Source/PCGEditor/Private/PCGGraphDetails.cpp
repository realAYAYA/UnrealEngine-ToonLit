// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphDetails.h"
#include "PCGGraph.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"

#define LOCTEXT_NAMESPACE "PCGGraphDetails"

TSharedRef<IDetailCustomization> FPCGGraphDetails::MakeInstance()
{
	return MakeShareable(new FPCGGraphDetails());
}

void FPCGGraphDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const FName PCGCategoryName("PCG");
	IDetailCategoryBuilder& PCGCategory = DetailBuilder.EditCategory(PCGCategoryName);

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	for (TWeakObjectPtr<UObject>& Object : ObjectsBeingCustomized)
	{
		UPCGGraph* Graph = Cast<UPCGGraph>(Object.Get());
		if (ensure(Graph))
		{
			SelectedGraphs.Add(Graph);
		}
	}

	TArray<TSharedRef<IPropertyHandle>> AllProperties;
	bool bSimpleProperties = true;
	bool bAdvancedProperties = false;
	// Add all properties in the category in order
	PCGCategory.GetDefaultProperties(AllProperties, bSimpleProperties, bAdvancedProperties);

	for (auto& Property : AllProperties)
	{
		PCGCategory.AddProperty(Property);
	}
}

#undef LOCTEXT_NAMESPACE