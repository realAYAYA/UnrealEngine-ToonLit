// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamStack.h"
#include "Param/ParamHelpers.h"
#include "PropertyBag.h"
#include "EngineLogs.h"

#define LOCTEXT_NAMESPACE "AnimNextParamStack"

namespace UE::AnimNext
{

FParamStack::FParam::FParam(const FParamTypeHandle& InTypeHandle, TArrayView<uint8> InData, bool bInIsReference, bool bInIsMutable)
	: Data(nullptr)
	, TypeHandle(InTypeHandle)
	, Size(InData.Num())
	, Flags(EParamFlags::None)
{
	check(TypeHandle.IsValid());
	check(InData.Num() > 0 && InData.Num() < 0xffff);

	// If we can store our data inside of a ptr, we do
	if (!bInIsReference && InData.Num() <= sizeof(void*))
	{
		FParamHelpers::Copy(InTypeHandle, InData, TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
		Flags |= EParamFlags::Embedded;
	}
	else
	{
		Data = InData.GetData();
	}

	if (bInIsReference)
	{
		Flags |= EParamFlags::Reference;
	}

	if (bInIsMutable)
	{
		Flags |= EParamFlags::Mutable;
	}
}

FParamStack::FParam::~FParam()
{
	if (Size > 0)
	{
		if (IsEmbedded())
		{
			FParamHelpers::Destroy(TypeHandle, TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
		}
	}
}

FParamStack::FParam::FParam(const FParam& InOtherParam)
	: TypeHandle(InOtherParam.TypeHandle)
	, Size(InOtherParam.Size)
	, Flags(InOtherParam.Flags)
{
	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}
}

FParamStack::FParam& FParamStack::FParam::operator=(const FParam& InOtherParam)
{
	TypeHandle = InOtherParam.TypeHandle;
	Size = InOtherParam.Size;
	Flags = InOtherParam.Flags;

	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}

	return *this;
}

FParamStack::FParam::FParam(FParam&& InOtherParam)
	: TypeHandle(InOtherParam.TypeHandle)
	, Size(InOtherParam.Size)
	, Flags(InOtherParam.Flags)
{
	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}
}

FParamStack::FParam& FParamStack::FParam::operator=(FParam&& InOtherParam)
{
	TypeHandle = InOtherParam.TypeHandle;
	Size = InOtherParam.Size;
	Flags = InOtherParam.Flags;

	if (IsEmbedded())
	{
		FParamHelpers::Copy(InOtherParam.TypeHandle, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&InOtherParam.Data), sizeof(void*)), TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), sizeof(void*)));
	}
	else
	{
		Data = InOtherParam.Data;
	}

	return *this;
}

FParamStackLayer::FParamStackLayer(const FInstancedPropertyBag& InPropertyBag)
{
	TConstArrayView<FPropertyBagPropertyDesc> Descs = InPropertyBag.GetPropertyBagStruct()->GetPropertyDescs();
	
	// Determine param ID range for this layer
	TArray<FParamId> CachedIds;
	CachedIds.SetNumUninitialized(Descs.Num());
	MinParamId = MAX_uint32;
	uint32 MaxParamId = 0;
	for (uint32 DescIndex = 0; DescIndex < static_cast<uint32>(Descs.Num()); ++DescIndex)
	{
		const FPropertyBagPropertyDesc& Desc = Descs[DescIndex];
		const FParamId& ParamId = CachedIds[DescIndex] = FParamId(Desc.Name);
		MinParamId = FMath::Min(ParamId.ToInt(), MinParamId);
		MaxParamId = FMath::Max(ParamId.ToInt(), MaxParamId);
	}

	if (MinParamId <= MaxParamId)
	{
		const uint32 ParamRangeSize = (MaxParamId - MinParamId) + 1;
		Params.SetNumZeroed(ParamRangeSize);
		FConstStructView StructView = InPropertyBag.GetValue();
		for (uint32 DescIndex = 0; DescIndex < static_cast<uint32>(Descs.Num()); ++DescIndex)
		{
			const FPropertyBagPropertyDesc& Desc = Descs[DescIndex];
			const FParamId& ParamId = CachedIds[DescIndex];
			const uint8* DataPtr = StructView.GetMemory() + Desc.CachedProperty->GetOffset_ForInternal();
			const uint32 LocalParamIndex = ParamId.ToInt() - MinParamId;
			Params[LocalParamIndex] = FParamStack::FParam(FParamTypeHandle::FromPropertyBagPropertyDesc(Desc), TArrayView<uint8>(const_cast<uint8*>(DataPtr), Desc.CachedProperty->GetSize()), true, false);
		}
	}
}

FParamStackLayer::FParamStackLayer(TConstArrayView<TPair<FParamId, FParamStack::FParam>> InParams)
{
	MinParamId = MAX_uint32;
	uint32 MaxParamId = 0;
	for (uint32 ParamIndex = 0; ParamIndex < static_cast<uint32>(InParams.Num()); ++ParamIndex)
	{
		MinParamId = FMath::Min(InParams[ParamIndex].Key.ToInt(), MinParamId);
		MaxParamId = FMath::Max(InParams[ParamIndex].Key.ToInt(), MaxParamId);
	}

	if (MinParamId <= MaxParamId)
	{
		const uint32 ParamRangeSize = (MaxParamId - MinParamId) + 1;
		Params.SetNumZeroed(ParamRangeSize);
		for (uint32 ParamIndex = 0; ParamIndex < static_cast<uint32>(InParams.Num()); ++ParamIndex)
		{
			const TPair<FParamId, FParamStack::FParam>& Pair = InParams[ParamIndex];
			const uint32 LocalParamIndex = Pair.Key.ToInt() - MinParamId;
			Params[LocalParamIndex] = Pair.Value;
		}
	}
}

FParamStack::FPushedLayer::FPushedLayer(FParamStackLayer& InLayer, FParamStack& InStack)
	: Layer(InLayer)
{
	if (Layer.Params.Num())
	{
		InStack.ResizeLayerIndices();

		for (uint32 LocalParamIndex = 0; LocalParamIndex < (uint32)InLayer.Params.Num(); ++LocalParamIndex)
		{
			const uint32 GlobalParamIndex = Layer.MinParamId + LocalParamIndex;
			InStack.LayerIndices[GlobalParamIndex] = InLayer.Params[LocalParamIndex].IsValid() ? 0 : MAX_uint16;
		}
	}
}

FParamStack::FPushedLayer::FPushedLayer(const FPushedLayer& InPreviousLayer, FParamStackLayer& InLayer, FParamStack& InStack)
	: Layer(InLayer)
{
	if (Layer.Params.Num())
	{
		InStack.ResizeLayerIndices();

		const uint32 NumParams = Layer.Params.Num();
		const uint32 BaseLayerIndex = InStack.PreviousLayerIndices.Num();
		InStack.PreviousLayerIndices.SetNum(BaseLayerIndex + NumParams);
		PreviousLayerIndexStart = BaseLayerIndex;
		
		// Update layer indices and build previous layer indices
		for (uint32 LocalParamIndex = 0; LocalParamIndex < NumParams; ++LocalParamIndex)
		{
			const uint32 GlobalParamIndex = Layer.MinParamId + LocalParamIndex;
			InStack.PreviousLayerIndices[BaseLayerIndex + LocalParamIndex] = InStack.LayerIndices[GlobalParamIndex];
			if(InLayer.Params[LocalParamIndex].IsValid())
			{
				InStack.LayerIndices[GlobalParamIndex] = InStack.Layers.Num();
			}
		}
	}
}

struct FParamStackThreadData : TThreadSingleton<FParamStackThreadData>
{
	FParamStack Stack;
};

FParamStack::FParamStack()
{
	Layers.Reserve(8);
	LayerIndices.Reserve(FParamId::GetMaxParamId().ToInt());
	PreviousLayerIndices.Reserve(FParamId::GetMaxParamId().ToInt());
}

FParamStack& FParamStack::Get()
{
	return FParamStackThreadData::Get().Stack;
}

void FParamStack::PushLayer(FParamStackLayer& InLayer)
{
	if (Layers.Num() < MAX_uint16)
	{
		if (Layers.Num())
		{
			Layers.Push(FPushedLayer(Layers.Top(), InLayer, *this));
		}
		else
		{
			Layers.Push(FPushedLayer(InLayer, *this));
		}
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("FParamStack: Could not push a layer: Maximum 65535 stack layers."))
	}
}

void FParamStack::PushLayer(TConstArrayView<TPair<FParamId, FParamStack::FParam>> InParams)
{
	if(Layers.Num() < MAX_uint16)
	{
		FParamStackLayer& OwnedLayer = OwnedStackLayers.Add_GetRef(FParamStackLayer(InParams));
		OwnedLayer.OwnedStorageOffset = AllocAndCopyOwnedParamStorage(OwnedLayer.Params);
		PushLayer(OwnedLayer);
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("FParamStack: Could not push a layer: Maximum 65535 stack layers."))
	}
}

void FParamStack::PopLayer()
{
	if(Layers.Num() > 0)
	{
		const FPushedLayer& TopLayer = Layers.Top();

		// Fixup layer indices to previous, if any
		const uint32 NumParams = TopLayer.Layer.Params.Num();
		if(NumParams > 0 && PreviousLayerIndices.Num() > 0)
		{
			for (uint32 LocalParamIndex = 0; LocalParamIndex < (uint32)NumParams; ++LocalParamIndex)
			{
				const uint16 PreviousLayerIndex = PreviousLayerIndices[TopLayer.PreviousLayerIndexStart + LocalParamIndex];
				if (PreviousLayerIndex != MAX_uint16)
				{
					const uint32 GlobalParamIndex = TopLayer.Layer.MinParamId + LocalParamIndex;
					LayerIndices[GlobalParamIndex] = PreviousLayerIndex;
				}
			}

			PreviousLayerIndices.SetNum(PreviousLayerIndices.Num() - NumParams);
		}

		// Dont shrink allocs to avoid thrashing
		constexpr bool bAllowShrinking = false;

		// If we own the layer, pop the owned stack
		if(TopLayer.Layer.OwnedStorageOffset != MAX_uint32)
		{
			check(OwnedStackLayers.Num() && &TopLayer.Layer == &OwnedStackLayers[OwnedStackLayers.Num() - 1]);
			OwnedStackLayers.Pop(bAllowShrinking);

			// Free any owned storage
			FreeOwnedParamStorage(TopLayer.Layer.OwnedStorageOffset);
		}

		// Pop the layer itself
		Layers.Pop(bAllowShrinking);
	}
}

TUniquePtr<FParamStackLayer> FParamStack::MakeLayer(const FInstancedPropertyBag& InPropertyBag)
{
	TUniquePtr<FParamStackLayer> Layer = TUniquePtr<FParamStackLayer>(new FParamStackLayer(InPropertyBag));
	return Layer;
}

TUniquePtr<FParamStackLayer> FParamStack::MakeLayer(TConstArrayView<TPair<FParamId, FParamStack::FParam>> InParams)
{
	TUniquePtr<FParamStackLayer> Layer = TUniquePtr<FParamStackLayer>(new FParamStackLayer(InParams));
	return Layer;
}

FParamStack::EGetParamResult FParamStack::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData) const
{
	if (InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return EGetParamResult::NotInScope;
	}

	const FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const uint32 LocalParamIndex = InId.ToInt() - Layer.Layer.MinParamId;

	const FParam& Param = Layer.Layer.Params[LocalParamIndex];
	if (!Param.IsValid())
	{
		return EGetParamResult::NotInScope;
	}

	if (Param.GetTypeHandle() != InTypeHandle)
	{
		return EGetParamResult::IncorrectType;
	}

	OutParamData = Param.GetData();

	return EGetParamResult::Succeeded;
}

FParamStack::EGetParamResult FParamStack::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData)
{
	if (InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return EGetParamResult::NotInScope;
	}

	FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const uint32 LocalParamIndex = InId.ToInt() - Layer.Layer.MinParamId;

	FParam& Param = Layer.Layer.Params[LocalParamIndex];
	if (!Param.IsValid())
	{
		return EGetParamResult::NotInScope;
	}

	EGetParamResult AccessResult = Param.GetTypeHandle() != InTypeHandle ? EGetParamResult::IncorrectType : EGetParamResult::Succeeded;
	AccessResult |= !Param.IsMutable() ? EGetParamResult::Immutable : EGetParamResult::Succeeded;
	if (AccessResult != EGetParamResult::Succeeded)
	{
		return AccessResult;
	}

	OutParamData = Param.GetMutableData();

	return EGetParamResult::Succeeded;
}

bool FParamStack::IsMutableParam(FParamId InId) const
{
	if (InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return false;
	}

	const FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const uint32 LocalParamIndex = InId.ToInt() - Layer.Layer.MinParamId;

	const FParam& Param = Layer.Layer.Params[LocalParamIndex];

	return Param.IsMutable();
}

bool FParamStack::IsReferenceParam(FParamId InId) const
{
	if (InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return false;
	}

	const FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const uint32 LocalParamIndex = InId.ToInt() - Layer.Layer.MinParamId;

	const FParam& Param = Layer.Layer.Params[LocalParamIndex];

	return Param.IsReference();
}

uint32 FParamStack::AllocAndCopyOwnedParamStorage(TArrayView<FParam> InParams)
{
	const uint32 CurrentOffset = OwnedLayerParamStorage.Num();
	const uint32 PageSize = OwnedLayerParamStorage.MaxPerPage();

	for (FParam& Param : InParams)
	{
		if(Param.IsValid())
		{
			FParamTypeHandle TypeHandle = Param.GetTypeHandle();
			const uint32 Size = TypeHandle.GetSize();

			// Dont copy references or embdedded values to internal storage
			// - References should refer back to the originally-passed in data
			// - Embedded do not need their own storage 
			if (!Param.IsReference() && !Param.IsEmbedded())
			{
				const uint32 Alignment = TypeHandle.GetAlignment();
				uint32 AllocSize = Align(Size, Alignment);
				const uint32 NumPages = OwnedLayerParamStorage.NumPages();
				check(AllocSize < PageSize);

				// Do we need a new page? If so extend allocated size to the next page boundary
				const uint32 PageMax = (NumPages + 1) * PageSize;
				uint32 BaseOffset = Align(CurrentOffset, Alignment);
				if (BaseOffset + AllocSize > PageMax)
				{
					OwnedLayerParamStorage.SetNum((NumPages + 1) * PageSize);
				}

				// Extend to encompass requested size
				BaseOffset = Align(OwnedLayerParamStorage.Num(), Alignment);
				OwnedLayerParamStorage.SetNum(BaseOffset + AllocSize);
		
				// Copy param data
				TArrayView<uint8> TargetMemory(&OwnedLayerParamStorage[BaseOffset], Size);
				FParamHelpers::Copy(TypeHandle, TypeHandle, Param.GetData(), TargetMemory);

				// Update param to reference new storage
				Param.Data = TargetMemory.GetData();
			}
		}
	}

	return CurrentOffset;
}

void FParamStack::FreeOwnedParamStorage(uint32 InOffset)
{
	check(InOffset <= (uint32)OwnedLayerParamStorage.Num());

	OwnedLayerParamStorage.SetNum(InOffset, false);
}

void FParamStack::ResizeLayerIndices()
{
	const uint32 NumParams = FParamId::GetMaxParamId().ToInt();
	const uint32 NumLayerIndices = LayerIndices.Num();

	if (NumParams > NumLayerIndices)
	{
		LayerIndices.SetNum(NumParams);
		FMemory::Memset(&LayerIndices[NumLayerIndices], 0xff, (NumParams - NumLayerIndices) * sizeof(uint16));
	}
}

}

#undef LOCTEXT_NAMESPACE