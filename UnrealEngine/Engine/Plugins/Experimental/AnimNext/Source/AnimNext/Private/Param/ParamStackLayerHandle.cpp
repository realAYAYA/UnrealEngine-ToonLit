// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamStackLayerHandle.h"
#include "Param/ParamStackLayer.h"
#include "Param/ParamResult.h"
#include "Param/ParamHelpers.h"
#include "Param/ParamEntry.h"

namespace UE::AnimNext
{

FParamStackLayerHandle::FParamStackLayerHandle()
{
}

FParamStackLayerHandle::FParamStackLayerHandle(FParamStackLayerHandle&& InLayer) noexcept
{
	Layer = MoveTemp(InLayer.Layer);
}

FParamStackLayerHandle& FParamStackLayerHandle::operator=(FParamStackLayerHandle&& InLayer) noexcept
{
	Layer = MoveTemp(InLayer.Layer);
	return *this;
}

FParamStackLayerHandle::FParamStackLayerHandle(TUniquePtr<FParamStackLayer>&& InLayer)
	: Layer(MoveTemp(InLayer))
{}

FParamStackLayerHandle::~FParamStackLayerHandle()
{
	Layer = nullptr;
}

bool FParamStackLayerHandle::IsValid() const
{
	return Layer.IsValid();
}

void FParamStackLayerHandle::Invalidate()
{
	Layer = nullptr;
}

FParamResult FParamStackLayerHandle::SetValueRaw(FParamId InParamId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8> InData)
{
	check(Layer.IsValid());

	TArrayView<uint8> ParamData;
	FParamResult Result = Layer->GetMutableParamData(InParamId, InTypeHandle, ParamData);
	if(Result.IsSuccessful())
	{
		FParamHelpers::Copy(InTypeHandle, InData, ParamData);
	}

	return Result;
}

FParamResult FParamStackLayerHandle::GetValueRaw(FParamId InParamId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutData)
{
	check(Layer.IsValid());

	TArrayView<uint8> ParamData;
	return Layer->GetParamData(InParamId, InTypeHandle, OutData);
}

FParamResult FParamStackLayerHandle::SetValuesInternal(TConstArrayView<Private::FParamEntry> InParams)
{
	check(Layer.IsValid());

	for (const Private::FParamEntry& Param : InParams)
	{
		if(Private::FParamEntry* ParamPtr = Layer->FindMutableEntry(Param.GetId()))
		{
			check(ParamPtr->GetTypeHandle() == Param.GetTypeHandle());
			check(ParamPtr->IsMutable());

			FParamHelpers::Copy(ParamPtr->GetTypeHandle(), Param.GetData(), ParamPtr->GetMutableData());
			return EParamResult::Success;
		}
	}

	return EParamResult::NotInScope;
}

UObject* FParamStackLayerHandle::GetUObjectFromLayer() const
{
	return Layer->AsUObject();
}

FInstancedPropertyBag* FParamStackLayerHandle::GetInstancedPropertyBagFromLayer() const
{
	return Layer->AsInstancedPropertyBag();
}

FParamResult FParamStackLayerHandle::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	return Layer->GetParamData(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
}

FParamResult FParamStackLayerHandle::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	return Layer->GetMutableParamData(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
}

}