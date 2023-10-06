// Copyright Epic Games, Inc. All Rights Reserved.

#include "PerPlatformProperties.h"
#include "Concepts/StaticStructProvider.h"
#include "Engine/Engine.h"
#include "Misc/DelayedAutoRegister.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PerPlatformProperties)

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#endif

IMPLEMENT_TYPE_LAYOUT(FFreezablePerPlatformFloat);
IMPLEMENT_TYPE_LAYOUT(FFreezablePerPlatformInt);

/** Serializer to cook out the most appropriate platform override */
template<typename StructType, typename ValueType, EName BasePropertyName>
ENGINE_API FArchive& operator<<(FArchive& Ar, TPerPlatformProperty<StructType, ValueType, BasePropertyName>& Property)
{
	bool bCooked = false;
#if WITH_EDITOR
	if (Ar.IsCooking())
	{
		bCooked = true;
		Ar << bCooked;
		// Save out platform override if it exists and Default otherwise
		ValueType Value = Property.GetValueForPlatform(*Ar.CookingTarget()->IniPlatformName());
		Ar << Value;
	}
	else
#endif
	{
		StructType* This = StaticCast<StructType*>(&Property);
		Ar << bCooked;
		Ar << This->Default;
#if WITH_EDITORONLY_DATA
		if (!bCooked)
		{
			using MapType = decltype(This->PerPlatform);
			using KeyFuncs = typename PerPlatformProperty::Private::KeyFuncs<MapType>;
			KeyFuncs::SerializePerPlatformMap(Ar, This->PerPlatform);
		}
#endif
	}
	return Ar;
}

/** Serializer to cook out the most appropriate platform override */
template<typename StructType, typename ValueType, EName BasePropertyName>
ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<StructType, ValueType, BasePropertyName>& Property)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	bool bCooked = false;
#if WITH_EDITOR
	if (UnderlyingArchive.IsCooking())
	{
		bCooked = true;
		Record << SA_VALUE(TEXT("bCooked"), bCooked);
		// Save out platform override if it exists and Default otherwise
		ValueType Value = Property.GetValueForPlatform(*UnderlyingArchive.CookingTarget()->IniPlatformName());
		Record << SA_VALUE(TEXT("Value"), Value);
	}
	else
#endif
	{
		StructType* This = StaticCast<StructType*>(&Property);
		Record << SA_VALUE(TEXT("bCooked"), bCooked);
		Record << SA_VALUE(TEXT("Value"), This->Default);
#if WITH_EDITORONLY_DATA
		if (!bCooked)
		{
			using MapType = decltype(This->PerPlatform);
			using KeyFuncs = typename PerPlatformProperty::Private::KeyFuncs<MapType>;
			KeyFuncs::SerializePerPlatformMap(UnderlyingArchive, Record, This->PerPlatform);
		}
#endif
	}
}

template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FFreezablePerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FFreezablePerPlatformFloat, float, NAME_FloatProperty>&);

template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFrameRate, FFrameRate, NAME_FrameRate>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformFrameRate, FFrameRate, NAME_FrameRate>&);

FString FPerPlatformInt::ToString() const
{
	FString Result = FString::FromInt(Default);

#if WITH_EDITORONLY_DATA
	TArray<FName> SortedPlatforms;
	PerPlatform.GetKeys(/*out*/ SortedPlatforms);
	SortedPlatforms.Sort(FNameLexicalLess());

	for (FName Platform : SortedPlatforms)
	{
		Result = FString::Printf(TEXT("%s, %s=%d"), *Result, *Platform.ToString(), PerPlatform.FindChecked(Platform));
	}
#endif

	return Result;
}

FString FFreezablePerPlatformInt::ToString() const
{
	return FPerPlatformInt(*this).ToString();
}

#if WITH_EDITORONLY_DATA && WITH_EDITOR
bool GEngine_GetPreviewPlatformName(FName& PlatformName)
{
	return GEngine && GEngine->GetPreviewPlatformName(PlatformName);
}
#endif
