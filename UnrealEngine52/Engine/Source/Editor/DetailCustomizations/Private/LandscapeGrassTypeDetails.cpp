// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeGrassTypeDetails.h"

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "DetailLayoutBuilder.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Misc/AssertionMacros.h"
#include "PropertyHandle.h"
#include "PropertyRestriction.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "LandscapeGrassType.h"

class UObject;

static bool GShowBothPerQualityAndPerPlaformProperties = false;
static FAutoConsoleVariableRef CVarShowBothPerQualityAndPerPlaform(
	TEXT("r.grass.ShowBothPerQualityAndPerPlaformProperties"),
	GShowBothPerQualityAndPerPlaformProperties,
	TEXT("Show both per platform and per quality properties in the editor."));

#define LOCTEXT_NAMESPACE "LandscapeGrassTypeDetails"

TSharedRef<IDetailCustomization> FLandscapeGrassTypeDetails::MakeInstance()
{
	return MakeShareable(new FLandscapeGrassTypeDetails);
}

FLandscapeGrassTypeDetails::~FLandscapeGrassTypeDetails()
{
}

void FLandscapeGrassTypeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	if (GShowBothPerQualityAndPerPlaformProperties)
	{
		return;
	}

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);

	const TArray<FName> PerPlatformProperties =
	{
		GET_MEMBER_NAME_CHECKED(FGrassVariety, GrassDensity),
		GET_MEMBER_NAME_CHECKED(FGrassVariety, StartCullDistance),
		GET_MEMBER_NAME_CHECKED(FGrassVariety, EndCullDistance),
	};

	const TArray<FName> PerQualityProperties =
	{
		GET_MEMBER_NAME_CHECKED(FGrassVariety, GrassDensityQuality),
		GET_MEMBER_NAME_CHECKED(FGrassVariety, StartCullDistanceQuality),
		GET_MEMBER_NAME_CHECKED(FGrassVariety, EndCullDistanceQuality),
	};

	if (Objects.Num() > 0)
	{
		// get the grass variety array
		TSharedPtr<IPropertyHandle> GrassVariety = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeGrassType, GrassVarieties));
		TSharedPtr<IPropertyHandleArray> GrassVarietyArrayHandle = GrassVariety->AsArray();

		if (GrassVarietyArrayHandle.IsValid())
		{
			uint32 NumElements;
			GrassVarietyArrayHandle->GetNumElements(NumElements);

			for (uint32 Index = 0; Index < NumElements; ++Index)
			{
				TSharedRef<IPropertyHandle> ElementHandle = GrassVarietyArrayHandle->GetElement(Index);

				uint32 NumChildren;
				ElementHandle->GetNumChildren(NumChildren);

				for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
				{
					TSharedRef<IPropertyHandle> ChildHandle = ElementHandle->GetChildHandle(ChildIndex).ToSharedRef();
					const FName& ChildPropertyName = ChildHandle->GetProperty()->GetFName();

					if (PerPlatformProperties.Contains(ChildPropertyName) && GEngine && GEngine->UseGrassVarityPerQualityLevels)
					{
						DetailLayout.HideProperty(ChildHandle);
					}

					if (PerQualityProperties.Contains(ChildPropertyName) && GEngine && !GEngine->UseGrassVarityPerQualityLevels)
					{
						DetailLayout.HideProperty(ChildHandle);
					}
				}
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE
