// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceParam.h"
#include "AnimNextInterfaceParamStorage.h"

namespace UE::AnimNext::Interface
{

static FParamType DefaultType;
static TArray<FParamType*> GTypeRegistry;
static TArray<TUniqueFunction<void(void)>> GDeferredTypes;

FParam::FParam(const FParamType& InType, void* InData, EFlags InFlags)
	: Data(InData)
	, NumElements((InData != nullptr) ? 1 : 0)
	, TypeId(InType.GetTypeId())
	, Flags(InFlags)
{
	check(TypeId != INDEX_NONE);
	check(Data);
}

FParam::FParam(const FParamType& InType, void* InData, int32 InNumElements, EFlags InFlags)
	: Data(InData)
	, NumElements(InNumElements)
	, TypeId(InType.GetTypeId())
	, Flags(InFlags)
{
}

FParam::FParam(const FParamType& InType, EFlags InFlags)
	: Data(nullptr)
	, NumElements(0)
	, TypeId(InType.GetTypeId())
	, Flags(InFlags)
{
	check(TypeId != INDEX_NONE);
}

FParam::FParam(const FParam* InOtherParam)
	: Data(EnumHasAnyFlags(InOtherParam->Flags, FParam::EFlags::Embedded) ? (void*)&InOtherParam->Data : InOtherParam->Data)
	, NumElements(InOtherParam->NumElements)
	, TypeId(InOtherParam->TypeId)
	, Flags(InOtherParam->Flags)
{
	EnumRemoveFlags(Flags, FParam::EFlags::Embedded); // For compatibility reasons, I can not keep the embedded value if we create a copy of the FParam
}

bool FParam::CanAssignTo(const FParam& InParam) const
{
	return CanAssignWith(InParam.GetType(), InParam.Flags, InParam.GetNumElements());
}

bool FParam::CanAssignWith(const FParamType& InType, EFlags InFlags, int32 InNumElements, FStringBuilderBase* OutReasonPtr) const
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
	
	// Check const if not empty, else we assume it is an empty param and can be written, mutable or not
	if(InNumElements > 0 && !EnumHasAnyFlags(InFlags, EFlags::Mutable))
	{
		if(OutReasonPtr)
		{
			OutReasonPtr->Append(TEXT("Target is not mutable"));
		}
		return false;
	}

	// Check batching
	if (EnumHasAnyFlags(Flags, EFlags::Batched) != EnumHasAnyFlags(InFlags, EFlags::Batched))
	{
		// If different batching, allow it if the target is empty and the source is 1 (this enables passing a single value to a context that expects N)
		if (NumElements != 1 || InNumElements != 0)
		{
			if (OutReasonPtr)
			{
				OutReasonPtr->Append(TEXT("Incompatible batching"));
			}
			return false;
		}
	}
	
	return true;
}

void FParamType::FRegistrar::RegisterType(FParamType& InType, const UScriptStruct* InStruct, FName InName, uint32 InSize, uint32 InAlignment, const ParamCopyFunction InParamCopyFunctionPtr)
{
	InType.Struct = InStruct;
	InType.Name = InName;
	InType.Size = InSize;
	InType.Alignment = InAlignment;
	InType.ParamCopyFunctionPtr = InParamCopyFunctionPtr;

	check(InType.Name != NAME_None);
	check(InType.Size != 0);
	check(InType.Alignment != 0);
	check(InType.ParamCopyFunctionPtr != nullptr);
	
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

FHParam::FHParam(FParamStorage* InOwnerStorage, FParamHandle InParamHandle)
	: OwnerStorage(InOwnerStorage)
	, ParamHandle(InParamHandle)
{
}

FHParam::FHParam(const FHParam& Other)
	: OwnerStorage(Other.OwnerStorage)
	, ParamHandle(Other.ParamHandle)
{
	if (ParamHandle != InvalidParamHandle)
	{
		OwnerStorage->IncRefCount(ParamHandle);
	}
}

FHParam::~FHParam()
{
	if (ParamHandle != InvalidParamHandle)
	{
		check(OwnerStorage != nullptr);
		OwnerStorage->DecRefCount(ParamHandle);
	}
}

} // end namespace UE::AnimNext::Interface
