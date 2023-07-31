// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGSettingsHelpers.h"

#include "PCGComponent.h"
#include "PCGHelpers.h"
#include "PCGSettings.h"

namespace PCGSettingsHelpers
{
	template<typename T>
	bool GetParamValue(T& Value, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InKey = 0)
	{
		if (InAttribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id)
		{
			Value = static_cast<const FPCGMetadataAttribute<T>*>(InAttribute)->GetValueFromItemKey(InKey);
			return true;
		}
		else
		{
			return false;
		}
	}

	template<typename T, typename U, typename... OtherTypes>
	bool GetParamValue(T& Value, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InKey = 0)
	{
		if (InAttribute->GetTypeId() == PCG::Private::MetadataTypes<U>::Id)
		{
			Value = (T)static_cast<const FPCGMetadataAttribute<U>*>(InAttribute)->GetValueFromItemKey(InKey);
			return true;
		}
		else
		{
			return GetParamValue<T, OtherTypes...>(Value, InAttribute, InKey);
		}
	}

	void SetValue(UPCGParamData* InParams, UObject* Object, FProperty* Property)
	{
		check(InParams && Property);
		const FPCGMetadataAttributeBase* MatchingAttribute = InParams->Metadata ? InParams->Metadata->GetConstAttribute(Property->GetFName()) : nullptr;

		// TODO? Support arrays (1) which would be possible if we have a consistent naming scheme
		if (!MatchingAttribute)
		{
			return;
		}

		bool bTypeError = false;

		if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			if (NumericProperty->IsFloatingPoint())
			{
				double Value = 0;
				if (GetParamValue<double, float>(Value, MatchingAttribute))
				{
					NumericProperty->SetFloatingPointPropertyValue(NumericProperty->ContainerPtrToValuePtr<double>(Object, 0), Value);
				}
				else
				{
					bTypeError = true;
				}
			}
			else if(NumericProperty->IsInteger())
			{
				int64 Value = 0;
				if (GetParamValue<int64, int32>(Value, MatchingAttribute))
				{
					NumericProperty->SetIntPropertyValue(NumericProperty->ContainerPtrToValuePtr<int64>(Object, 0), Value);
				}
				else
				{
					bTypeError = true;
				}
			}
			else
			{
				// Not a float and not an integer...?
				bTypeError = true;
			}
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool Value = 0;
			if(GetParamValue<bool>(Value, MatchingAttribute))
			{
				BoolProperty->SetPropertyValue(BoolProperty->ContainerPtrToValuePtr<bool>(Object, 0), Value);
			}
			else
			{
				bTypeError = true;
			}
		}
		else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			FString Value;
			if (GetParamValue<FString>(Value, MatchingAttribute))
			{
				StringProperty->SetPropertyValue(StringProperty->ContainerPtrToValuePtr<FString>(Object, 0), Value);
			}
			else
			{
				bTypeError = true;
			}
		}
		else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			FString Value;
			if (GetParamValue<FString>(Value, MatchingAttribute))
			{
				FName NValue(Value);
				NameProperty->SetPropertyValue(NameProperty->ContainerPtrToValuePtr<FName>(Object), NValue);
			}
			else
			{
				bTypeError = true;
			}
		}
		else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();

			int64 Value;
			if (GetParamValue<int64, int32, uint32, int16, uint16, int8, uint8>(Value, MatchingAttribute))
			{
				UnderlyingProperty->SetIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Object, 0), Value);
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			const FName PropertyTypeName = StructProperty->Struct->GetFName();
			void* PropertyValue = StructProperty->ContainerPtrToValuePtr<void>(Object, 0);

			if (PropertyTypeName == NAME_Vector)
			{
				FVector Value = FVector::Zero();
				if(GetParamValue<FVector, double, float>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FVector*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			else if (PropertyTypeName == NAME_Vector4)
			{
				FVector4 Value = FVector4::Zero();
				if (GetParamValue<FVector4, double, float>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FVector4*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			else if (PropertyTypeName == NAME_Quat)
			{
				FQuat Value = FQuat::Identity;
				if (GetParamValue<FQuat, FRotator>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FQuat*>(PropertyValue) = Value;
				}
			}
			else if (PropertyTypeName == NAME_Transform)
			{
				FTransform Value = FTransform::Identity;
				if (GetParamValue<FTransform, FVector>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FTransform*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			else if (PropertyTypeName == NAME_Rotator)
			{
				FRotator Value = FRotator::ZeroRotator;
				if (GetParamValue<FRotator, FQuat>(Value, MatchingAttribute))
				{
					*reinterpret_cast<FRotator*>(PropertyValue) = Value;
				}
				else
				{
					bTypeError = true;
				}
			}
			//else if (PropertyTypeName == NAME_Color)
			//else if (PropertyTypeName == NAME_LinearColor)
			//else if soft object path to something?
			else
			{
				bTypeError = true;
			}
		}

		if (bTypeError)
		{
			// Mismatching types
			UE_LOG(LogPCG, Warning, TEXT("Mismatching type in params vs. property %s"), *Property->GetFName().ToString());
		}
	}

	int ComputeSeedWithOverride(const UPCGSettings* InSettings, const UPCGComponent* InComponent, UPCGParamData* InParams)
	{
		check(InSettings);

		const int SettingsSeed = InParams ? PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGSettings, Seed), InSettings->Seed, InParams) : InSettings->Seed;
		return InComponent ? PCGHelpers::ComputeSeed(SettingsSeed, InComponent->Seed) : SettingsSeed;
	}
}