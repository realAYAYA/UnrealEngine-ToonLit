// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPCGAttributeAccessorTpl.h"

#include "Containers/UnrealString.h" // IWYU pragma: keep
#include "UObject/SoftObjectPath.h" // IWYU pragma: keep
#include "UObject/UnrealType.h" // IWYU pragma: keep

class FEnumProperty;

namespace PCGPropertyAccessor
{
	TArray<const void*> GetContainerKeys(int32 Index, int32 Range, const IPCGAttributeAccessorKeys& Keys);
	TArray<void*> GetContainerKeys(int32 Index, int32 Range, IPCGAttributeAccessorKeys& Keys);

	template <typename T, typename Func>
	bool IterateGet(const FProperty* Property, TArrayView<T>& OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys, Func&& Getter)
	{
		TArray<const void*> ContainerKeys = GetContainerKeys(Index, OutValues.Num(), Keys);
		if (ContainerKeys.IsEmpty())
		{
			return false;
		}

		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			const void* PropertyAddressData = Property->ContainerPtrToValuePtr<void>(ContainerKeys[i]);
			OutValues[i] = Getter(PropertyAddressData);
		}

		return true;
	}

	template <typename T, typename Func>
	bool IterateSet(const FProperty* Property, TArrayView<const T>& InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, Func&& Setter)
	{
		TArray<void*> ContainerKeys = GetContainerKeys(Index, InValues.Num(), Keys);
		if (ContainerKeys.IsEmpty())
		{
			return false;
		}

		for (int32 i = 0; i < InValues.Num(); ++i)
		{
			void* PropertyAddressData = Property->ContainerPtrToValuePtr<void>(ContainerKeys[i]);
			Setter(PropertyAddressData, InValues[i]);
		}

		return true;
	}
}

/**
* Templated accessor class for numeric properties. Will wrap around a numeric property.
* Do not instanciate it manually, use PCGAttributeAccessorHelpers::CreatePropertyAccessor.
* Only support integral and floating point types.
* Key supported: Generic object
*/
template <typename T>
class FPCGNumericPropertyAccessor : public IPCGAttributeAccessorT<FPCGNumericPropertyAccessor<T>>
{
public:
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGNumericPropertyAccessor<T>>;

	FPCGNumericPropertyAccessor(const FNumericProperty* InProperty)
		: Super(/*bInReadOnly=*/ false)
		, Property(InProperty)
	{
		static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
		check(Property);
	}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		return PCGPropertyAccessor::IterateGet(Property, OutValues, Index, Keys, [this](const void* PropertyAddressData) -> T
			{
				if constexpr (std::is_integral_v<T>)
				{
					return T(Property->GetSignedIntPropertyValue(PropertyAddressData));
				}
				else
				{
					return T(Property->GetFloatingPointPropertyValue(PropertyAddressData));
				}
			});
	}

	bool SetRangeImpl(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{
		return PCGPropertyAccessor::IterateSet(Property, InValues, Index, Keys, [this](void* PropertyAddressData, const T& Value) -> void
			{
				if constexpr (std::is_integral_v<T>)
				{
					Property->SetIntPropertyValue(PropertyAddressData, Value);
				}
				else
				{
					Property->SetFloatingPointPropertyValue(PropertyAddressData, Value);
				}
			});
	}

private:
	const FNumericProperty* Property = nullptr;
};

/**
* Templated accessor class for enum properties. Will wrap around an enum property.
* Will always convert to int64 for PCG
* Do not instanciate it manually, use PCGAttributeAccessorHelpers::CreatePropertyAccessor.
* Key supported: Generic object
*/
class FPCGEnumPropertyAccessor : public IPCGAttributeAccessorT<FPCGEnumPropertyAccessor>
{
public:
	using Type = int64;
	using Super = IPCGAttributeAccessorT<FPCGEnumPropertyAccessor>;

	FPCGEnumPropertyAccessor(const FEnumProperty* InProperty)
		: Super(/*bInReadOnly=*/ false)
		, Property(InProperty)
	{
		check(Property);
	}

	bool GetRangeImpl(TArrayView<int64> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const;
	bool SetRangeImpl(TArrayView<const int64> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags);

private:
	const FEnumProperty* Property = nullptr;
};

/**
* Templated accessor class for struct properties. Will wrap around a struct property.
* Do not instanciate it manually, use PCGAttributeAccessorHelpers::CreatePropertyAccessor.
* WARNING: Getting the address of the underlying data from a container using this property
* should point to a "T" object in memory, otherwise it is UD.
* Key supported: Generic object
*/
template <typename T>
class FPCGPropertyStructAccessor : public IPCGAttributeAccessorT<FPCGPropertyStructAccessor<T>>
{
public:
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGPropertyStructAccessor<T>>;

	FPCGPropertyStructAccessor(const FStructProperty* InProperty)
		: Super(/*bInReadOnly=*/ false)
		, Property(InProperty)
	{
		static_assert(PCG::Private::IsPCGType<T>());
		check(Property);
	}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		return PCGPropertyAccessor::IterateGet(Property, OutValues, Index, Keys, [this](const void* PropertyAddressData) -> T
			{
				return *reinterpret_cast<const T*>(PropertyAddressData);
			});
	}

	bool SetRangeImpl(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{
		return PCGPropertyAccessor::IterateSet(Property, InValues, Index, Keys, [this](void* PropertyAddressData, const T& Value) -> void
			{
				*reinterpret_cast<T*>(PropertyAddressData) = Value;
			});
	}

private:
	const FStructProperty* Property = nullptr;
};

/**
* Templated accessor class for properties that has a (Get/Set)PropertyValue. Will wrap around a property.
* For example String/Name/Bool
* Do not instanciate it manually, use PCGAttributeAccessorHelpers::CreatePropertyAccessor.
* Key supported: Generic object
*/
template <typename T, typename PropertyType>
class FPCGPropertyAccessor : public IPCGAttributeAccessorT<FPCGPropertyAccessor<T, PropertyType>>
{
public:
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGPropertyAccessor<T, PropertyType>>;

	FPCGPropertyAccessor(const PropertyType* InProperty)
		: Super(/*bInReadOnly=*/ false)
		, Property(InProperty)
	{
		static_assert(PCG::Private::IsPCGType<T>());
		check(Property);
	}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		return PCGPropertyAccessor::IterateGet(Property, OutValues, Index, Keys, [this](const void* PropertyAddressData) -> T
			{
				return Property->GetPropertyValue(PropertyAddressData);
			});
	}

	bool SetRangeImpl(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{
		return PCGPropertyAccessor::IterateSet(Property, InValues, Index, Keys, [this](void* PropertyAddressData, const T& Value) -> void
			{
				Property->SetPropertyValue(PropertyAddressData, Value);
			});
	}

private:
	const PropertyType* Property = nullptr;
};

/**
* Templated accessor class for path properties. Will wrap around a soft path property.
* Do not instanciate it manually, use PCGAttributeAccessorHelpers::CreatePropertyAccessor.
* Will always convert to FString for PCG
* Key supported: Generic object
*/
template <typename T>
class FPCGPropertyPathAccessor : public IPCGAttributeAccessorT<FPCGPropertyPathAccessor<T>>
{
public:
	using Type = FString;
	using Super = IPCGAttributeAccessorT<FPCGPropertyPathAccessor<T>>;

	FPCGPropertyPathAccessor(const FProperty* InProperty)
		: Super(/*bInReadOnly=*/ false)
		, Property(InProperty)
	{
		static_assert(std::is_same_v<FSoftObjectPath, T> || std::is_same_v<FSoftClassPath, T>);
		check(Property);
	}

	bool GetRangeImpl(TArrayView<Type> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		return PCGPropertyAccessor::IterateGet(Property, OutValues, Index, Keys, [this](const void* PropertyAddressData) -> Type
			{
				return reinterpret_cast<const T*>(PropertyAddressData)->ToString();
			});
	}

	bool SetRangeImpl(TArrayView<const Type> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{
		return PCGPropertyAccessor::IterateSet(Property, InValues, Index, Keys, [this](void* PropertyAddressData, const Type& Value) -> void
			{
				reinterpret_cast<T*>(PropertyAddressData)->SetPath(Value);
			});
	}

private:
	const FProperty* Property = nullptr;
};

/**
* Templated accessor class for soft object/class ptr properties.
* Do not instantiate it manually, use PCGAttributeAccessorHelpers::CreatePropertyAccessor.
* Will always convert to FString for PCG
* Key supported: Generic object
*/
class FPCGPropertySoftPtrAccessor : public IPCGAttributeAccessorT<FPCGPropertySoftPtrAccessor>
{
public:
	using Type = FString;
	using Super = IPCGAttributeAccessorT<FPCGPropertySoftPtrAccessor>;

	FPCGPropertySoftPtrAccessor(const FSoftObjectProperty* InProperty)
		: Super(/*bInReadOnly=*/ false)
		, Property(InProperty)
	{
		check(Property);
	}

	bool GetRangeImpl(TArrayView<Type> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		return PCGPropertyAccessor::IterateGet(Property, OutValues, Index, Keys, [this](const void* PropertyAddressData) -> Type
		{
			return Property->GetPropertyValue(PropertyAddressData).ToString();
		});
	}

	bool SetRangeImpl(TArrayView<const Type> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{
		return PCGPropertyAccessor::IterateSet(Property, InValues, Index, Keys, [this](void* PropertyAddressData, const Type& Value) -> void
		{
			Property->SetPropertyValue(PropertyAddressData, FSoftObjectPtr(FSoftObjectPath(Value)));
		});
	}

private:
	const FSoftObjectProperty* Property = nullptr;
};

/**
* Templated accessor class for object/class ptr properties.
* Do not instantiate it manually, use PCGAttributeAccessorHelpers::CreatePropertyAccessor.
* Will always convert to FString for PCG
* Key supported: Generic object
*/
class FPCGPropertyObjectPtrAccessor : public IPCGAttributeAccessorT<FPCGPropertyObjectPtrAccessor>
{
public:
	using Type = FString;
	using Super = IPCGAttributeAccessorT<FPCGPropertyObjectPtrAccessor>;

	FPCGPropertyObjectPtrAccessor(const FObjectProperty* InProperty)
		: Super(/*bInReadOnly=*/ false)
		, Property(InProperty)
	{
		check(Property);
	}

	bool GetRangeImpl(TArrayView<Type> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		return PCGPropertyAccessor::IterateGet(Property, OutValues, Index, Keys, [this](const void* PropertyAddressData) -> Type
		{
			return Property->GetPropertyValue(PropertyAddressData).GetPath();
		});
	}

	bool SetRangeImpl(TArrayView<const Type> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{
		return PCGPropertyAccessor::IterateSet(Property, InValues, Index, Keys, [this](void* PropertyAddressData, const Type& Value) -> void
		{
			Property->SetPropertyValue(PropertyAddressData, FSoftObjectPath(Value).TryLoad());
		});
	}

private:
	const FObjectProperty* Property = nullptr;
};