// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieScenePropertyRegistry.h"
#include "MovieSceneFwd.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "EntitySystem/MovieScenePropertyBinding.h"
#include "MovieSceneCommonHelpers.h"


namespace UE
{
namespace MovieScene
{

TOptional<uint16> ComputeFastPropertyPtrOffset(UClass* ObjectClass, const FMovieScenePropertyBinding& PropertyBinding)
{
	using namespace UE::MovieScene;

	FProperty* Property = ObjectClass->FindPropertyByName(PropertyBinding.PropertyName);
	
	const bool bFoundSetter = (!Property || Property->HasSetter());
	if (bFoundSetter)
	{
		return TOptional<uint16>();
	}

	// @todo: Constructing FNames from strings is _very_ costly and we really shouldn't be doing this at runtime.
	UFunction* Setter   = ObjectClass->FindFunctionByName(*(FString("Set") + PropertyBinding.PropertyName.ToString()));
	if (Property && !Setter)
	{
		// @todo: We disable fast offsets for all bools atm becasue there is no way of knowing whether the property
		// represents a raw bool, or a bool : 1 or uint8 : 1;
		FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
		if (BoolProperty)
		{
			return TOptional<uint16>();
		}

		UObject* DefaultObject   = ObjectClass->GetDefaultObject();
		uint8*   PropertyAddress = Property->ContainerPtrToValuePtr<uint8>(DefaultObject);
		int32    PropertyOffset  = PropertyAddress - reinterpret_cast<uint8*>(DefaultObject);

		if (PropertyOffset >= 0 && PropertyOffset < int32(uint16(0xFFFF)))
		{
			return uint16(PropertyOffset);
		}
	}

	return TOptional<uint16>();
}

TOptional<FResolvedFastProperty> FPropertyRegistry::ResolveFastProperty(UObject* Object, const FMovieScenePropertyBinding& PropertyBinding, FCustomAccessorView CustomAccessors)
{
	UClass* Class = Object->GetClass();

	if (CustomAccessors.Num() != 0)
	{
		const int32 CustomPropertyIndex = CustomAccessors.FindCustomAccessorIndex(Class, PropertyBinding.PropertyPath);
		if (CustomPropertyIndex != INDEX_NONE)
		{
			check(CustomPropertyIndex < MAX_uint16);

			// This property has a custom property accessor that can apply properties through a static function ptr.
			// Just add the function ptrs to the property entity so they can be called directly
			return FResolvedFastProperty(TInPlaceType<FCustomPropertyIndex>(), FCustomPropertyIndex{ static_cast<uint16>(CustomPropertyIndex) });
		}
	}

	if (PropertyBinding.CanUseClassLookup())
	{
		TOptional<uint16> FastPtrOffset = ComputeFastPropertyPtrOffset(Class, PropertyBinding);
		if (FastPtrOffset.IsSet())
		{
			// This property/object combination has no custom setter function and a constant property offset from the base ptr for all instances of the object.
			return FResolvedFastProperty(TInPlaceType<uint16>(), FastPtrOffset.GetValue());
		}
	}

	return TOptional<FResolvedFastProperty>();
}

TOptional<FResolvedProperty> FPropertyRegistry::ResolveProperty(UObject* Object, const FMovieScenePropertyBinding& PropertyBinding, FCustomAccessorView CustomAccessors)
{
	TOptional< FResolvedFastProperty > FastProperty = FPropertyRegistry::ResolveFastProperty(Object, PropertyBinding, CustomAccessors);
	if (FastProperty.IsSet())
	{
		if (const FCustomPropertyIndex* CustomIndex = FastProperty->TryGet<FCustomPropertyIndex>())
		{
			return FResolvedProperty(TInPlaceType<FCustomPropertyIndex>(), *CustomIndex);
		}
		return FResolvedProperty(TInPlaceType<uint16>(), FastProperty->Get<uint16>());
	}

	// None of the above optimized paths can apply to this property (probably because it has a setter function or because it is within a compound property), so we must use the slow property bindings
	TSharedPtr<FTrackInstancePropertyBindings> SlowBindings = MakeShared<FTrackInstancePropertyBindings>(PropertyBinding.PropertyName, PropertyBinding.PropertyPath.ToString());
	if (SlowBindings->GetProperty(*Object) == nullptr)
	{
		UE_LOG(LogMovieSceneECS, Warning, TEXT("Unable to resolve property '%s' from '%s' instance '%s'"), *PropertyBinding.PropertyPath.ToString(), *Object->GetClass()->GetName(), *Object->GetName());
		return TOptional<FResolvedProperty>();
	}

	return FResolvedProperty(TInPlaceType<TSharedPtr<FTrackInstancePropertyBindings>>(), SlowBindings);
}


} // namespace MovieScene
} // namespace UE