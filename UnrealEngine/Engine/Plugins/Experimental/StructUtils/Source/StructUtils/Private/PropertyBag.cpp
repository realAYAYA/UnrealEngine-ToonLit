// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBag.h"
#include "StructView.h"
#include "Hash/CityHash.h"
#include "Misc/Guid.h"
#include "UObject/TextProperty.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PropertyBag)

struct STRUCTUTILS_API FPropertyBagCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// Added support for array types
		ContainerTypes = 1,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FPropertyBagCustomVersion() {}
};


const FGuid FPropertyBagCustomVersion::GUID(0x134A157E, 0xD5E249A3, 0x8D4E843C, 0x98FE9E31);

// Register the custom version with core
FCustomVersionRegistration GPropertyBagCustomVersion(FPropertyBagCustomVersion::GUID, FPropertyBagCustomVersion::LatestVersion, TEXT("PropertyBagCustomVersion"));


namespace UE::StructUtils::Private
{

	bool CanCastTo(const UStruct* From, const UStruct* To)
	{
		return From != nullptr && To != nullptr && From->IsChildOf(To);
	}

	uint64 GetObjectHash(const UObject* Object)
	{
		const FString PathName = GetPathNameSafe(Object);
		return CityHash64((const char*)GetData(PathName), PathName.Len() * sizeof(TCHAR));
	}

	uint64 CalcPropertyDescHash(const FPropertyBagPropertyDesc& Desc)
	{
#if WITH_EDITORONLY_DATA
		const uint32 Hashes[] = { GetTypeHash(Desc.ID), GetTypeHash(Desc.Name), GetTypeHash(Desc.ValueType), GetTypeHash(Desc.ContainerType), GetTypeHash(Desc.MetaData) };
#else
		const uint32 Hashes[] = { GetTypeHash(Desc.ID), GetTypeHash(Desc.Name), GetTypeHash(Desc.ValueType), GetTypeHash(Desc.ContainerType)};
#endif
		return CityHash64WithSeed((const char*)Hashes, sizeof(Hashes), GetObjectHash(Desc.ValueTypeObject));
	}

	uint64 CalcPropertyDescArrayHash(const TConstArrayView<FPropertyBagPropertyDesc> Descs)
	{
		uint64 Hash = 0;
		for (const FPropertyBagPropertyDesc& Desc : Descs)
		{
			Hash = CityHash128to64(Uint128_64(Hash, CalcPropertyDescHash(Desc)));
		}
		return Hash;
	}

	EPropertyBagContainerType GetContainerTypeFromProperty(const FProperty* InSourceProperty)
	{
		if (CastField<FArrayProperty>(InSourceProperty))
		{
			return EPropertyBagContainerType::Array;
		}
		
		return EPropertyBagContainerType::None;
	}

	EPropertyBagPropertyType GetValueTypeFromProperty(const FProperty* InSourceProperty)
	{
		if (CastField<FBoolProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Bool;
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InSourceProperty))
		{
			return ByteProperty->IsEnum() ? EPropertyBagPropertyType::Enum : EPropertyBagPropertyType::Byte;
		}
		if (CastField<FIntProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Int32;
		}
		if (CastField<FInt64Property>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Int64;
		}
		if (CastField<FFloatProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Float;
		}
		if (CastField<FDoubleProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Double;
		}
		if (CastField<FNameProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Name;
		}
		if (CastField<FStrProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::String;
		}
		if (CastField<FTextProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Text;
		}
		if (CastField<FEnumProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Enum;
		}
		if (CastField<FStructProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Struct;
		}
		if (CastField<FObjectProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Object;
		}
		if (CastField<FSoftObjectProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::SoftObject;
		}
		if (CastField<FClassProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Class;
		}
		if (CastField<FSoftClassProperty>(InSourceProperty))
		{
			return EPropertyBagPropertyType::SoftClass;
		}

		// Handle array property
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			return GetValueTypeFromProperty(ArrayProperty->Inner);	
		}

		return EPropertyBagPropertyType::None;
	}

	UObject* GetValueTypeObjectFromProperty(const FProperty* InSourceProperty)
	{
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InSourceProperty))
		{
			if (ByteProperty->IsEnum())
			{
				return ByteProperty->Enum;
			}
		}
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(InSourceProperty))
		{
			return EnumProp->GetEnum();
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(InSourceProperty))
		{
			return StructProperty->Struct;
		}
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InSourceProperty))
		{
			return ObjectProperty->PropertyClass;
		}
		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(InSourceProperty))
		{
			return SoftObjectProperty->PropertyClass;
		}
		if (const FClassProperty* ClassProperty = CastField<FClassProperty>(InSourceProperty))
		{
			return ClassProperty->PropertyClass;
		}
		if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(InSourceProperty))
		{
			return SoftClassProperty->PropertyClass;
		}
		
		// Handle array property
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			return GetValueTypeObjectFromProperty(ArrayProperty->Inner);
		}


		return nullptr;
	}

	FProperty* CreatePropertyFromDesc(const FPropertyBagPropertyDesc& Desc, const FFieldVariant PropertyScope)
	{
		// Handle array property
		if (Desc.ContainerType == EPropertyBagContainerType::Array)
		{
			FArrayProperty* Prop = new FArrayProperty(PropertyScope, Desc.Name, RF_Public);

			FPropertyBagPropertyDesc InnerDesc = Desc;
			InnerDesc.Name = FName(InnerDesc.Name.ToString() + TEXT("_inner"));
			InnerDesc.ContainerType = EPropertyBagContainerType::None;
			Prop->Inner = CreatePropertyFromDesc(InnerDesc, Prop);
				
			return Prop;
		}
		
		switch (Desc.ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			{
				FBoolProperty* Prop = new FBoolProperty(PropertyScope, Desc.Name, RF_Public);
				return Prop;
			}
		case EPropertyBagPropertyType::Byte:
			{
				FByteProperty* Prop = new FByteProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::Int32:
			{
				FIntProperty* Prop = new FIntProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::Int64:
			{
				FInt64Property* Prop = new FInt64Property(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::Float:
			{
				FFloatProperty* Prop = new FFloatProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::Double:
			{
				FDoubleProperty* Prop = new FDoubleProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::Name:
			{
				FNameProperty* Prop = new FNameProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::String:
			{
				FStrProperty* Prop = new FStrProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::Text:
			{
				FTextProperty* Prop = new FTextProperty(PropertyScope, Desc.Name, RF_Public);
				return Prop;
			}
		case EPropertyBagPropertyType::Enum:
			if (UEnum* Enum = Cast<UEnum>(Desc.ValueTypeObject))
			{
				FEnumProperty* Prop = new FEnumProperty(PropertyScope, Desc.Name, RF_Public);
				FNumericProperty* UnderlyingProp = new FByteProperty(Prop, "UnderlyingType", RF_Public); // HACK: Hardwire to byte property for now for BP compatibility
				Prop->SetEnum(Enum);
				Prop->AddCppProperty(UnderlyingProp);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::Struct:
			if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Desc.ValueTypeObject))
			{
				FStructProperty* Prop = new FStructProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->Struct = ScriptStruct;

				if (ScriptStruct->GetCppStructOps() && ScriptStruct->GetCppStructOps()->HasGetTypeHash())
				{
					Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				}
				
				if (ScriptStruct->StructFlags & STRUCT_HasInstancedReference)
				{
					Prop->SetPropertyFlags(CPF_ContainsInstancedReference);
				}
				
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::Object:
			if (UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FObjectProperty* Prop = new FObjectProperty(PropertyScope, Desc.Name, RF_Public);
				if (Class->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					Prop->SetPropertyFlags(CPF_InstancedReference);
				}
				Prop->SetPropertyClass(Class);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::SoftObject:
			if (UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FSoftObjectProperty* Prop = new FSoftObjectProperty(PropertyScope, Desc.Name, RF_Public);
				if (Class->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					Prop->SetPropertyFlags(CPF_InstancedReference);
				}
				Prop->SetPropertyClass(Class);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::Class:
			if (UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FClassProperty* Prop = new FClassProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetMetaClass(Class);
				Prop->PropertyClass = UClass::StaticClass();
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::SoftClass:
			if (UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FSoftClassProperty* Prop = new FSoftClassProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetMetaClass(Class);
				Prop->PropertyClass = UClass::StaticClass();
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		default:
			ensureMsgf(false, TEXT("Unhandled stype %s"), *UEnum::GetValueAsString(Desc.ValueType));
		}

		return nullptr;
	}

	// Helper functions to get and set property values

	//----------------------------------------------------------------//
	//  Getters
	//----------------------------------------------------------------//

	EPropertyBagResult GetPropertyAsInt64(const FPropertyBagPropertyDesc* Desc, const void* Address, int64& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		switch(Desc->ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			{
				const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address) ? 1 : 0;
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Byte:
			{
				const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int32:
			{
				const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Float:
			{
				const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
				OutValue = (int64)Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Double:
			{
				const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
				OutValue = (int64)Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Enum:
			{
				const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
				const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
				check(UnderlyingProperty);
				OutValue = UnderlyingProperty->GetSignedIntPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		default:
			return EPropertyBagResult::TypeMismatch;
		}
	}

	EPropertyBagResult GetPropertyAsDouble(const FPropertyBagPropertyDesc* Desc, const void* Address, double& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		switch(Desc->ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			{
				const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address) ? 1.0 : 0.0;
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Byte:
			{
				const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int32:
			{
				const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Float:
			{
				const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Double:
			{
				const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Enum:
			{
				const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
				const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
				check(UnderlyingProperty);
				OutValue = UnderlyingProperty->GetSignedIntPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		default:
			return EPropertyBagResult::TypeMismatch;
		}
	}

	// Generic property getter. Used for FName, FString, FText. 
	template<typename T, typename PropT>
	EPropertyBagResult GetPropertyValue(const FPropertyBagPropertyDesc* Desc, const void* Address, T& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
    	{
    		return EPropertyBagResult::PropertyNotFound;
    	}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		if (!Desc->CachedProperty->IsA<PropT>())
		{
			return EPropertyBagResult::TypeMismatch;
		}
		
		const PropT* Property = CastFieldChecked<PropT>(Desc->CachedProperty);
		OutValue = Property->GetPropertyValue(Address);

		return EPropertyBagResult::Success;
	}

	EPropertyBagResult GetPropertyValueAsEnum(const FPropertyBagPropertyDesc* Desc, const UEnum* RequestedEnum, const void* Address, uint8& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
    	{
    		return EPropertyBagResult::PropertyNotFound;
    	}
		if (Desc->ValueType != EPropertyBagPropertyType::Enum)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
		const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
		check(UnderlyingProperty);
	
		if (RequestedEnum != EnumProperty->GetEnum())
		{
			return EPropertyBagResult::TypeMismatch;
		}
	
		OutValue = (uint8)UnderlyingProperty->GetUnsignedIntPropertyValue(Address);

		return EPropertyBagResult::Success;
	}

	EPropertyBagResult GetPropertyValueAsStruct(const FPropertyBagPropertyDesc* Desc, const UScriptStruct* RequestedStruct, const void* Address, FStructView& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EPropertyBagPropertyType::Struct)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Desc->CachedProperty);
		check(StructProperty->Struct);

		if (RequestedStruct != nullptr && CanCastTo(StructProperty->Struct, RequestedStruct) == false)
		{
			return EPropertyBagResult::TypeMismatch;
		}
	
		OutValue = FStructView(StructProperty->Struct, (uint8*)Address);

		return EPropertyBagResult::Success;
	}

	EPropertyBagResult GetPropertyValueAsObject(const FPropertyBagPropertyDesc* Desc, const UClass* RequestedClass, const void* Address, UObject*& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EPropertyBagPropertyType::Object
			&& Desc->ValueType != EPropertyBagPropertyType::SoftObject
			&& Desc->ValueType != EPropertyBagPropertyType::Class
			&& Desc->ValueType != EPropertyBagPropertyType::SoftClass)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		const FObjectPropertyBase* ObjectProperty = CastFieldChecked<FObjectPropertyBase>(Desc->CachedProperty);
		check(ObjectProperty->PropertyClass);

		if (RequestedClass != nullptr && CanCastTo(ObjectProperty->PropertyClass, RequestedClass) == false)
		{
			return EPropertyBagResult::TypeMismatch;
		}
	
		OutValue = ObjectProperty->GetObjectPropertyValue(Address);

		return EPropertyBagResult::Success;
	}

	//----------------------------------------------------------------//
	//  Setters
	//----------------------------------------------------------------//

	EPropertyBagResult SetPropertyFromInt64(const FPropertyBagPropertyDesc* Desc, void* Address, const int64 InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		
		switch(Desc->ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			{
				const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, InValue != 0);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Byte:
			{
				const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, (uint8)InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int32:
			{
				const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, (int32)InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Float:
			{
				const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, (float)InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Double:
			{
				const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, (double)InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Enum:
			{
				const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
				const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
				check(UnderlyingProperty);
				UnderlyingProperty->SetIntPropertyValue(Address, (uint64)InValue);
				return EPropertyBagResult::Success;
			}
		default:
			return EPropertyBagResult::TypeMismatch;
		}
	}

	EPropertyBagResult SetPropertyFromDouble(const FPropertyBagPropertyDesc* Desc, void* Address, const double InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		switch(Desc->ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			{
				const FBoolProperty* Property = CastFieldChecked<FBoolProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, FMath::IsNearlyZero(InValue) ? false : true);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Byte:
			{
				const FByteProperty* Property = CastFieldChecked<FByteProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, FMath::RoundToInt32(InValue));
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int32:
			{
				const FIntProperty* Property = CastFieldChecked<FIntProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, FMath::RoundToInt32(InValue));
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, FMath::RoundToInt64(InValue));
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Float:
			{
				const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, (float)InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Double:
			{
				const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Enum:
			{
				const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
				const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
				check(UnderlyingProperty);
				UnderlyingProperty->SetIntPropertyValue(Address, (uint64)InValue);
				return EPropertyBagResult::Success;
			}
		default:
			return EPropertyBagResult::TypeMismatch;
		}
	}

	// Generic property setter. Used for FName, FString, FText 
	template<typename T, typename PropT>
	EPropertyBagResult SetPropertyValue(const FPropertyBagPropertyDesc* Desc, void* Address, const T& InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		if (!Desc->CachedProperty->IsA<PropT>())
		{
			return EPropertyBagResult::TypeMismatch;
		}
		
		const PropT* Property = CastFieldChecked<PropT>(Desc->CachedProperty);
		Property->SetPropertyValue(Address, InValue);

		return EPropertyBagResult::Success;
	}

	EPropertyBagResult SetPropertyValueAsEnum(const FPropertyBagPropertyDesc* Desc, void* Address, const uint8 InValue, const UEnum* Enum)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EPropertyBagPropertyType::Enum)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
		const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
		check(UnderlyingProperty);

		if (Enum != EnumProperty->GetEnum())
		{
			return EPropertyBagResult::TypeMismatch;
		}
	
		UnderlyingProperty->SetIntPropertyValue(Address, (uint64)InValue);
	
		return EPropertyBagResult::Success;
	}

	EPropertyBagResult SetPropertyValueAsStruct(const FPropertyBagPropertyDesc* Desc, void* Address, const FConstStructView InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EPropertyBagPropertyType::Struct)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Desc->CachedProperty);
		check(StructProperty->Struct);

		if (InValue.IsValid())
		{
			if (InValue.GetScriptStruct() != StructProperty->Struct)
			{
				return EPropertyBagResult::TypeMismatch;
			}
			StructProperty->Struct->CopyScriptStruct(Address, InValue.GetMemory());
		}
		else
		{
			StructProperty->Struct->ClearScriptStruct(Address);
		}

		return EPropertyBagResult::Success;
	}

	EPropertyBagResult SetPropertyValueAsObject(const FPropertyBagPropertyDesc* Desc, void* Address, UObject* InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EPropertyBagPropertyType::Object
			&& Desc->ValueType != EPropertyBagPropertyType::SoftObject
			&& Desc->ValueType != EPropertyBagPropertyType::Class
			&& Desc->ValueType != EPropertyBagPropertyType::SoftClass)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerType != EPropertyBagContainerType::None)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		
		const FObjectPropertyBase* ObjectProperty = CastFieldChecked<FObjectPropertyBase>(Desc->CachedProperty);
		check(ObjectProperty->PropertyClass);
		check(Desc->ValueTypeObject);

		if (Desc->ValueType == EPropertyBagPropertyType::Object
			|| Desc->ValueType == EPropertyBagPropertyType::SoftObject)
		{
			if (InValue && CanCastTo(InValue->GetClass(),  ObjectProperty->PropertyClass) == false)
			{
				return EPropertyBagResult::TypeMismatch;
			}
		}
		else
		{
			const UClass* Class = Cast<UClass>(InValue);
			const UClass* PropClass = nullptr;

			if (const FClassProperty* ClassProperty = CastFieldChecked<FClassProperty>(Desc->CachedProperty))
			{
				PropClass = ClassProperty->MetaClass;
			}
			else if (const FSoftClassProperty* SoftClassProperty = CastFieldChecked<FSoftClassProperty>(Desc->CachedProperty))
			{
				PropClass = SoftClassProperty->MetaClass;
			}
			
			if (!Class || !PropClass || !Class->IsChildOf(PropClass))
			{
				return EPropertyBagResult::TypeMismatch;
			}
		}

		ObjectProperty->SetObjectPropertyValue(Address, InValue);

		return EPropertyBagResult::Success;
	}

	void CopyMatchingValuesByID(const FConstStructView Source, const FStructView Target)
	{
		if (!Source.IsValid() || !Target.IsValid())
		{
			return;
		}

		const UPropertyBag* SourceBagStruct = Cast<const UPropertyBag>(Source.GetScriptStruct());
		const UPropertyBag* TargetBagStruct = Cast<const UPropertyBag>(Target.GetScriptStruct());

		if (!SourceBagStruct || !TargetBagStruct)
		{
			return;
		}

		// Iterate over source and copy to target if possible. Source is expected to usually have less items.
		for (const FPropertyBagPropertyDesc& SourceDesc : SourceBagStruct->GetPropertyDescs())
		{
			const FPropertyBagPropertyDesc* PotentialTargetDesc = TargetBagStruct->FindPropertyDescByID(SourceDesc.ID);
			if (PotentialTargetDesc == nullptr
				|| PotentialTargetDesc->CachedProperty == nullptr
				|| SourceDesc.CachedProperty == nullptr)
			{
				continue;
			}

			const FPropertyBagPropertyDesc& TargetDesc = *PotentialTargetDesc;
			void* TargetAddress = Target.GetMutableMemory() + TargetDesc.CachedProperty->GetOffset_ForInternal();
			const void* SourceAddress = Source.GetMemory() + SourceDesc.CachedProperty->GetOffset_ForInternal();
			
			if (TargetDesc.CompatibleType(SourceDesc))
			{
				TargetDesc.CachedProperty->CopyCompleteValue(TargetAddress, SourceAddress);
			}
			else if (TargetDesc.ContainerType == EPropertyBagContainerType::None
					&& SourceDesc.ContainerType == EPropertyBagContainerType::None)
			{
				if (TargetDesc.IsNumericType() && SourceDesc.IsNumericType())
				{
					// Try to convert numeric types.
					if (TargetDesc.IsNumericFloatType())
					{
						double Value = 0;
						if (GetPropertyAsDouble(&SourceDesc, SourceAddress, Value) == EPropertyBagResult::Success)
						{
							SetPropertyFromDouble(&TargetDesc, TargetAddress, Value);
						}
					}
					else
					{
						int64 Value = 0;
						if (GetPropertyAsInt64(&SourceDesc, SourceAddress, Value) == EPropertyBagResult::Success)
						{
							SetPropertyFromInt64(&TargetDesc, TargetAddress, Value);
						}
					}
				}
				else if ((TargetDesc.IsObjectType() && SourceDesc.IsObjectType())
					|| (TargetDesc.IsClassType() && SourceDesc.IsClassType()))
				{
					// Try convert between compatible objects and classes.
					const UClass* TargetObjectClass = Cast<const UClass>(TargetDesc.ValueTypeObject);
					const UClass* SourceObjectClass = Cast<const UClass>(SourceDesc.ValueTypeObject);
					if (CanCastTo(SourceObjectClass, TargetObjectClass))
					{
						const FObjectPropertyBase* TargetProp = CastFieldChecked<FObjectPropertyBase>(TargetDesc.CachedProperty);
						const FObjectPropertyBase* SourceProp = CastFieldChecked<FObjectPropertyBase>(SourceDesc.CachedProperty);
						TargetProp->SetObjectPropertyValue(TargetAddress, SourceProp->GetObjectPropertyValue(SourceAddress));
					}
				}
			}
		}
	}

	void RemovePropertyByName(TArray<FPropertyBagPropertyDesc>& Descs, const FName PropertyName, const int32 StartIndex = 0)
	{
		// Remove properties which dont have unique name.
		for (int32 Index = StartIndex; Index < Descs.Num(); Index++)
		{
			if (Descs[Index].Name == PropertyName)
			{
				Descs.RemoveAt(Index);
				Index--;
			}
		}
	}

}; // UE::StructUtils::Private


//----------------------------------------------------------------//
//  FPropertyBagPropertyDesc
//----------------------------------------------------------------//

void FPropertyBagPropertyDescMetaData::Serialize(FArchive& Ar)
{
	Ar << Key;
	Ar << Value;
}

FPropertyBagPropertyDesc::FPropertyBagPropertyDesc(const FName InName, const FProperty* InSourceProperty)
	: Name(InName)
{
	ValueType = UE::StructUtils::Private::GetValueTypeFromProperty(InSourceProperty);
	ValueTypeObject = UE::StructUtils::Private::GetValueTypeObjectFromProperty(InSourceProperty);
	ContainerType = UE::StructUtils::Private::GetContainerTypeFromProperty(InSourceProperty);

#if WITH_EDITORONLY_DATA
	if (const TMap<FName, FString>* SourcePropertyMetaData = InSourceProperty->GetMetaDataMap())
	{
		for (const TPair<FName, FString>& MetaDataPair : *SourcePropertyMetaData)
		{
			MetaData.Add({ MetaDataPair.Key, MetaDataPair.Value });
		}
	}
#endif
}

static FArchive& operator<<(FArchive& Ar, FPropertyBagPropertyDesc& Bag)
{
	Ar << Bag.ValueTypeObject;
	Ar << Bag.ID;
	Ar << Bag.Name;
	Ar << Bag.ValueType;

	if (Ar.CustomVer(FPropertyBagCustomVersion::GUID) >= FPropertyBagCustomVersion::ContainerTypes)
	{
		Ar << Bag.ContainerType;
	}

	bool bHasMetaData = false;
#if WITH_EDITORONLY_DATA
	if (Ar.IsSaving())
	{
		bHasMetaData = !Ar.IsCooking() && Bag.MetaData.Num() > 0;
	}
#endif
	Ar << bHasMetaData;

	if (bHasMetaData)
	{
#if WITH_EDITORONLY_DATA
		Ar << Bag.MetaData;
#else
		TArray<FPropertyBagPropertyDescMetaData> TempMetaData; 
		Ar << TempMetaData;
#endif
	}	
	
	return Ar;
}

bool FPropertyBagPropertyDesc::IsNumericType() const
{
	switch (ValueType)
	{
	case EPropertyBagPropertyType::Bool: return true;
	case EPropertyBagPropertyType::Byte: return true;
	case EPropertyBagPropertyType::Int32: return true;
	case EPropertyBagPropertyType::Int64: return true;
	case EPropertyBagPropertyType::Float: return true;
	case EPropertyBagPropertyType::Double: return true;
	default: return false;
	}
}

bool FPropertyBagPropertyDesc::IsNumericFloatType() const
{
	switch (ValueType)
	{
	case EPropertyBagPropertyType::Float: return true;
	case EPropertyBagPropertyType::Double: return true;
	default: return false;
	}
}

bool FPropertyBagPropertyDesc::IsObjectType() const
{
	switch (ValueType)
	{
	case EPropertyBagPropertyType::Object: return true;
	case EPropertyBagPropertyType::SoftObject: return true;
	default: return false;
	}
}

bool FPropertyBagPropertyDesc::IsClassType() const
{
	switch (ValueType)
	{
	case EPropertyBagPropertyType::Class: return true;
	case EPropertyBagPropertyType::SoftClass: return true;
	default: return false;
	}
}

bool FPropertyBagPropertyDesc::CompatibleType(const FPropertyBagPropertyDesc& Other) const
{
	// Containers must match
	if (ContainerType != Other.ContainerType)
	{
		return false;
	}

	// Values must match.
	if (ValueType != Other.ValueType)
	{
		return false;
	}

	// Struct and enum must have same value type class
	if (ValueType == EPropertyBagPropertyType::Enum || ValueType == EPropertyBagPropertyType::Struct)
	{
		return ValueTypeObject == Other.ValueTypeObject; 
	}

	// Objects should be castable.
	if (ValueType == EPropertyBagPropertyType::Object)
	{
		const UClass* ObjectClass = Cast<const UClass>(ValueTypeObject);
		const UClass* OtherObjectClass = Cast<const UClass>(Other.ValueTypeObject);
		return UE::StructUtils::Private::CanCastTo(OtherObjectClass, ObjectClass);
	}

	return true;
}

//----------------------------------------------------------------//
//  FInstancedPropertyBag
//----------------------------------------------------------------//

void FInstancedPropertyBag::InitializeFromBagStruct(const UPropertyBag* NewBagStruct)
{
	Value.InitializeAs(NewBagStruct);
}

void FInstancedPropertyBag::CopyMatchingValuesByID(const FInstancedPropertyBag& Other) const
{
	UE::StructUtils::Private::CopyMatchingValuesByID(Other.Value, Value);
}

int32 FInstancedPropertyBag::GetNumPropertiesInBag() const
{
	if (const UPropertyBag* BagStruct = GetPropertyBagStruct())
	{
		return BagStruct->PropertyDescs.Num();
	}

	return 0;
}

void FInstancedPropertyBag::AddProperties(const TConstArrayView<FPropertyBagPropertyDesc> NewDescs)
{
	TArray<FPropertyBagPropertyDesc> Descs;
	if (const UPropertyBag* CurrentBagStruct = GetPropertyBagStruct())
	{
		Descs = CurrentBagStruct->GetPropertyDescs();
	}

	for (const FPropertyBagPropertyDesc& NewDesc : NewDescs)
	{
		FPropertyBagPropertyDesc* ExistingProperty = Descs.FindByPredicate([&NewDesc](const FPropertyBagPropertyDesc& Desc) { return Desc.Name == NewDesc.Name; });
		if (ExistingProperty != nullptr)
		{
			ExistingProperty->ValueType = NewDesc.ValueType;
			ExistingProperty->ValueTypeObject = NewDesc.ValueTypeObject;
		}
		else
		{
			Descs.Add(NewDesc);
		}
	}

	const UPropertyBag* NewBagStruct = UPropertyBag::GetOrCreateFromDescs(Descs);
	MigrateToNewBagStruct(NewBagStruct);
}
	
void FInstancedPropertyBag::AddProperty(const FName InName, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject)
{
	AddProperties({ FPropertyBagPropertyDesc(InName, InValueType, InValueTypeObject) });
}

void FInstancedPropertyBag::AddContainerProperty(const FName InName, const EPropertyBagContainerType InContainerType, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject)
{
	AddProperties({ FPropertyBagPropertyDesc(InName, InContainerType, InValueType, InValueTypeObject) });
}

void FInstancedPropertyBag::AddProperty(const FName InName, const FProperty* InSourceProperty)
{
	AddProperties({ FPropertyBagPropertyDesc(InName, InSourceProperty ) });
}

void FInstancedPropertyBag::RemovePropertiesByName(const TConstArrayView<FName> PropertiesToRemove)
{
	TArray<FPropertyBagPropertyDesc> Descs;
	if (const UPropertyBag* CurrentBagStruct = GetPropertyBagStruct())
	{
		Descs = CurrentBagStruct->GetPropertyDescs();
	}

	for (const FName Name : PropertiesToRemove)
	{
		UE::StructUtils::Private::RemovePropertyByName(Descs, Name);
	}

	const UPropertyBag* NewBagStruct = UPropertyBag::GetOrCreateFromDescs(Descs);
	MigrateToNewBagStruct(NewBagStruct);
}
	
void FInstancedPropertyBag::RemovePropertyByName(const FName PropertyToRemove)
{
	RemovePropertiesByName({ PropertyToRemove });
}

void FInstancedPropertyBag::MigrateToNewBagStruct(const UPropertyBag* NewBagStruct)
{
	FInstancedStruct NewValue(NewBagStruct);

	UE::StructUtils::Private::CopyMatchingValuesByID(Value, NewValue);
	
	Value = MoveTemp(NewValue);
}

void FInstancedPropertyBag::MigrateToNewBagInstance(const FInstancedPropertyBag& NewBagInstance)
{
	FInstancedStruct NewValue(NewBagInstance.Value);

	UE::StructUtils::Private::CopyMatchingValuesByID(Value, NewValue);
	
	Value = MoveTemp(NewValue);
}

const UPropertyBag* FInstancedPropertyBag::GetPropertyBagStruct() const
{
	return Value.IsValid() ? Cast<const UPropertyBag>(Value.GetScriptStruct()) : nullptr;
}

const FPropertyBagPropertyDesc* FInstancedPropertyBag::FindPropertyDescByID(const FGuid ID) const
{
	if (const UPropertyBag* BagStruct = GetPropertyBagStruct())
	{
		return BagStruct->FindPropertyDescByID(ID);
	}
	return nullptr;
}
	
const FPropertyBagPropertyDesc* FInstancedPropertyBag::FindPropertyDescByName(const FName Name) const
{
	if (const UPropertyBag* BagStruct = GetPropertyBagStruct())
	{
		return BagStruct->FindPropertyDescByName(Name);
	}
	return nullptr;
}

void* FInstancedPropertyBag::GetValueAddress(const FPropertyBagPropertyDesc* Desc) const
{
	if (Desc == nullptr || !Value.IsValid())
	{
		return nullptr;
	}
	return Value.GetMutableMemory() + Desc->CachedProperty->GetOffset_ForInternal();
}

TValueOrError<bool, EPropertyBagResult> FInstancedPropertyBag::GetValueBool(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	int64 ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue != 0);
}

TValueOrError<uint8, EPropertyBagResult> FInstancedPropertyBag::GetValueByte(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	int64 ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((uint8)ReturnValue);
}

TValueOrError<int32, EPropertyBagResult> FInstancedPropertyBag::GetValueInt32(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	int64 ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((int32)ReturnValue);
}

TValueOrError<int64, EPropertyBagResult> FInstancedPropertyBag::GetValueInt64(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	int64 ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<float, EPropertyBagResult> FInstancedPropertyBag::GetValueFloat(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	double ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsDouble(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((float)ReturnValue);
}

TValueOrError<double, EPropertyBagResult> FInstancedPropertyBag::GetValueDouble(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	double ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsDouble(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<FName, EPropertyBagResult> FInstancedPropertyBag::GetValueName(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	FName ReturnValue;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValue<FName, FNameProperty>(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<FString, EPropertyBagResult> FInstancedPropertyBag::GetValueString(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	FString ReturnValue;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValue<FString, FStrProperty>(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<FText, EPropertyBagResult> FInstancedPropertyBag::GetValueText(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	FText ReturnValue;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValue<FText, FTextProperty>(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<uint8, EPropertyBagResult> FInstancedPropertyBag::GetValueEnum(const FName Name, const UEnum* RequestedEnum) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	uint8 ReturnValue;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsEnum(Desc, RequestedEnum, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<FStructView, EPropertyBagResult> FInstancedPropertyBag::GetValueStruct(const FName Name, const UScriptStruct* RequestedStruct) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	FStructView ReturnValue;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsStruct(Desc, RequestedStruct, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<UObject*, EPropertyBagResult> FInstancedPropertyBag::GetValueObject(const FName Name, const UClass* RequestedClass) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	UObject* ReturnValue = nullptr;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsObject(Desc, RequestedClass, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(ReturnValue);
}

TValueOrError<UClass*, EPropertyBagResult> FInstancedPropertyBag::GetValueClass(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	UObject* ReturnValue = nullptr;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsObject(Desc, nullptr, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	UClass* Class = Cast<UClass>(ReturnValue);
	if (Class == nullptr && ReturnValue != nullptr)
	{
		return MakeError(EPropertyBagResult::TypeMismatch);
	}
	return MakeValue(Class);
}


EPropertyBagResult FInstancedPropertyBag::SetValueBool(const FName Name, const bool bInValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromInt64(Desc, GetValueAddress(Desc), bInValue ? 1 : 0);
}

EPropertyBagResult FInstancedPropertyBag::SetValueByte(const FName Name, const uint8 InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromInt64(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueInt32(const FName Name, const int32 InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromInt64(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueInt64(const FName Name, const int64 InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromInt64(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueFloat(const FName Name, const float InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromDouble(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueDouble(const FName Name, const double InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromDouble(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueName(const FName Name, const FName InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValue<FName, FNameProperty>(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueString(const FName Name, const FString& InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValue<FString, FStrProperty>(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueText(const FName Name, const FText& InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValue<FText, FTextProperty>(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueEnum(const FName Name, const uint8 InValue, const UEnum* Enum) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsEnum(Desc, GetValueAddress(Desc), InValue, Enum);
}

EPropertyBagResult FInstancedPropertyBag::SetValueStruct(const FName Name, FConstStructView InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsStruct(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueObject(const FName Name, UObject* InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsObject(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueClass(const FName Name, UClass* InValue) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsObject(Desc, GetValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValue(const FName Name, const FProperty* InSourceProperty, const void* InSourceContainerAddress) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	if (Desc == nullptr || InSourceProperty == nullptr || InSourceContainerAddress == nullptr)
	{
		return EPropertyBagResult::PropertyNotFound;
	}
	check(Desc->CachedProperty);

	void* TargetAddress = Value.GetMutableMemory() + Desc->CachedProperty->GetOffset_ForInternal();
	void const* SourceAddress = InSourceProperty->ContainerPtrToValuePtr<void>(InSourceContainerAddress);

	if (InSourceProperty->GetClass() == Desc->CachedProperty->GetClass())
	{
		Desc->CachedProperty->CopyCompleteValue(TargetAddress, SourceAddress);
	}
	else
	{
		return EPropertyBagResult::TypeMismatch;
	}

	return EPropertyBagResult::Success;
}

TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> FInstancedPropertyBag::GetArrayRef(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	if (Desc == nullptr)
	{
		return MakeError(EPropertyBagResult::PropertyNotFound);
	}
	check(Desc->CachedProperty);

	if (Desc->ContainerType != EPropertyBagContainerType::Array)
	{
		return MakeError(EPropertyBagResult::TypeMismatch);
	}

	const void* Address = GetValueAddress(Desc);
	if (Address == nullptr)
	{
		return MakeError(EPropertyBagResult::PropertyNotFound);
	}
	
	return MakeValue(FPropertyBagArrayRef(*Desc, Address));
}

bool FInstancedPropertyBag::Serialize(FArchive& Ar)
{
	// Obsolete, use custom version instead.
	enum class EVersion : uint8
	{
		InitialVersion = 0,
		SerializeStructSize,
		// -----<new versions can be added above this line>-----
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
	EVersion Version = EVersion::LatestVersion;

	Ar.UsingCustomVersion(FPropertyBagCustomVersion::GUID);

	if (Ar.CustomVer(FPropertyBagCustomVersion::GUID) < FPropertyBagCustomVersion::ContainerTypes)
	{
		Ar << Version;
	}
	
	UPropertyBag* BagStruct = Cast<UPropertyBag>(const_cast<UScriptStruct*>(Value.GetScriptStruct()));
	bool bHasData = (BagStruct != nullptr);
	
	Ar << bHasData;
	
	if (bHasData)
	{
		// The script struct class is not serialized, the properties are serialized and type is created based on that.
		if (Ar.IsLoading())
		{
			TArray<FPropertyBagPropertyDesc> PropertyDescs;
			Ar << PropertyDescs;

			BagStruct = const_cast<UPropertyBag*>(UPropertyBag::GetOrCreateFromDescs(PropertyDescs));
			Value.InitializeAs(BagStruct);

			// Size of the serialized memory
			int32 SerialSize = 0; 
			if (Version >= EVersion::SerializeStructSize)
			{
				Ar << SerialSize;
			}
			
			// BagStruct can be null if it contains structs, classes or enums that could not be found.
			if (BagStruct != nullptr && Value.GetMutableMemory() != nullptr)
			{
				BagStruct->SerializeItem(Ar, Value.GetMutableMemory(), /*Defaults*/nullptr);
			}
			else
			{
				UE_LOG(LogCore, Warning, TEXT("Unable to create serialized UPropertyBag -> Advance %u bytes in the archive and reset to empty FInstancedPropertyBag"), SerialSize);
				Ar.Seek(Ar.Tell() + SerialSize);
			}
		}
		else if (Ar.IsSaving())
		{
			check(BagStruct);
			Ar << BagStruct->PropertyDescs;

			const int64 SizeOffset = Ar.Tell(); // Position to write the actual size after struct serialization
			int32 SerialSize = 0;
			// Size of the serialized memory (reserve location)
			Ar << SerialSize;

			const int64 InitialOffset = Ar.Tell(); // Position before struct serialization to compute its serial size

			check(Value.GetMutableMemory() != nullptr);
			BagStruct->SerializeItem(Ar, Value.GetMutableMemory(), /*Defaults*/nullptr);
		
			const int64 FinalOffset = Ar.Tell(); // Keep current offset to reset the archive pos after write the serial size

			// Size of the serialized memory
			Ar.Seek(SizeOffset);	// Go back in the archive to write the actual size
			SerialSize = (int32)(FinalOffset - InitialOffset);
			Ar << SerialSize;
			Ar.Seek(FinalOffset);	// Reset archive to its position
		}
	}
	
	return true;
}


//----------------------------------------------------------------//
//  FPropertyBagArrayRef
//----------------------------------------------------------------//

TValueOrError<bool, EPropertyBagResult> FPropertyBagArrayRef::GetValueBool(const int32 Index) const
{
	int64 IntValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(&ValueDesc, GetAddress(Index), IntValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(IntValue != 0);
}

TValueOrError<uint8, EPropertyBagResult> FPropertyBagArrayRef::GetValueByte(const int32 Index) const
{
	int64 IntValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(&ValueDesc, GetAddress(Index), IntValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((uint8)IntValue);
}

TValueOrError<int32, EPropertyBagResult> FPropertyBagArrayRef::GetValueInt32(const int32 Index) const
{
	int64 IntValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(&ValueDesc, GetAddress(Index), IntValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((int32)IntValue);
}

TValueOrError<int64, EPropertyBagResult> FPropertyBagArrayRef::GetValueInt64(const int32 Index) const
{
	int64 IntValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsInt64(&ValueDesc, GetAddress(Index), IntValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(IntValue);
}

TValueOrError<float, EPropertyBagResult> FPropertyBagArrayRef::GetValueFloat(const int32 Index) const
{
	double DblValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsDouble(&ValueDesc, GetAddress(Index), DblValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((float)DblValue);
}

TValueOrError<double, EPropertyBagResult> FPropertyBagArrayRef::GetValueDouble(const int32 Index) const
{
	double DblValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsDouble(&ValueDesc, GetAddress(Index), DblValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(DblValue);
}

TValueOrError<FName, EPropertyBagResult> FPropertyBagArrayRef::GetValueName(const int32 Index) const
{
	FName Value;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValue<FName, FNameProperty>(&ValueDesc, GetAddress(Index), Value);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(Value);
}

TValueOrError<FString, EPropertyBagResult> FPropertyBagArrayRef::GetValueString(const int32 Index) const
{
	FString Value;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValue<FString, FStrProperty>(&ValueDesc, GetAddress(Index), Value);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(Value);
}

TValueOrError<FText, EPropertyBagResult> FPropertyBagArrayRef::GetValueText(const int32 Index) const
{
	FText Value;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValue<FText, FTextProperty>(&ValueDesc, GetAddress(Index), Value);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(Value);
}

TValueOrError<uint8, EPropertyBagResult> FPropertyBagArrayRef::GetValueEnum(const int32 Index, const UEnum* RequestedEnum) const
{
	uint8 Value;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsEnum(&ValueDesc, RequestedEnum, GetAddress(Index), Value);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(Value);
}

TValueOrError<FStructView, EPropertyBagResult> FPropertyBagArrayRef::GetValueStruct(const int32 Index, const UScriptStruct* RequestedStruct) const
{
	FStructView Value;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsStruct(&ValueDesc, RequestedStruct, GetAddress(Index), Value);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(Value);
}

TValueOrError<UObject*, EPropertyBagResult> FPropertyBagArrayRef::GetValueObject(const int32 Index, const UClass* RequestedClass) const
{
	UObject* Value = nullptr;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsObject(&ValueDesc, RequestedClass, GetAddress(Index), Value);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(Value);
}

TValueOrError<UClass*, EPropertyBagResult> FPropertyBagArrayRef::GetValueClass(const int32 Index) const
{
	UObject* Value = nullptr;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsObject(&ValueDesc, nullptr, GetAddress(Index), Value);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	UClass* Class = Cast<UClass>(Value);
	if (Class == nullptr && Value != nullptr)
	{
		return MakeError(EPropertyBagResult::TypeMismatch);
	}
	return MakeValue(Class);
}


EPropertyBagResult FPropertyBagArrayRef::SetValueBool(const int32 Index, const bool bInValue) const
{
	return UE::StructUtils::Private::SetPropertyFromInt64(&ValueDesc, GetMutableAddress(Index), bInValue ? 1 : 0);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueByte(const int32 Index, const uint8 InValue) const
{
	return UE::StructUtils::Private::SetPropertyFromInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueInt32(const int32 Index, const int32 InValue) const
{
	return UE::StructUtils::Private::SetPropertyFromInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueInt64(const int32 Index, const int64 InValue) const
{
	return UE::StructUtils::Private::SetPropertyFromInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueFloat(const int32 Index, const float InValue) const
{
	return UE::StructUtils::Private::SetPropertyFromDouble(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueDouble(const int32 Index, const double InValue) const
{
	return UE::StructUtils::Private::SetPropertyFromDouble(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueName(const int32 Index, const FName InValue) const
{
	return UE::StructUtils::Private::SetPropertyValue<FName, FNameProperty>(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueString(const int32 Index, const FString& InValue) const
{
	return UE::StructUtils::Private::SetPropertyValue<FString, FStrProperty>(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueText(const int32 Index, const FText& InValue) const
{
	return UE::StructUtils::Private::SetPropertyValue<FText, FTextProperty>(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueEnum(const int32 Index, const uint8 InValue, const UEnum* Enum) const
{
	return UE::StructUtils::Private::SetPropertyValueAsEnum(&ValueDesc, GetMutableAddress(Index), InValue, Enum);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueStruct(const int32 Index, FConstStructView InValue) const
{
	return UE::StructUtils::Private::SetPropertyValueAsStruct(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueObject(const int32 Index, UObject* InValue) const
{
	return UE::StructUtils::Private::SetPropertyValueAsObject(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueClass(const int32 Index, UClass* InValue) const
{
	return UE::StructUtils::Private::SetPropertyValueAsObject(&ValueDesc, GetMutableAddress(Index), InValue);
}


//----------------------------------------------------------------//
//  UPropertyBag
//----------------------------------------------------------------//

const UPropertyBag* UPropertyBag::GetOrCreateFromDescs(const TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs)
{
	const uint64 BagHash = UE::StructUtils::Private::CalcPropertyDescArrayHash(PropertyDescs);
	const FString ScriptStructName = FString::Printf(TEXT("PropertyBag_%llx"), BagHash);

	if (const UPropertyBag* ExistingBag = FindObject<UPropertyBag>(GetTransientPackage(), *ScriptStructName))
	{
		return ExistingBag;
	}

	UPropertyBag* NewBag = NewObject<UPropertyBag>(GetTransientPackage(), *ScriptStructName, RF_Standalone | RF_Transient);

	NewBag->PropertyDescs = PropertyDescs;

	// Fix missing structs, enums, and objects.
	for (FPropertyBagPropertyDesc& Desc : NewBag->PropertyDescs)
	{
		if (Desc.ValueType == EPropertyBagPropertyType::Struct)
		{
			if (Desc.ValueTypeObject == nullptr || Desc.ValueTypeObject->GetClass()->IsChildOf(UScriptStruct::StaticClass()) == false)
			{
				UE_LOG(LogCore, Warning, TEXT("PropertyBag: Struct property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = FPropertyBagMissingStruct::StaticStruct();
			}
		}
		else if (Desc.ValueType == EPropertyBagPropertyType::Enum)
		{
			if (Desc.ValueTypeObject == nullptr || Desc.ValueTypeObject->GetClass()->IsChildOf(UEnum::StaticClass()) == false)
			{
				UE_LOG(LogCore, Warning, TEXT("PropertyBag: Enum property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = StaticEnum<EPropertyBagMissingEnum>();
			}
		}
		else if (Desc.ValueType == EPropertyBagPropertyType::Object || Desc.ValueType == EPropertyBagPropertyType::SoftObject)
		{
			if (Desc.ValueTypeObject == nullptr)
			{
				UE_LOG(LogCore, Warning, TEXT("PropertyBag: Object property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = UPropertyBagMissingObject::StaticClass();
			}
		}
		else if (Desc.ValueType == EPropertyBagPropertyType::Class || Desc.ValueType == EPropertyBagPropertyType::SoftClass)
		{
			if (Desc.ValueTypeObject == nullptr || Desc.ValueTypeObject->GetClass()->IsChildOf(UClass::StaticClass()) == false)
			{
				UE_LOG(LogCore, Warning, TEXT("PropertyBag: Class property '%s' is missing type."), *Desc.Name.ToString());
				Desc.ValueTypeObject = UPropertyBagMissingObject::StaticClass();
			}
		}
	}
	
	// Remove properties with same name
	for (int32 Index = 0; Index < NewBag->PropertyDescs.Num() - 1; Index++)
	{
		UE::StructUtils::Private::RemovePropertyByName(NewBag->PropertyDescs, NewBag->PropertyDescs[Index].Name, Index + 1);
	}
	
	// Add properties (AddCppProperty() adds them backwards in the linked list)
	for (int32 DescIndex = NewBag->PropertyDescs.Num() - 1; DescIndex >= 0; DescIndex--)
	{
		FPropertyBagPropertyDesc& Desc = NewBag->PropertyDescs[DescIndex];

		if (!Desc.ID.IsValid())
		{
			Desc.ID = FGuid::NewGuid();
		}


		if (FProperty* NewProperty = UE::StructUtils::Private::CreatePropertyFromDesc(Desc, NewBag))
		{
#if WITH_EDITORONLY_DATA
			// Add metadata
			for (const FPropertyBagPropertyDescMetaData& PropertyDescMetaData : Desc.MetaData)
			{
				NewProperty->SetMetaData(*PropertyDescMetaData.Key.ToString(), *PropertyDescMetaData.Value);
			}
#endif
			
			NewProperty->SetPropertyFlags(CPF_Edit);
			NewBag->AddCppProperty(NewProperty);
			Desc.CachedProperty = NewProperty; 
		}
	}

	NewBag->Bind();
	NewBag->StaticLink(/*RelinkExistingProperties*/true);

	return NewBag;
}

void UPropertyBag::InitializeStruct(void* Dest, int32 ArrayDim) const
{
	Super::InitializeStruct(Dest, ArrayDim);

	// Do ref counting based on struct usage.
	// This ensures that if the UPropertyBag is still valid in the C++ destructor of
	// the last instance of the bag.
	UPropertyBag* NonConstThis = const_cast<UPropertyBag*>(this);
	const int32 OldCount = NonConstThis->RefCount.fetch_add(1, std::memory_order_acq_rel);
	if (OldCount == 0)
	{
		NonConstThis->AddToRoot();
	}
}

void UPropertyBag::DestroyStruct(void* Dest, int32 ArrayDim) const
{
	Super::DestroyStruct(Dest, ArrayDim);

	// Do ref counting based on struct usage.
	// This ensures that if the UPropertyBag is still valid in the C++ destructor of
	// the last instance of the bag.

	UPropertyBag* NonConstThis = const_cast<UPropertyBag*>(this);
	const int32 OldCount = NonConstThis->RefCount.fetch_sub(1, std::memory_order_acq_rel);
	if (OldCount == 1)
	{
		NonConstThis->RemoveFromRoot();
	}
	if (OldCount <= 0)
	{
		UE_LOG(LogCore, Error, TEXT("PropertyBag: DestroyStruct is called when RefCount is %d."), OldCount);
	}
}

void UPropertyBag::FinishDestroy()
{
	const int32 Count = RefCount.load(); 
	if (Count > 0)
	{
		UE_LOG(LogCore, Error, TEXT("PropertyBag: Expecting RefCount to be zero on destructor, but it is %d."), Count);
	}
	
	Super::FinishDestroy();
}

const FPropertyBagPropertyDesc* UPropertyBag::FindPropertyDescByID(const FGuid ID) const
{
	return PropertyDescs.FindByPredicate([&ID](const FPropertyBagPropertyDesc& Desc) { return Desc.ID == ID; });
}

const FPropertyBagPropertyDesc* UPropertyBag::FindPropertyDescByName(const FName Name) const
{
	return PropertyDescs.FindByPredicate([&Name](const FPropertyBagPropertyDesc& Desc) { return Desc.Name == Name; });
}

