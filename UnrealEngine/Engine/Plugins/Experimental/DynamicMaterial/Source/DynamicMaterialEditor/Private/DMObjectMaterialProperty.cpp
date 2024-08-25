// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMObjectMaterialProperty.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Internationalization/Text.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Model/DynamicMaterialModel.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DMObjectMaterialProperty"

FDMObjectMaterialProperty::FDMObjectMaterialProperty()
	: OuterWeak(nullptr)
	, Property(nullptr)
	, PropertyName(NAME_None)
	, Index(INDEX_NONE)
{
}

FDMObjectMaterialProperty::FDMObjectMaterialProperty(UPrimitiveComponent* InOuter, int32 InIndex)
	: OuterWeak(InOuter)
	, Property(nullptr)
	, PropertyName(NAME_None)
	, Index(InIndex)
{
}

FDMObjectMaterialProperty::FDMObjectMaterialProperty(UObject* InOuter, FProperty* InProperty, int32 InIndex)
	: OuterWeak(InOuter)
	, Property(InProperty)
	, PropertyName(InProperty ? InProperty->GetFName() : NAME_None)
	, Index(InIndex)
{
}

UDynamicMaterialModel* FDMObjectMaterialProperty::GetMaterialModel() const
{
	if (UDynamicMaterialInstance* Instance = GetMaterial())
	{
		return Instance->GetMaterialModel();
	}

	return nullptr;
}

UDynamicMaterialInstance* FDMObjectMaterialProperty::GetMaterial() const
{
	UObject* Outer = OuterWeak.Get();

	if (!Outer)
	{
		return nullptr;
	}

	if (Property != nullptr)
	{
		UMaterialInterface* Material = nullptr;

		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);

			if (ObjectProperty && ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Outer));

				if (ArrayHelper.IsValidIndex(Index))
				{
					void* Value = ArrayHelper.GetRawPtr(Index);
					Material = *reinterpret_cast<UMaterialInterface**>(Value);
				}
			}
		}
		else
		{
			FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);

			if (ObjectProperty && ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
			{
				Property->GetValue_InContainer(Outer, &Material);
			}
		}

		if (Material)
		{
			return Cast<UDynamicMaterialInstance>(Material);
		}
	}
	else if (Index != INDEX_NONE)
	{
		if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Outer))
		{
			if (Index >= 0 && Index < Component->GetNumMaterials())
			{
				return Cast<UDynamicMaterialInstance>(Component->GetMaterial(Index));
			}
		}
	}

	return nullptr;
}

void FDMObjectMaterialProperty::SetMaterial(UDynamicMaterialInstance* DynamicMaterial)
{
	UObject* Outer = OuterWeak.Get();

	if (!Outer)
	{
		return;
	}

	if (Property != nullptr)
	{
		Outer->PreEditChange(Property);

		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);

			if (ObjectProperty && ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Outer));

				if (ArrayHelper.IsValidIndex(Index))
				{
					*reinterpret_cast<UMaterialInterface**>(ArrayHelper.GetRawPtr(Index)) = DynamicMaterial;
				}
			}
		}
		else
		{
			FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);

			if (ObjectProperty && ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
			{
				Property->SetValue_InContainer(Outer, &DynamicMaterial);
			}
		}

		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
		Outer->PostEditChangeProperty(PropertyChangedEvent);
	}
	else if (Index != INDEX_NONE)
	{
		if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Outer))
		{
			if (Index >= 0 && Index < Component->GetNumMaterials())
			{
				Component->SetMaterial(Index, DynamicMaterial);
			}
		}
	}
}

bool FDMObjectMaterialProperty::IsValid() const
{
	UObject* Outer = OuterWeak.Get();

	if (!Outer)
	{
		return false;
	}

	if (Property != nullptr)
	{
		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);

			if (ObjectProperty && ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Outer));

				return ArrayHelper.IsValidIndex(Index);
			}
		}
		else
		{
			FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);

			if (ObjectProperty && ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
			{
				return true;
			}
		}

		return false;
	}
	else if (Index != INDEX_NONE)
	{
		if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Outer))
		{
			if (Index >= 0 && Index < Component->GetNumMaterials())
			{
				return true;
			}
		}
	}

	return false;
}

FText FDMObjectMaterialProperty::GetPropertyName(bool bInIgnoreNewStatus) const
{
	UObject* Outer = OuterWeak.Get();

	if (!Outer)
	{
		return FText::GetEmpty();
	}

	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	if (Property != nullptr)
	{
		FText PropertyNameText = Property->GetDisplayNameText();

		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			PropertyNameText = FText::Format(
				LOCTEXT("PropertyNameFormatArray", "{0} [{1}]"),
				PropertyNameText,
				FText::AsNumber(Index)
			);
		}

		if (MaterialModel || bInIgnoreNewStatus)
		{
			return FText::Format(
				LOCTEXT("PropertyNameFormat", "{0}"),
				PropertyNameText
			);
		}
		else
		{
			return FText::Format(
				LOCTEXT("PropertyNameFormatNew", "{0} (Create New)"),
				PropertyNameText
			);
		}
	}
	else if (Index != INDEX_NONE)
	{
		if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Outer))
		{
			if (Index >= 0 && Index <= Component->GetNumMaterials())
			{
				if (MaterialModel || bInIgnoreNewStatus)
				{
					return FText::Format(
						LOCTEXT("MaterialListNameFormat", "Material Slot {0}"),
						FText::AsNumber(Index)
					);
				}
				else
				{
					return FText::Format(
						LOCTEXT("MaterialListNameFormatNew", "Material Slot {0} (Create New)"),
						FText::AsNumber(Index)
					);
				}
			}
		}

		return FText::GetEmpty();
	}

	return FText::GetEmpty();
}

void FDMObjectMaterialProperty::Reset()
{
	OuterWeak = nullptr;
	Property = nullptr;
	PropertyName = NAME_None;
	Index = INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE
