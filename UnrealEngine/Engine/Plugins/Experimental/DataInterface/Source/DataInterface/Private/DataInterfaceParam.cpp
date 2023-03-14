// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceParam.h"

namespace UE::DataInterface
{

static FParamType DefaultType;
static TArray<FParamType*> GTypeRegistry;
static TArray<TUniqueFunction<void(void)>> GDeferredTypes;

FParam::FParam(const FParamType& InType, void* InData, EFlags InFlags)
	: Data(InData)
	, TypeId(InType.GetTypeId())
	, Flags(InFlags)
{
	check(TypeId != INDEX_NONE);
	check(Data);
}

FParam::FParam(const FParam* InOtherParam)
	: Data(InOtherParam->Data)
	, TypeId(InOtherParam->TypeId)
	, Flags(InOtherParam->Flags)
{
}

bool FParam::CanAssignTo(const FParam& InParam) const
{
	return CanAssignWith(InParam.GetType(), InParam.Flags);
}

bool FParam::CanAssignWith(const FParamType& InType, EFlags InFlags, FStringBuilderBase* OutReasonPtr) const
{
	// Check type
	if(TypeId != InType.GetTypeId())
	{
		if(OutReasonPtr)
		{
			const FParamType& ThisType = FParamType::FRegistrar::GetTypeById(TypeId);
			OutReasonPtr->Appendf(TEXT("Types do not match: %s and %s"), *ThisType.GetName().ToString(), *InType.GetName().ToString());
		}
		return false;
	}
	
	// Check const
	if(!EnumHasAnyFlags(InFlags, EFlags::Mutable))
	{
		if(OutReasonPtr)
		{
			OutReasonPtr->Append(TEXT("Target is not mutable"));
		}
		return false;
	}

	// Check chunking
	if(EnumHasAnyFlags(Flags, EFlags::Chunked) != EnumHasAnyFlags(InFlags, EFlags::Chunked))
	{
		if(OutReasonPtr)
		{
			OutReasonPtr->Append(TEXT("Incompatible chunking"));
		}
		return false;
	}
	
	return true;
}

void FParamType::FRegistrar::RegisterType(FParamType& InType, const UScriptStruct* InStruct, FName InName, uint32 InSize, uint32 InAlignment)
{
	InType.Struct = InStruct;
	InType.Name = InName;
	InType.Size = InSize;
	InType.Alignment = InAlignment;

	check(InType.Name != NAME_None);
	check(InType.Size != 0);
	check(InType.Alignment != 0);
	
	InType.TypeId = GTypeRegistry.Add(&InType);
}

const FParamType& FParamType::FRegistrar::FindTypeByName(FName InName)
{
	for(FParamType* Type : GTypeRegistry)
	{
		if(Type->Name == InName)
		{
			return *Type;
		}
	}

	checkf(false, TEXT("Type %s not found"), *InName.ToString());
	return DefaultType;
}

const FParamType& FParamType::FRegistrar::GetTypeById(uint16 InTypeId)
{
	if(GTypeRegistry.IsValidIndex(InTypeId))
	{
		return *GTypeRegistry[InTypeId];
	}

	checkf(false, TEXT("Type ID %d not found"), InTypeId);
	return DefaultType;	
}

FParamType::FRegistrar::FRegistrar(TUniqueFunction<void(void)>&& InFunction)
{
	check(IsInGameThread());
	GDeferredTypes.Add(MoveTemp(InFunction));
}

void FParamType::FRegistrar::RegisterDeferredTypes()
{
	check(IsInGameThread());
	
	for(TUniqueFunction<void(void)>& Function : GDeferredTypes)
	{
		Function();
	}

	GDeferredTypes.Empty();
}

const FParamType& FParam::GetType() const
{
	check(GTypeRegistry.IsValidIndex(TypeId));
	return *GTypeRegistry[TypeId];
}
/*
static FParamType& GetTypeForProperty(const FProperty* InProperty)
{
	const FName TypeName = *InProperty->GetCPPType();
	return FParamType::FRegistrar::FindTypeByName(TypeName);
}

FPropertyParam::FPropertyParam(const FProperty* InProperty, UObject* InContainer)
	: FParam(GetTypeForProperty(InProperty), InProperty->ContainerPtrToValuePtr<void>(InContainer), EFlags::Const)	// Property parameters are assumed to be const
{
}
*/
}
