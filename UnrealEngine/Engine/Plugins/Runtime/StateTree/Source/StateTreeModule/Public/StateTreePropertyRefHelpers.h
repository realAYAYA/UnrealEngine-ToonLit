// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/Field.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"

#if WITH_EDITOR
#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointerFwd.h"
#endif

class FProperty;
struct FBindingChainElement;
struct FStateTreeBindableStructDesc;
struct FStateTreePropertyPathIndirection;
struct FEdGraphPinType;

namespace UE::StateTree::PropertyRefHelpers
{
#if WITH_EDITOR
	/**
	 * @param RefProperty Property of PropertyRef type
	 * @param SourceProperty Property to check it's type compatibility
	 * @return true if SourceProperty type is compatible with PropertyRef
	 */
	bool STATETREEMODULE_API IsPropertyRefCompatibleWithProperty(const FProperty& RefProperty, const FProperty& SourceProperty);

	/**
	 * @param SourcePropertyPathIndirections Path indirections of the referenced property
	 * @param SourceStruct Bindable owner of referenced property
	 * @return true if property can be referenced by PropertyRef
	 */
	bool STATETREEMODULE_API IsPropertyAccessibleForPropertyRef(TConstArrayView<FStateTreePropertyPathIndirection> SourcePropertyPathIndirections, FStateTreeBindableStructDesc SourceStruct);

	/**
	 * @param SourceProperty Referenced property
	 * @param BindingChain Binding chain to referenced property
	 * @param SourceStruct Bindable owner of referenced property
	 * @return true if property can be referenced by PropertyRef
	 */
	bool STATETREEMODULE_API IsPropertyAccessibleForPropertyRef(const FProperty& SourceProperty, TConstArrayView<FBindingChainElement> BindingChain, FStateTreeBindableStructDesc SourceStruct);

	/**
	 * @param Property Property to check
	 * @return true if Property is a PropertyRef
	 */
	bool STATETREEMODULE_API IsPropertyRef(const FProperty& Property);

	/**
	 * @param RefProperty Property of PropertyRef type
	 * @return PinType for PropertyRef's internal type
	 */
	FEdGraphPinType STATETREEMODULE_API GetPropertyRefInternalTypeAsPin(const FProperty& RefProperty);
#endif

	template<class T, class = void>
	struct Validator
	{};

	template<>
	struct Validator<FBoolProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FBoolProperty>(); };
	};

	template<>
	struct Validator<FByteProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FByteProperty>(); };
	};

	template<>
	struct Validator<FIntProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FIntProperty>(); };
	};

	template<>
	struct Validator<FInt64Property::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FInt64Property>(); };
	};

	template<>
	struct Validator<FFloatProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FFloatProperty>(); };
	};

	template<>
	struct Validator<FDoubleProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FDoubleProperty>(); };
	};

	template<>
	struct Validator<FNameProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FNameProperty>(); };
	};

	template<>
	struct Validator<FStrProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FStrProperty>(); };
	};

	template<>
	struct Validator<FTextProperty::TCppType>
	{
		static bool IsValid(const FProperty& Property){ return Property.IsA<FTextProperty>(); };
	};

	template<class T>
	struct Validator<T, typename TEnableIf<TIsTArray_V<T>, void>::Type>
	{
		static bool IsValid(const FProperty& Property)
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
			{
				return Validator<typename T::ElementType>::IsValid(*ArrayProperty->Inner);
			}

			return false;
		}
	};

	template<class T>
	struct Validator<T, decltype(TBaseStructure<T>::Get, void())>
	{
		static bool IsValid(const FProperty& Property)
		{		
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
			{
				return StructProperty->Struct->IsChildOf(TBaseStructure<T>::Get());
			}

			return false;
		}
	};

	template<class T>
	struct Validator<T, typename TEnableIf<TIsDerivedFrom<typename TRemovePointer<T>::Type, UObject>::IsDerived>::Type>
	{
		static bool IsValid(const FProperty& Property)
		{		
			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(&Property))
			{
				return ObjectProperty->PropertyClass == TRemovePointer<T>::Type::StaticClass();
			}

			return false;
		}
	};

	template<class T>
	struct Validator<T, typename TEnableIf<TIsEnum<T>::Value>::Type>
	{
		static bool IsValid(const FProperty& Property)
		{		
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
			{
				return EnumProperty->GetEnum() == StaticEnum<T>();
			}

			return false;
		}
	};
}