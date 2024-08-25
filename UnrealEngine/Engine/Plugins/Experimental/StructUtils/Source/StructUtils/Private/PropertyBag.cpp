// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBag.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/TextProperty.h"

#if WITH_ENGINE && WITH_EDITOR
#include "Engine/UserDefinedStruct.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PropertyBag)

struct STRUCTUTILS_API FPropertyBagCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// Added support for array types
		ContainerTypes = 1,
		NestedContainerTypes = 2,
		MetaClass = 3,

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
		const uint32 Hashes[] = { GetTypeHash(Desc.ID), GetTypeHash(Desc.Name), GetTypeHash(Desc.ValueType), GetTypeHash(Desc.ContainerTypes), GetTypeHash(Desc.MetaData) };
#else
		const uint32 Hashes[] = { GetTypeHash(Desc.ID), GetTypeHash(Desc.Name), GetTypeHash(Desc.ValueType), GetTypeHash(Desc.ContainerTypes)};
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

	FPropertyBagContainerTypes GetContainerTypesFromProperty(const FProperty* InSourceProperty)
	{
		FPropertyBagContainerTypes ContainerTypes;

		while (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			if (ContainerTypes.Add(EPropertyBagContainerType::Array))
			{
				InSourceProperty = CastField<FArrayProperty>(ArrayProperty->Inner);
			}
			else // we reached the nested containers limit
			{
				ContainerTypes.Reset();
				break;
			}
		}

		return ContainerTypes;
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
		if (CastField<FUInt32Property>(InSourceProperty))
		{
			return EPropertyBagPropertyType::UInt32;
		}
		if (CastField<FInt64Property>(InSourceProperty))
		{
			return EPropertyBagPropertyType::Int64;
		}
		if (CastField<FUInt64Property>(InSourceProperty))
		{
			return EPropertyBagPropertyType::UInt64;
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
			if (CastField<FClassProperty>(InSourceProperty))
			{
				return EPropertyBagPropertyType::Class;
			}

			return EPropertyBagPropertyType::Object;
		}
		if (CastField<FSoftObjectProperty>(InSourceProperty))
		{
			if (CastField<FSoftClassProperty>(InSourceProperty))
			{
				return EPropertyBagPropertyType::SoftClass;
			}

			return EPropertyBagPropertyType::SoftObject;
		}

		// Handle array property
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			return GetValueTypeFromProperty(ArrayProperty->Inner);	
		}

		return EPropertyBagPropertyType::None;
	}

	const UObject* GetValueTypeObjectFromProperty(const FProperty* InSourceProperty)
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
			if (const FClassProperty* ClassProperty = CastField<FClassProperty>(InSourceProperty))
			{
				return ClassProperty->PropertyClass;
			}

			return ObjectProperty->PropertyClass;
		}
		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(InSourceProperty))
		{
			if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(InSourceProperty))
			{
				return SoftClassProperty->PropertyClass;
			}

			return SoftObjectProperty->PropertyClass;
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
		// Handle array and nested containers properties
		if (Desc.ContainerTypes.Num() > 0)
		{
			FProperty* Prop = nullptr; // the first created container will fill the return value, nested ones will fill the inner

			// support for nested containers, i.e. : TArray<TArray<float>>
			FFieldVariant PropertyOwner = PropertyScope;
			FProperty** ValuePropertyPtr = &Prop;
			FString ParentName;

			// Create the container list
			for (EPropertyBagContainerType BagContainerType : Desc.ContainerTypes)
			{
				switch(BagContainerType)
				{
				case EPropertyBagContainerType::Array:
					{
						// create an array property as a container for the tail
						const FString PropName = ParentName.IsEmpty() ? Desc.Name.ToString() : ParentName + TEXT("_InnerArray");
						FArrayProperty* ArrayProperty = new FArrayProperty(PropertyOwner, FName(PropName), RF_Public);
						*ValuePropertyPtr = ArrayProperty;
						ValuePropertyPtr = &ArrayProperty->Inner;
						PropertyOwner = ArrayProperty;
						ParentName = PropName;
						break;
					}
				default:
					ensureMsgf(false, TEXT("Unsuported container type %s"), *UEnum::GetValueAsString(BagContainerType));
					break;
				}
			}

			// finally create the tail type
			FPropertyBagPropertyDesc InnerDesc = Desc;
			InnerDesc.Name = FName(ParentName + TEXT("_InnerType"));
			InnerDesc.ContainerTypes.Reset();
			*ValuePropertyPtr = CreatePropertyFromDesc(InnerDesc, PropertyOwner);
				
			return Prop;
		}
		
		switch (Desc.ValueType)
		{
		case EPropertyBagPropertyType::Bool:
			{
				FBoolProperty* Prop = new FBoolProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->SetBoolSize(sizeof(bool), true); // Enable native access (init the whole byte, rather than just first bit)
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
		case EPropertyBagPropertyType::UInt32:
			{
				FUInt32Property* Prop = new FUInt32Property(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::Int64:
			{
				FInt64Property* Prop = new FInt64Property(PropertyScope, Desc.Name, RF_Public);
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
		case EPropertyBagPropertyType::UInt64:
			{
				FUInt64Property* Prop = new FUInt64Property(PropertyScope, Desc.Name, RF_Public);
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
			if (const UEnum* Enum = Cast<UEnum>(Desc.ValueTypeObject))
			{
				FEnumProperty* Prop = new FEnumProperty(PropertyScope, Desc.Name, RF_Public);
				FNumericProperty* UnderlyingProp = new FByteProperty(Prop, "UnderlyingType", RF_Public); // HACK: Hardwire to byte property for now for BP compatibility
				Prop->SetEnum(const_cast<UEnum*>(Enum));
				Prop->AddCppProperty(UnderlyingProp);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::Struct:
			if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Desc.ValueTypeObject))
			{
				FStructProperty* Prop = new FStructProperty(PropertyScope, Desc.Name, RF_Public);
				Prop->Struct = const_cast<UScriptStruct*>(ScriptStruct);

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
		case EPropertyBagPropertyType::Class:
			if (const UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FClassProperty* Prop = new FClassProperty(PropertyScope, Desc.Name, RF_Public);
#if WITH_EDITORONLY_DATA
				Prop->SetMetaClass(Desc.MetaClass ? Desc.MetaClass.Get() : const_cast<UClass*>(Class));
#else
				Prop->SetMetaClass(const_cast<UClass*>(Class));
#endif
				Prop->PropertyClass = UClass::StaticClass();
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::SoftClass:
			if (const UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FSoftClassProperty* Prop = new FSoftClassProperty(PropertyScope, Desc.Name, RF_Public);
#if WITH_EDITORONLY_DATA
				Prop->SetMetaClass(Desc.MetaClass ? Desc.MetaClass.Get() : const_cast<UClass*>(Class));
#else
				Prop->SetMetaClass(const_cast<UClass*>(Class));
#endif
				Prop->PropertyClass = UClass::StaticClass();
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::Object:
			if (const UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FObjectProperty* Prop = new FObjectProperty(PropertyScope, Desc.Name, RF_Public);
				if (Class->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					Prop->SetPropertyFlags(CPF_InstancedReference);
				}
				Prop->SetPropertyClass(const_cast<UClass*>(Class));
				Prop->SetPropertyFlags(CPF_HasGetValueTypeHash | CPF_TObjectPtr);
				return Prop;
			}
			break;
		case EPropertyBagPropertyType::SoftObject:
			if (const UClass* Class = Cast<UClass>(Desc.ValueTypeObject))
			{
				FSoftObjectProperty* Prop = new FSoftObjectProperty(PropertyScope, Desc.Name, RF_Public);
				if (Class->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					Prop->SetPropertyFlags(CPF_InstancedReference);
				}
				Prop->SetPropertyClass(const_cast<UClass*>(Class));
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
		if (Desc->ContainerTypes.Num() > 0)
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
		case EPropertyBagPropertyType::UInt32:
			{
				const FUInt32Property* Property = CastFieldChecked<FUInt32Property>(Desc->CachedProperty);
				OutValue = (uint32)Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::UInt64:
			{
				const FUInt64Property* Property = CastFieldChecked<FUInt64Property>(Desc->CachedProperty);
				OutValue = (uint64)Property->GetPropertyValue(Address);
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

	EPropertyBagResult GetPropertyAsUInt64(const FPropertyBagPropertyDesc* Desc, const void* Address, uint64& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		switch (Desc->ValueType)
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
			OutValue = (uint32)Property->GetPropertyValue(Address);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::UInt32:
		{
			const FUInt32Property* Property = CastFieldChecked<FUInt32Property>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::Int64:
		{
			const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
			OutValue = (uint64)Property->GetPropertyValue(Address);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::UInt64:
		{
			const FUInt64Property* Property = CastFieldChecked<FUInt64Property>(Desc->CachedProperty);
			OutValue = Property->GetPropertyValue(Address);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::Float:
		{
			const FFloatProperty* Property = CastFieldChecked<FFloatProperty>(Desc->CachedProperty);
			OutValue = (uint64)Property->GetPropertyValue(Address);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::Double:
		{
			const FDoubleProperty* Property = CastFieldChecked<FDoubleProperty>(Desc->CachedProperty);
			OutValue = (uint64)Property->GetPropertyValue(Address);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::Enum:
		{
			const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Desc->CachedProperty);
			const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			check(UnderlyingProperty);
			OutValue = UnderlyingProperty->GetUnsignedIntPropertyValue(Address);
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
		if (Desc->ContainerTypes.Num() > 0)
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
		case EPropertyBagPropertyType::UInt32:
			{
				const FUInt32Property* Property = CastFieldChecked<FUInt32Property>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				OutValue = Property->GetPropertyValue(Address);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::UInt64:
			{
				const FUInt64Property* Property = CastFieldChecked<FUInt64Property>(Desc->CachedProperty);
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
		if (Desc->ContainerTypes.Num() > 0)
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
		if (Desc->ContainerTypes.Num() > 0)
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
		if (Desc->ContainerTypes.Num() > 0)
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
		if (Desc->ContainerTypes.Num() > 0)
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

	EPropertyBagResult GetPropertyValueAsSoftPath(const FPropertyBagPropertyDesc* Desc, const void* Address, FSoftObjectPath& OutValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EPropertyBagPropertyType::SoftObject
			&& Desc->ValueType != EPropertyBagPropertyType::SoftClass)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		const FSoftObjectProperty* SoftObjectProperty = CastFieldChecked<FSoftObjectProperty>(Desc->CachedProperty);
		check(SoftObjectProperty->PropertyClass);

		OutValue = SoftObjectProperty->GetPropertyValue(Address).ToSoftObjectPath();

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
		if (Desc->ContainerTypes.Num() > 0)
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
		case EPropertyBagPropertyType::UInt32:
			{
				const FUInt32Property* Property = CastFieldChecked<FUInt32Property>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, (uint32)InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, InValue);
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::UInt64:
			{
				const FUInt64Property* Property = CastFieldChecked<FUInt64Property>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, (uint64)InValue);
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

	EPropertyBagResult SetPropertyFromUInt64(const FPropertyBagPropertyDesc* Desc, void* Address, const uint64 InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		switch (Desc->ValueType)
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
		case EPropertyBagPropertyType::UInt32:
		{
			const FUInt32Property* Property = CastFieldChecked<FUInt32Property>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, (uint32)InValue);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::Int64:
		{
			const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
			Property->SetPropertyValue(Address, (int64)InValue);
			return EPropertyBagResult::Success;
		}
		case EPropertyBagPropertyType::UInt64:
		{
			const FUInt64Property* Property = CastFieldChecked<FUInt64Property>(Desc->CachedProperty);
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
			UnderlyingProperty->SetIntPropertyValue(Address, InValue);
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
		if (Desc->ContainerTypes.Num() > 0)
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
		case EPropertyBagPropertyType::UInt32:
			{
				const FUInt32Property* Property = CastFieldChecked<FUInt32Property>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, FMath::RoundToInt32(InValue));
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::Int64:
			{
				const FInt64Property* Property = CastFieldChecked<FInt64Property>(Desc->CachedProperty);
				Property->SetPropertyValue(Address, FMath::RoundToInt64(InValue));
				return EPropertyBagResult::Success;
			}
		case EPropertyBagPropertyType::UInt64:
			{
				const FUInt64Property* Property = CastFieldChecked<FUInt64Property>(Desc->CachedProperty);
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
		if (Desc->ContainerTypes.Num() > 0)
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
		if (Desc->ContainerTypes.Num() > 0)
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
		if (Desc->ContainerTypes.Num() > 0)
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
		if (Desc->ContainerTypes.Num() > 0)
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

	EPropertyBagResult SetPropertyValueAsSoftPath(const FPropertyBagPropertyDesc* Desc, void* Address, const FSoftObjectPath& InValue)
	{
		if (Desc == nullptr || Desc->CachedProperty == nullptr)
		{
			return EPropertyBagResult::PropertyNotFound;
		}
		if (Desc->ValueType != EPropertyBagPropertyType::SoftObject &&
			Desc->ValueType != EPropertyBagPropertyType::SoftClass)
		{
			return EPropertyBagResult::TypeMismatch;
		}
		if (Address == nullptr)
		{
			return EPropertyBagResult::OutOfBounds;
		}
		if (Desc->ContainerTypes.Num() > 0)
		{
			return EPropertyBagResult::TypeMismatch;
		}

		const FSoftObjectProperty* SoftObjectProperty = CastFieldChecked<FSoftObjectProperty>(Desc->CachedProperty);
		check(SoftObjectProperty->PropertyClass);
		check(Desc->ValueTypeObject);

		SoftObjectProperty->SetPropertyValue(Address, FSoftObjectPtr(InValue));

		return EPropertyBagResult::Success;
	}

	/**
	 * Copies properties from Source to Target property bag. The bag layouts does not need to match.
	 * Properties are matched based on the ID in the property bag descs.
	 * If bHasOverrides == true, then only the matching properties, whose ID is found in Overrides are copied (if Overrides is empty, nothing is copied).
	 * If bHasOverrides == false, all matching properties are copied.
	 */
	void CopyMatchingValuesByID(const FConstStructView Source, FStructView Target, bool bHasOverrides, TConstArrayView<FGuid> Overrides)
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
			const bool bShouldCopy = !bHasOverrides || Overrides.Contains(SourceDesc.ID);
			if (!bShouldCopy)
			{
				continue;
			}
			
			const FPropertyBagPropertyDesc* PotentialTargetDesc = TargetBagStruct->FindPropertyDescByID(SourceDesc.ID);
			if (PotentialTargetDesc == nullptr
				|| PotentialTargetDesc->CachedProperty == nullptr
				|| SourceDesc.CachedProperty == nullptr)
			{
				continue;
			}

			const FPropertyBagPropertyDesc& TargetDesc = *PotentialTargetDesc;
			void* TargetAddress = Target.GetMemory() + TargetDesc.CachedProperty->GetOffset_ForInternal();
			const void* SourceAddress = Source.GetMemory() + SourceDesc.CachedProperty->GetOffset_ForInternal();
			
			if (TargetDesc.CompatibleType(SourceDesc))
			{
				TargetDesc.CachedProperty->CopyCompleteValue(TargetAddress, SourceAddress);
			}
			else if (TargetDesc.ContainerTypes.Num() == 0
					&& SourceDesc.ContainerTypes.Num() == 0)
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
						if (TargetDesc.IsUnsignedNumericType())
						{
							uint64 Value = 0;
							if (GetPropertyAsUInt64(&SourceDesc, SourceAddress, Value) == EPropertyBagResult::Success)
							{
								SetPropertyFromUInt64(&TargetDesc, TargetAddress, Value);
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

	void CopyMatchingValuesByID(const FConstStructView Source, FStructView Target)
	{
		CopyMatchingValuesByID(Source, Target, /*bHasOverrides*/false, {});
	}

	void CopyMatchingValuesByIDWithOverrides(const FConstStructView Source, FStructView Target, TConstArrayView<FGuid> Overrides)
	{
		CopyMatchingValuesByID(Source, Target, /*bHasOverrides*/true, Overrides);
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
//  FPropertyBagContainerTypes
//----------------------------------------------------------------//
EPropertyBagContainerType FPropertyBagContainerTypes::PopHead()
{
	EPropertyBagContainerType Head = EPropertyBagContainerType::None;
		
	if (NumContainers > 0)
	{
		Swap(Head, Types[0]);

		uint8 Index = NumContainers - 1;
		while (Index > 0)
		{
			Types[Index - 1] = Types[Index];
			Types[Index] = EPropertyBagContainerType::None;
			Index--;
		}
		NumContainers--;
	}

	return Head;
}

void FPropertyBagContainerTypes::Serialize(FArchive& Ar)
{
	Ar << NumContainers;
	for (int i = 0; i < NumContainers; ++i)
	{
		Ar << Types[i];
	}
}

bool FPropertyBagContainerTypes::operator == (const FPropertyBagContainerTypes& Other) const
{
	if (NumContainers != Other.NumContainers)
	{
		return false;
	}

	for (int i = 0; i < NumContainers; ++i)
	{
		if (Types[i] != Other.Types[i])
		{
			return false;
		}
	}

	return true;
}

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
	// @todo : improve error handling - if we reach the nested containers limit, the Desc will be invalid (empty container types)
	ContainerTypes = UE::StructUtils::Private::GetContainerTypesFromProperty(InSourceProperty);

#if WITH_EDITORONLY_DATA
	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(InSourceProperty))
	{
		MetaClass = ClassProperty->MetaClass;
	}

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
		if (Ar.IsLoading() && Ar.CustomVer(FPropertyBagCustomVersion::GUID) < FPropertyBagCustomVersion::NestedContainerTypes)
		{
			EPropertyBagContainerType TmpContainerType = EPropertyBagContainerType::None;
			Ar << TmpContainerType;

			if (TmpContainerType != EPropertyBagContainerType::None)
			{
				Bag.ContainerTypes.Add(TmpContainerType);
			}
		}
		else
		{
			Ar << Bag.ContainerTypes;
		}
	}

	bool bHasMetaData = false;
#if WITH_EDITORONLY_DATA
	if (Ar.IsSaving() && !Ar.IsCooking())
	{
		bHasMetaData = Bag.MetaData.Num() > 0 || Bag.MetaClass;
	}
#endif
	Ar << bHasMetaData;

	if (bHasMetaData)
	{
#if WITH_EDITORONLY_DATA
		Ar << Bag.MetaData;

		if (Ar.CustomVer(FPropertyBagCustomVersion::GUID) >= FPropertyBagCustomVersion::MetaClass)
		{
			Ar << Bag.MetaClass;
		}
#else
		TArray<FPropertyBagPropertyDescMetaData> TempMetaData; 
		Ar << TempMetaData;

		if (Ar.CustomVer(FPropertyBagCustomVersion::GUID) >= FPropertyBagCustomVersion::MetaClass)
		{
			TObjectPtr<UClass> TempMetaClass = nullptr;
			Ar << TempMetaClass;
		}
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
	case EPropertyBagPropertyType::UInt32: return true;
	case EPropertyBagPropertyType::Int64: return true;
	case EPropertyBagPropertyType::UInt64: return true;
	case EPropertyBagPropertyType::Float: return true;
	case EPropertyBagPropertyType::Double: return true;
	default: return false;
	}
}

bool FPropertyBagPropertyDesc::IsUnsignedNumericType() const
{
	switch (ValueType)
	{
	case EPropertyBagPropertyType::Byte: return true;
	case EPropertyBagPropertyType::UInt32: return true;
	case EPropertyBagPropertyType::UInt64: return true;
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
	if (ContainerTypes != Other.ContainerTypes)
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

void FInstancedPropertyBag::CopyMatchingValuesByID(const FInstancedPropertyBag& Other)
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

void FInstancedPropertyBag::AddContainerProperty(const FName InName, const FPropertyBagContainerTypes InContainerTypes, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject)
{
	AddProperties({ FPropertyBagPropertyDesc(InName, InContainerTypes, InValueType, InValueTypeObject) });
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

void FInstancedPropertyBag::MigrateToNewBagInstanceWithOverrides(const FInstancedPropertyBag& NewBagInstance, TConstArrayView<FGuid> OverriddenPropertyIDs)
{
	FInstancedStruct NewValue(NewBagInstance.Value);

	UE::StructUtils::Private::CopyMatchingValuesByIDWithOverrides(Value, NewValue, OverriddenPropertyIDs);
	
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

const void* FInstancedPropertyBag::GetValueAddress(const FPropertyBagPropertyDesc* Desc) const
{
	if (Desc == nullptr || !Value.IsValid())
	{
		return nullptr;
	}
	return Value.GetMemory() + Desc->CachedProperty->GetOffset_ForInternal();
}

void* FInstancedPropertyBag::GetMutableValueAddress(const FPropertyBagPropertyDesc* Desc)
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
	uint64 ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsUInt64(Desc, GetValueAddress(Desc), ReturnValue);
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

TValueOrError<uint32, EPropertyBagResult> FInstancedPropertyBag::GetValueUInt32(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	uint64 ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsUInt64(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((uint32)ReturnValue);
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

TValueOrError<uint64, EPropertyBagResult> FInstancedPropertyBag::GetValueUInt64(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	uint64 ReturnValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsUInt64(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((uint64)ReturnValue);
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

TValueOrError<FSoftObjectPath, EPropertyBagResult> FInstancedPropertyBag::GetValueSoftPath(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	FSoftObjectPath ReturnValue;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsSoftPath(Desc, GetValueAddress(Desc), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}

	return MakeValue(std::move(ReturnValue));
}

TValueOrError<FString, EPropertyBagResult> FInstancedPropertyBag::GetValueSerializedString(const FName Name)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	if (Desc == nullptr)
	{
		return MakeError(EPropertyBagResult::PropertyNotFound);
	}
	
	const FProperty* Property = Desc->CachedProperty;
	check(Property);

	const void* ValueAddress = GetValueAddress(Desc);
	FString OutStringValue;
	if (!Property->ExportText_Direct(OutStringValue, ValueAddress, ValueAddress, nullptr, PPF_None))
	{
		UE_LOG(LogCore, Warning, TEXT("PropertyBag: Getting the serialized value of the property '%s' failed."), *Desc->Name.ToString());
		return MakeError(EPropertyBagResult::TypeMismatch);
	}

	return MakeValue(OutStringValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueBool(const FName Name, const bool bInValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromInt64(Desc, GetMutableValueAddress(Desc), bInValue ? 1 : 0);
}

EPropertyBagResult FInstancedPropertyBag::SetValueByte(const FName Name, const uint8 InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromUInt64(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueInt32(const FName Name, const int32 InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromInt64(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueUInt32(const FName Name, const uint32 InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromUInt64(Desc, GetMutableValueAddress(Desc), (int64)InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueInt64(const FName Name, const int64 InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromInt64(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueUInt64(const FName Name, const uint64 InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromUInt64(Desc, GetMutableValueAddress(Desc), (int64)InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueFloat(const FName Name, const float InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromDouble(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueDouble(const FName Name, const double InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyFromDouble(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueName(const FName Name, const FName InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValue<FName, FNameProperty>(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueString(const FName Name, const FString& InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValue<FString, FStrProperty>(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueText(const FName Name, const FText& InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValue<FText, FTextProperty>(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueEnum(const FName Name, const uint8 InValue, const UEnum* Enum)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsEnum(Desc, GetMutableValueAddress(Desc), InValue, Enum);
}

EPropertyBagResult FInstancedPropertyBag::SetValueStruct(const FName Name, FConstStructView InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsStruct(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueObject(const FName Name, UObject* InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsObject(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueClass(const FName Name, UClass* InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsObject(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueSoftPath(const FName Name, const FSoftObjectPath& InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	return UE::StructUtils::Private::SetPropertyValueAsSoftPath(Desc, GetMutableValueAddress(Desc), InValue);
}

EPropertyBagResult FInstancedPropertyBag::SetValueSerializedString(const FName Name, const FString& InValue)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	if (Desc == nullptr)
	{
		return EPropertyBagResult::PropertyNotFound;
	}
	
	const FProperty* Property = Desc->CachedProperty;
	check(Property);

	if (!Property->ImportText_Direct(*InValue, GetMutableValueAddress(Desc), nullptr, PPF_None))
	{
		UE_LOG(LogCore, Warning, TEXT("PropertyBag: Setting the value of the property '%s' failed because the string representation provided was not accepted."), *Desc->Name.ToString());
		return EPropertyBagResult::TypeMismatch;
	}

	return EPropertyBagResult::Success;
}

EPropertyBagResult FInstancedPropertyBag::SetValue(const FName Name, const FProperty* InSourceProperty, const void* InSourceContainerAddress)
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

TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> FInstancedPropertyBag::GetMutableArrayRef(const FName Name)
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	if (Desc == nullptr)
	{
		return MakeError(EPropertyBagResult::PropertyNotFound);
	}
	check(Desc->CachedProperty);

	if (Desc->ContainerTypes.GetFirstContainerType() != EPropertyBagContainerType::Array)
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

TValueOrError<const FPropertyBagArrayRef, EPropertyBagResult> FInstancedPropertyBag::GetArrayRef(const FName Name) const
{
	const FPropertyBagPropertyDesc* Desc = FindPropertyDescByName(Name);
	if (Desc == nullptr)
	{
		return MakeError(EPropertyBagResult::PropertyNotFound);
	}
	check(Desc->CachedProperty);

	if (Desc->ContainerTypes.GetFirstContainerType() != EPropertyBagContainerType::Array)
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

bool FInstancedPropertyBag::Identical(const FInstancedPropertyBag* Other, const uint32 PortFlags) const
{
	return Other && Value.Identical(&Other->Value, PortFlags);
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

			for (FPropertyBagPropertyDesc& PropDesc : PropertyDescs)
			{
				if (PropDesc.ValueTypeObject)
				{
					Ar.Preload(const_cast<UObject*>(PropDesc.ValueTypeObject.Get()));
				}
			}
			
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
			
			TArray<FPropertyBagPropertyDesc> PropertyDescs = BagStruct->PropertyDescs;
#if WITH_ENGINE && WITH_EDITOR
			// Save primary struct for user defined struct properties.
			// This is used as part of the user defined struct reinstancing logic.
			for (FPropertyBagPropertyDesc& Desc : PropertyDescs)
			{
				if (Desc.ValueType == EPropertyBagPropertyType::Struct)
				{
					const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(Desc.ValueTypeObject);
					if (UserDefinedStruct
						&& UserDefinedStruct->Status == EUserDefinedStructureStatus::UDSS_Duplicate
						&& UserDefinedStruct->PrimaryStruct.IsValid())
					{
						Desc.ValueTypeObject = UserDefinedStruct->PrimaryStruct.Get();
					}
				}
			}
#endif			
			
			Ar << PropertyDescs;

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
	else
	{
		if (Ar.IsLoading())
		{
			// If loading and there was no data saved in the archive, make sure the value is empty.
			Reset();
		}
	}
	
	return true;
}

void FInstancedPropertyBag::AddStructReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_ENGINE && WITH_EDITOR
	// Reference collector is used to visit all instances of instanced structs the the like when a user defined struct is reinstanced.
	if (const UUserDefinedStruct* StructureToReinstance = UE::StructUtils::Private::GetStructureToReinstance())
	{
		const UPropertyBag* Bag = GetPropertyBagStruct();
		if (Bag && Bag->ContainsUserDefinedStruct(StructureToReinstance))
		{
			if (StructureToReinstance->Status == EUserDefinedStructureStatus::UDSS_Duplicate)
			{
				// On the first pass we create a new bag that contains copy of UDS that represents the currently allocated struct.
				// GStructureToReinstance is the duplicated struct, and GStructureToReinstance->PrimaryStruct is the UDS that is being reinstanced.

				TArray<FPropertyBagPropertyDesc> PropertyDescs = Bag->PropertyDescs;
				for (FPropertyBagPropertyDesc& Desc : PropertyDescs)
				{
					if (Desc.ValueType == EPropertyBagPropertyType::Struct)
					{
						if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(Desc.ValueTypeObject))
						{
							if (UserDefinedStruct == StructureToReinstance->PrimaryStruct)
							{
								Desc.ValueTypeObject = StructureToReinstance;
							}
						}					
					}
				}

				const UPropertyBag* NewBag = UPropertyBag::GetOrCreateFromDescs(PropertyDescs);
				Value.ReplaceScriptStructInternal(NewBag);

				// Adjust recount manually, since we replaced the struct above.
				Bag->DecrementRefCount();
				NewBag->IncrementRefCount();
			}
			else
			{
				// On the second pass we reinstantiate the data using serialization.
				// When saving, the UDSs are written using the duplicate which represents current layout, but PrimaryStruct is serialized as the type.
				// When reading, the data is initialized with the new type, and the serialization will take care of reading from the old data.

				if (UObject* Outer = UE::StructUtils::Private::GetCurrentReinstanceOuterObject())
				{
					if (!Outer->IsA<UClass>() && !Outer->HasAnyFlags(RF_ClassDefaultObject))
					{
						Outer->MarkPackageDirty();
					}
				}
				
				TArray<uint8> Data;

				FMemoryWriter Writer(Data);
				FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/true);
				Serialize(WriterProxy);
				
				FMemoryReader Reader(Data);
				FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);
				Serialize(ReaderProxy);
			}
		}
	}
#endif	
}

void FInstancedPropertyBag::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	if (const UPropertyBag* BagStruct = GetPropertyBagStruct())
	{
		for (const FPropertyBagPropertyDesc& Desc : BagStruct->PropertyDescs)
		{
			if (Desc.ValueTypeObject)
			{
				OutDeps.Add(const_cast<UObject*>(Desc.ValueTypeObject.Get()));
			}
		}

		// Report indirect dependencies of the instanced property bag struct
		// The iterator will recursively loop through all structs in structs/containers too
		for (TPropertyValueIterator<FStructProperty> It(BagStruct, Value.GetMutableMemory()); It; ++It)
		{
			const UScriptStruct* StructType = It.Key()->Struct;
			if (UScriptStruct::ICppStructOps* CppStructOps = StructType->GetCppStructOps())
			{
				void* StructDataPtr = const_cast<void*>(It.Value());
				CppStructOps->GetPreloadDependencies(StructDataPtr, OutDeps);
			}
		}
	}
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
	uint64 IntValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsUInt64(&ValueDesc, GetAddress(Index), IntValue);
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

TValueOrError<uint32, EPropertyBagResult> FPropertyBagArrayRef::GetValueUInt32(const int32 Index) const
{
	uint64 IntValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsUInt64(&ValueDesc, GetAddress(Index), IntValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((uint32)IntValue);
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

TValueOrError<uint64, EPropertyBagResult> FPropertyBagArrayRef::GetValueUInt64(const int32 Index) const
{
	uint64 IntValue = 0;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyAsUInt64(&ValueDesc, GetAddress(Index), IntValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue((uint64)IntValue);
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

TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> FPropertyBagArrayRef::GetMutableNestedArrayRef(const int32 Index) const
{
	if (ValueDesc.ContainerTypes.GetFirstContainerType() != EPropertyBagContainerType::Array)
	{
		return MakeError(EPropertyBagResult::TypeMismatch);
	}

	check(ValueDesc.CachedProperty);

	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ValueDesc.CachedProperty);

	// Get the array address
	const void* Address = GetAddress(Index);
	if (Address == nullptr)
	{
		return MakeError(EPropertyBagResult::PropertyNotFound);
	}
	
	// And create a FPropertyBagArrayRef with the dummy desc and the element address
	return MakeValue(FPropertyBagArrayRef(ValueDesc, Address));
}

TValueOrError<const FPropertyBagArrayRef, EPropertyBagResult> FPropertyBagArrayRef::GetNestedArrayRef(const int32 Index) const
{
	if (ValueDesc.ContainerTypes.GetFirstContainerType() != EPropertyBagContainerType::Array)
	{
		return MakeError(EPropertyBagResult::TypeMismatch);
	}

	check(ValueDesc.CachedProperty);

	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ValueDesc.CachedProperty);

	// Get the array address
	const void* Address = GetAddress(Index);
	if (Address == nullptr)
	{
		return MakeError(EPropertyBagResult::PropertyNotFound);
	}
	
	// And create a FPropertyBagArrayRef with the dummy desc and the element address
	return MakeValue(FPropertyBagArrayRef(ValueDesc, Address));
}

TValueOrError<FSoftObjectPath, EPropertyBagResult> FPropertyBagArrayRef::GetValueSoftPath(const int32 Index) const
{
	FSoftObjectPath ReturnValue;
	const EPropertyBagResult Result = UE::StructUtils::Private::GetPropertyValueAsSoftPath(&ValueDesc, GetAddress(Index), ReturnValue);
	if (Result != EPropertyBagResult::Success)
	{
		return MakeError(Result);
	}
	return MakeValue(std::move(ReturnValue));
}

EPropertyBagResult FPropertyBagArrayRef::SetValueBool(const int32 Index, const bool bInValue)
{
	return UE::StructUtils::Private::SetPropertyFromInt64(&ValueDesc, GetMutableAddress(Index), bInValue ? 1 : 0);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueByte(const int32 Index, const uint8 InValue)
{
	return UE::StructUtils::Private::SetPropertyFromUInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueInt32(const int32 Index, const int32 InValue)
{
	return UE::StructUtils::Private::SetPropertyFromInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueUInt32(const int32 Index, const uint32 InValue)
{
	return UE::StructUtils::Private::SetPropertyFromUInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueInt64(const int32 Index, const int64 InValue)
{
	return UE::StructUtils::Private::SetPropertyFromInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueUInt64(const int32 Index, const uint64 InValue)
{
	return UE::StructUtils::Private::SetPropertyFromUInt64(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueFloat(const int32 Index, const float InValue)
{
	return UE::StructUtils::Private::SetPropertyFromDouble(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueDouble(const int32 Index, const double InValue)
{
	return UE::StructUtils::Private::SetPropertyFromDouble(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueName(const int32 Index, const FName InValue)
{
	return UE::StructUtils::Private::SetPropertyValue<FName, FNameProperty>(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueString(const int32 Index, const FString& InValue)
{
	return UE::StructUtils::Private::SetPropertyValue<FString, FStrProperty>(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueText(const int32 Index, const FText& InValue)
{
	return UE::StructUtils::Private::SetPropertyValue<FText, FTextProperty>(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueEnum(const int32 Index, const uint8 InValue, const UEnum* Enum)
{
	return UE::StructUtils::Private::SetPropertyValueAsEnum(&ValueDesc, GetMutableAddress(Index), InValue, Enum);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueStruct(const int32 Index, FConstStructView InValue)
{
	return UE::StructUtils::Private::SetPropertyValueAsStruct(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueObject(const int32 Index, UObject* InValue)
{
	return UE::StructUtils::Private::SetPropertyValueAsObject(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueClass(const int32 Index, UClass* InValue)
{
	return UE::StructUtils::Private::SetPropertyValueAsObject(&ValueDesc, GetMutableAddress(Index), InValue);
}

EPropertyBagResult FPropertyBagArrayRef::SetValueSoftPath(const int32 Index, const FSoftObjectPath& InValue)
{
	return UE::StructUtils::Private::SetPropertyValueAsSoftPath(&ValueDesc, GetMutableAddress(Index), InValue);
}

//----------------------------------------------------------------//
//  UPropertyBag
//----------------------------------------------------------------//

namespace UE::StructUtils::Private
{
	// Lock to prevent concurrent access to lazily-constructed UPropertyBag objects in UPropertyBag::GetOrCreateFromDescs
	static FCriticalSection GPropertyBagLock;
}

const UPropertyBag* UPropertyBag::GetOrCreateFromDescs(const TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs, const TCHAR* PrefixName)
{
	const uint64 BagHash = UE::StructUtils::Private::CalcPropertyDescArrayHash(PropertyDescs);
	const FString ScriptStructName = PrefixName == nullptr
		? FString::Printf(TEXT("PropertyBag_%llx"), BagHash)
		: FString::Printf(TEXT("%s_%llx"), PrefixName, BagHash);

	// We need to linearize this entire function otherwise threads that create bags of identical layouts can view
	// partially-constructed objects 
	FScopeLock ScopeLock(&UE::StructUtils::Private::GPropertyBagLock);

	// we need to use StaticFindObjectFastInternal with ExclusiveInternalFlags = EInternalObjectFlags::None
	// here because objects with RF_NeedPostLoad cannot be found by regular FindObject calls as they will have
	// ExclusiveInternalFlags = EInternalObjectFlags::AsyncLoading when called from game thread.
	if (UObject* ExistingObject = StaticFindObjectFastInternal(UPropertyBag::StaticClass(), GetTransientPackage(), *ScriptStructName, true, RF_NoFlags, EInternalObjectFlags::None))
	{
		if (const UPropertyBag* ExistingBag = Cast<UPropertyBag>(ExistingObject))
		{
			return ExistingBag;
		}
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

	// @hack:
	// This method is called to prevent non-editor builds to not crash on IsChildOf().
	// The issues is that the UScriptStruct(const FObjectInitializer& ObjectInitializer) ctor (which is macro/UHT generated)
	// does not call ReinitializeBaseChainArray(), when the code is compiled with USTRUCT_ISCHILDOF_STRUCTARRAY.
	// Calling SetSuperStruct() forces the ReinitializeBaseChainArray() to be called.
	NewBag->SetSuperStruct(nullptr);
	
	NewBag->Bind();
	NewBag->StaticLink(/*RelinkExistingProperties*/true);

	return NewBag;
}

#if WITH_ENGINE && WITH_EDITOR
bool UPropertyBag::ContainsUserDefinedStruct(const UUserDefinedStruct* UserDefinedStruct) const
{
	if (!UserDefinedStruct)
	{
		return false;
	}
	
	for (const FPropertyBagPropertyDesc& Desc : PropertyDescs)
	{
		if (Desc.ValueType == EPropertyBagPropertyType::Struct)
		{
			if (const UUserDefinedStruct* OwnedUserDefinedStruct = Cast<UUserDefinedStruct>(Desc.ValueTypeObject))
			{
				if (OwnedUserDefinedStruct == UserDefinedStruct
					|| OwnedUserDefinedStruct->PrimaryStruct == UserDefinedStruct
					|| OwnedUserDefinedStruct == UserDefinedStruct->PrimaryStruct
					|| OwnedUserDefinedStruct->PrimaryStruct == UserDefinedStruct->PrimaryStruct)
				{
					return true;
				}
			}
		}
	}
	return false;
}
#endif

void UPropertyBag::DecrementRefCount() const
{
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

void UPropertyBag::IncrementRefCount() const
{
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

void UPropertyBag::InitializeStruct(void* Dest, int32 ArrayDim) const
{
	Super::InitializeStruct(Dest, ArrayDim);

	IncrementRefCount();
}

void UPropertyBag::DestroyStruct(void* Dest, int32 ArrayDim) const
{
	Super::DestroyStruct(Dest, ArrayDim);

	DecrementRefCount();
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

const FPropertyBagPropertyDesc* UPropertyBag::FindPropertyDescByPropertyName(const FName PropertyName) const
{
	return PropertyDescs.FindByPredicate([&PropertyName](const FPropertyBagPropertyDesc& Desc) { return Desc.CachedProperty && Desc.CachedProperty->GetFName() == PropertyName; });
}

const FPropertyBagPropertyDesc* UPropertyBag::FindPropertyDescByProperty(const FProperty* Property) const
{
	return PropertyDescs.FindByPredicate([&Property](const FPropertyBagPropertyDesc& Desc) { return Desc.CachedProperty && Desc.CachedProperty == Property; });
}
