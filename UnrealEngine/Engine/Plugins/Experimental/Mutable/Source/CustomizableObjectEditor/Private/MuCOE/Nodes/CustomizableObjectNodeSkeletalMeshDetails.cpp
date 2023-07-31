// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMeshDetails.h"

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Misc/AssertionMacros.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/PinViewer/SPinViewer.h"
#include "MuCOE/SCustomizableObjectNodeSkeletalMeshRTMorphSelector.h"
#include "PropertyHandle.h"
#include "Templates/Casts.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectNodeMaterialDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeSkeletalMeshDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeSkeletalMeshDetails);
}


void FCustomizableObjectNodeSkeletalMeshDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();

    if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeSkeletalMesh>(DetailsView->GetSelectedObjects()[0].Get());
	}

    if (!Node)
    {
        return;
    }

    DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeSkeletalMesh, UsedRealTimeMorphTargetNames));
    DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeSkeletalMesh, bUseAllRealTimeMorphs));

    // Needed to draw the CO information before the Material Layer information
    IDetailCategoryBuilder& CustomizableObject = DetailBuilder.EditCategory("CustomizableObject");

    // Cretaing new categories to show the material layers
    IDetailCategoryBuilder& MorphsCategory = DetailBuilder.EditCategory("RealTimeMorphTargets");

	TSharedPtr<SCustomizableObjectNodeSkeletalMeshRTMorphSelector> MorphSelector;
    MorphsCategory.AddCustomRow(LOCTEXT("MaterialLayerCategory", "RealTimeMorphTargets"))
    [
        SAssignNew(MorphSelector, SCustomizableObjectNodeSkeletalMeshRTMorphSelector).Node(Node)
    ];

	TSharedRef<IPropertyHandle> SkeletalMeshProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeSkeletalMesh, SkeletalMesh));
	SkeletalMeshProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(MorphSelector.Get(), &SCustomizableObjectNodeSkeletalMeshRTMorphSelector::UpdateWidget));

	PinViewerAttachToDetailCustomization(DetailBuilder);
}


#undef LOCTEXT_NAMESPACE
