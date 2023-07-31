// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterICVFXCameraComponentDetailsCustomization.h"

#include "DisplayClusterConfigurationStrings.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "CineCameraActor.h"
#include "DisplayClusterRootActor.h"
#include "UObject/SoftObjectPtr.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DisplayClusterICVFXCameraComponentDetailsCustomization"

namespace DisplayClusterICVFXCameraComponentDetailsCustomizationUtils
{
	void SortCategories(const TMap<FName, IDetailCategoryBuilder*>& AllCategoryMap)
	{
		static const TArray<FName> CategoryOrder =
		{
			TEXT("Variable"),
			TEXT("TransformCommon"),
			DisplayClusterConfigurationStrings::categories::ICVFXCategory,
			DisplayClusterConfigurationStrings::categories::CameraColorGradingCategory,
			DisplayClusterConfigurationStrings::categories::OCIOCategory,
			DisplayClusterConfigurationStrings::categories::ChromaKeyCategory,
			DisplayClusterConfigurationStrings::categories::OverrideCategory,
			DisplayClusterConfigurationStrings::categories::ConfigurationCategory
		};

		for (const TPair<FName, IDetailCategoryBuilder*>& Pair : AllCategoryMap)
		{
			int32 CurrentSortOrder = Pair.Value->GetSortOrder();

			int32 DesiredSortOrder;
			if (CategoryOrder.Find(Pair.Key, DesiredSortOrder))
			{
				CurrentSortOrder = DesiredSortOrder;
			}
			else
			{
				CurrentSortOrder += CategoryOrder.Num();
			}

			Pair.Value->SetSortOrder(CurrentSortOrder);
		}
	}
}

TSharedRef<IDetailCustomization> FDisplayClusterICVFXCameraComponentDetailsCustomization::MakeInstance()
{
	return MakeShared<FDisplayClusterICVFXCameraComponentDetailsCustomization>();
}

void FDisplayClusterICVFXCameraComponentDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& InLayoutBuilder)
{
	DetailLayout = &InLayoutBuilder;

	if (!EditedObject.IsValid())
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		InLayoutBuilder.GetObjectsBeingCustomized(Objects);

		for (TWeakObjectPtr<UObject> Object : Objects)
		{
			if (Object->IsA<UDisplayClusterICVFXCameraComponent>())
			{
				EditedObject = Cast<UDisplayClusterICVFXCameraComponent>(Object.Get());
			}
		}
	}

	// Hide some groups if an external CineCameraActor is set
	if (EditedObject.IsValid() && EditedObject->CameraSettings.ExternalCameraActor.IsValid())
	{
		InLayoutBuilder.HideCategory(TEXT("TransformCommon"));
		InLayoutBuilder.HideCategory(TEXT("Current Camera Settings"));
		InLayoutBuilder.HideCategory(TEXT("CameraOptions"));
		InLayoutBuilder.HideCategory(TEXT("Camera"));
		InLayoutBuilder.HideCategory(TEXT("PostProcess"));
		InLayoutBuilder.HideCategory(TEXT("Lens"));
		InLayoutBuilder.HideCategory(TEXT("LOD"));
		InLayoutBuilder.HideCategory(TEXT("ColorGrading"));
		InLayoutBuilder.HideCategory(TEXT("RenderingFeatures"));
		InLayoutBuilder.HideCategory(TEXT("Color Grading"));
		InLayoutBuilder.HideCategory(TEXT("Rendering Features"));
	}

	// Sockets category must be hidden manually instead of through the HideCategories metadata specifier
	InLayoutBuilder.HideCategory(TEXT("Sockets"));

	// Manually label the ICVFX category to properly format it to have the dash in "In-Camera"
	InLayoutBuilder.EditCategory(DisplayClusterConfigurationStrings::categories::ICVFXCategory, LOCTEXT("ICVFXCategoryLabel", "In-Camera VFX"));

	InLayoutBuilder.SortCategories(DisplayClusterICVFXCameraComponentDetailsCustomizationUtils::SortCategories);

	// Most of the properties in the camera settings are exposed through property references, so hide the camera settings property.
	InLayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UDisplayClusterICVFXCameraComponent, CameraSettings));
}

#undef LOCTEXT_NAMESPACE
