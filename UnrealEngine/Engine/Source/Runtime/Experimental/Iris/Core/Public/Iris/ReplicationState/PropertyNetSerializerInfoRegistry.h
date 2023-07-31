// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

namespace UE::Net
{

/**
 * Currently we require each supported type to register FPropertyNetSerializerInfo 
 * It provides information on what NetSerializer to use for which property and how to build the required NetSerializer config which is used when we build the dynamic descriptor
 * It is possible to register multiple FPropertyNetSerializerInfo for the same PropertyType-class as long as the IsSupportedFunction only matches a single Property, i.e. bool/nativebool enums of different sizes
 */
struct FPropertyNetSerializerInfo
{
	virtual const FFieldClass* GetPropertyTypeClass() const { return nullptr; }
	virtual bool IsSupported(const FProperty* Property) const { return true; }
	virtual const FNetSerializer* GetNetSerializer(const FProperty* Property) const { return nullptr; }
	virtual bool CanUseDefaultConfig(const FProperty* Property) const { return true; }
	virtual const FNetSerializerConfig* BuildNetSerializerConfig(void* NetSerializerConfigBuffer, const FProperty* Property) const { return nullptr; }
};

/**
 *  This is a simple static registry used by the ReplicationStateDescriptorBuiider when building descriptors using properties
 */
class FPropertyNetSerializerInfoRegistry
{
public:
	/** Register FPropertyNetSerializerInfo in the registry */
	IRISCORE_API static void Register(const FPropertyNetSerializerInfo* Info);
	IRISCORE_API static void Unregister(const FPropertyNetSerializerInfo* Info);

	/** Reset the registry */
	IRISCORE_API static void Reset();

	/** Sort entries in registry on property type */
	IRISCORE_API static void Freeze();

	/** Find the FPropertyNetSerializerInfo for the provided property */
	IRISCORE_API static const FPropertyNetSerializerInfo* FindSerializerInfo(const FProperty* Property);

	/** Find StructSerializerInfo by name for non property based serialization */
	IRISCORE_API static const FPropertyNetSerializerInfo* FindStructSerializerInfo(const FName Name);

	/**
	 * Get NopNetSerializerInfo. For when you want to use the NopNetSerializer. An example
	 * could be that you want to give a system access to some meta data that should not be
	 * replicated.
	 * 
	 * @see FNopNetSerializerConfig
	 */
	IRISCORE_API static const FPropertyNetSerializerInfo* GetNopNetSerializerInfo();

private:
	typedef TTuple<const FFieldClass*, const FPropertyNetSerializerInfo*> FRegistryEntry;
	typedef TArray<FRegistryEntry> FNetSerializerInfoRegistry;

	static bool bRegistryIsDirty;
	static FNetSerializerInfoRegistry Registry; 
};

/** 
 * Some helpers to register default infos for properties
*/

// Helper to implement simple default PropertyNetSerializerInfo for primitive types
template <typename T, typename ConfigType = FNetSerializerConfig>
struct TSimplePropertyNetSerializerInfo : public FPropertyNetSerializerInfo
{
	const FNetSerializer& Serializer;

	TSimplePropertyNetSerializerInfo(const FNetSerializer& InSerializer) : Serializer(InSerializer) {}
	virtual const FFieldClass* GetPropertyTypeClass() const override { return T::StaticClass(); }
	virtual const FNetSerializer* GetNetSerializer(const FProperty* Property) const override { return &Serializer; }
	virtual FNetSerializerConfig* BuildNetSerializerConfig(void* NetSerializerConfigBuffer, const FProperty* Property) const override { return new (NetSerializerConfigBuffer) ConfigType; }
};

// Helper to implement simple default PropetyNetSerializerInfo for structs
struct FNamedStructPropertyNetSerializerInfo : public TSimplePropertyNetSerializerInfo<FStructProperty>
{
	typedef TSimplePropertyNetSerializerInfo<FStructProperty> Super;
	const FName PropertyFName;

	FNamedStructPropertyNetSerializerInfo(const FName InPropertyFName, const FNetSerializer& InSerializer) : Super(InSerializer), PropertyFName(InPropertyFName) {}

	virtual bool IsSupported(const FProperty* Property) const override
	{	
		const FStructProperty* StructProp = CastFieldChecked<const FStructProperty>(Property);
		return StructProp->Struct->GetFName() == PropertyFName;
	};
};

struct FLastResortPropertyNetSerializerInfo : public FPropertyNetSerializerInfo
{		
	IRISCORE_API virtual const FFieldClass* GetPropertyTypeClass() const override;
	IRISCORE_API virtual bool IsSupported(const FProperty* Property) const override;
	IRISCORE_API virtual const FNetSerializer* GetNetSerializer(const FProperty* Property) const override final;
	IRISCORE_API virtual bool CanUseDefaultConfig(const FProperty* Property) const override final;
	IRISCORE_API virtual FNetSerializerConfig* BuildNetSerializerConfig(void* NetSerializerConfigBuffer, const FProperty* Property) const override final;
};

struct FNamedStructLastResortPropertyNetSerializerInfo final : public FLastResortPropertyNetSerializerInfo
{
	FNamedStructLastResortPropertyNetSerializerInfo(const FName InPropertyFName)
	: PropertyFName(InPropertyFName)
	{
	}

	IRISCORE_API virtual const FFieldClass* GetPropertyTypeClass() const override;
	IRISCORE_API virtual bool IsSupported(const FProperty* Property) const override;

private:
	const FName PropertyFName;
};

}

// Only needed if we want to export PropertyNetSerializerInfo, this goes in the header if we need to export it
#define UE_NET_DECLARE_NETSERIALIZER_INFO(NetSerializerInfo) \
const UE::Net::FPropertyNetSerializerInfo& GetPropertyNetSerializerInfo_##NetSerializerInfo();

// Implement FPropertyNetSerializerInfo from struct, this goes in the cpp file
#define UE_NET_IMPLEMENT_NETSERIALIZER_INFO(NetSerializerInfo) \
const UE::Net::FPropertyNetSerializerInfo& GetPropertyNetSerializerInfo_##NetSerializerInfo() { static NetSerializerInfo StaticInstance; return StaticInstance; };

// Implement simple FPropertyNetSerializerInfo binding a SerializerType to the property
#define UE_NET_IMPLEMENT_SIMPLE_NETSERIALIZER_INFO(PropertyType, SerializerName) \
const UE::Net::FPropertyNetSerializerInfo& GetPropertyNetSerializerInfo_##PropertyType() { static UE::Net::TSimplePropertyNetSerializerInfo<PropertyType> StaticInstance(UE_NET_GET_SERIALIZER(SerializerName)); return StaticInstance; };

// Implement simple FPropertyNetSerializerInfo for struct types with custom serializers
#define UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(Name, SerializerName) \
const UE::Net::FPropertyNetSerializerInfo& GetPropertyNetSerializerInfo_##Name() { static UE::Net::FNamedStructPropertyNetSerializerInfo StaticInstance(Name, UE_NET_GET_SERIALIZER(SerializerName)); return StaticInstance; };

// Implement FPropertyNetSerializerInfo for cases where LastResortPropertyNetSerializer is needed for struct with custom serialization.
#define UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_INFO(StructName) \
const UE::Net::FPropertyNetSerializerInfo& GetPropertyNetSerializerInfo_##StructName() { static UE::Net::FNamedStructLastResortPropertyNetSerializerInfo StaticInstance(StructName); return StaticInstance; };

// Register 
#define UE_NET_REGISTER_NETSERIALIZER_INFO(Name) \
UE::Net::FPropertyNetSerializerInfoRegistry::Register(&GetPropertyNetSerializerInfo_##Name());

// Unregister
#if !IS_MONOLITHIC
#define UE_NET_UNREGISTER_NETSERIALIZER_INFO(Name) \
UE::Net::FPropertyNetSerializerInfoRegistry::Unregister(&GetPropertyNetSerializerInfo_##Name());
#else
#define UE_NET_UNREGISTER_NETSERIALIZER_INFO(...) 
#endif

// Implement minimal required delegates for a NetSerializer
#define UE_NET_IMPLEMENT_NETSERIALZIER_REGISTRY_DELEGATES(Name) \
struct F##Name##NetSerializerRegistryDelegates : protected FNetSerializerRegistryDelegates \
{ \
	~F##Name##NetSerializerRegistryDelegates() { UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_##Name); } \
	virtual void OnPreFreezeNetSerializerRegistry() override { UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_##Name); } \
}; \
static F##Name##NetSerializerRegistryDelegates Name##NetSerializerRegistryDelegates;

// Utility that can be used to forward serialization of a Struct to a specific NetSerializer
#define UE_NET_IMPLEMENT_FORWARDING_NETSERIALIZER_AND_REGISTRY_DELEGATES(Name, SerializerName) \
	static const FName PropertyNetSerializerRegistry_NAME_##Name( PREPROCESSOR_TO_STRING(Name) ); \
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_##Name, FGameplayEffectContextNetSerializer); \
	UE_NET_IMPLEMENT_NETSERIALZIER_REGISTRY_DELEGATES(Name)
