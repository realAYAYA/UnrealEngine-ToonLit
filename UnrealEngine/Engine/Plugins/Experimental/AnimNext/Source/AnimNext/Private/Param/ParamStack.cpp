// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamStack.h"

#include "Param/ParamHelpers.h"
#include "PropertyBag.h"
#include "EngineLogs.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Param/ParamStackLayer.h"
#include "UObject/ObjectKey.h"

DEFINE_STAT(STAT_AnimNext_ParamStack_GetParam);
DEFINE_STAT(STAT_AnimNext_ParamStack_Adapter);
DEFINE_STAT(STAT_AnimNext_ParamStack_Coalesce);
DEFINE_STAT(STAT_AnimNext_ParamStack_Decoalesce);

#define LOCTEXT_NAMESPACE "AnimNextParamStack"

namespace UE::AnimNext
{

// Stack layer that can own its own data or reference an external FInstancedPropertyBag
struct FInstancedPropertyBagLayer : FParamStackLayer, FGCObject
{
	FInstancedPropertyBagLayer() = delete;

	explicit FInstancedPropertyBagLayer(FInstancedPropertyBag& InPropertyBag, bool bInMutable)
		: FParamStackLayer(InPropertyBag.GetPropertyBagStruct() ? InPropertyBag.GetPropertyBagStruct()->GetPropertyDescs().Num() : 0)
	{
		if (const UPropertyBag* PropertyBagStruct = InPropertyBag.GetPropertyBagStruct())
		{
			TConstArrayView<FPropertyBagPropertyDesc> Descs = PropertyBagStruct->GetPropertyDescs();
			Params.Reserve(Descs.Num());
			FStructView StructView = InPropertyBag.GetMutableValue();
			for (uint32 DescIndex = 0; DescIndex < static_cast<uint32>(Descs.Num()); ++DescIndex)
			{
				const FPropertyBagPropertyDesc& Desc = Descs[DescIndex];
				const FParamId ParamId(Desc.Name);
				uint8* DataPtr = StructView.GetMemory() + Desc.CachedProperty->GetOffset_ForInternal();
				uint32 ParamIndex = Params.Emplace(ParamId, FParamTypeHandle::FromPropertyBagPropertyDesc(Desc), TArrayView<uint8>(DataPtr, Desc.CachedProperty->GetSize()), true, true);
				HashTable.Add(ParamId.GetHash(), ParamIndex);
			}
		}
	}

	// FGCObject interface
	virtual FString GetReferencerName() const override
	{
		return TEXT("AnimNext Instanced Property Bag Parameter Layer");
	}
};

// Stack layer that owns its own data as a FInstancedPropertyBag
struct FInstancedPropertyBagValueLayer : FInstancedPropertyBagLayer
{
	FInstancedPropertyBagValueLayer() = delete;

	explicit FInstancedPropertyBagValueLayer(FInstancedPropertyBag&& InPropertyBag, bool bInMutable)
		: FInstancedPropertyBagLayer(InPropertyBag, bInMutable)
		, PropertyBag(MoveTemp(InPropertyBag))
	{}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		PropertyBag.AddStructReferencedObjects(Collector);
	}

	// FParamStackLayer interface
	virtual FInstancedPropertyBag* AsInstancedPropertyBag() override
	{
		return &PropertyBag;
	}

	FInstancedPropertyBag PropertyBag;
};

// Stack layer that references an external FInstancedPropertyBag
struct FInstancedPropertyBagReferenceLayer : FInstancedPropertyBagLayer
{
	FInstancedPropertyBagReferenceLayer() = delete;

	explicit FInstancedPropertyBagReferenceLayer(FInstancedPropertyBag& InPropertyBag, bool bInMutable)
		: FInstancedPropertyBagLayer(InPropertyBag, bInMutable)
		, PropertyBag(InPropertyBag)
	{}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) { /* We do not own the references held here, assume they are accounted for elsewhere */ }

	// FParamStackLayer interface
	virtual FInstancedPropertyBag* AsInstancedPropertyBag() override
	{
		return &PropertyBag;
	}

	FInstancedPropertyBag& PropertyBag;
};

// Stack layer that remaps params from another layer
struct FRemappedLayer : FParamStackLayer
{
	FRemappedLayer() = delete;

	explicit FRemappedLayer(const FParamStackLayer& InLayer, const TMap<FName, FName>& InMapping)
		: FParamStackLayer(InMapping.Num())
	{
		Params.Reserve(InMapping.Num());
		for (const Private::FParamEntry& OtherParamEntry : InLayer.Params)
		{
			if(const FName* RemappedName = InMapping.Find(OtherParamEntry.GetName()))
			{
				uint32 ParamIndex = Params.Emplace(OtherParamEntry);
				Private::FParamEntry& ParamEntry = Params[ParamIndex];
				ParamEntry.Id = FParamId(*RemappedName);
				HashTable.Add(ParamEntry.Id.GetHash(), ParamIndex);
			}
		}
	}
};

FParamStack::FPushedLayer::FPushedLayer(FParamStackLayer& InLayer, FParamStack& InStack)
	: Layer(InLayer)
{
	SerialNumber = InStack.MakeSerialNumber();

	InStack.EntryStack.Reserve(InStack.EntryStack.Num() + InLayer.Params.Num());
	for (Private::FParamEntry& Param : InLayer.Params)
	{
		uint32 ParamIndex = InStack.EntryStack.Add(&Param);
		InStack.LayerHash.Add(Param.GetHash(), ParamIndex);
	}
}

// Current stack assigned to this thread
static thread_local TWeakPtr<FParamStack> GWeakStack;

// Stacks that are associated with objects, pending execution of an object's tick function
static TMap<TObjectKey<UObject>, TWeakPtr<FParamStack>> GPendingObjects;
static FRWLock GPendingObjectsLock;

FParamStack::FParamStack()
	: LayerHash(1024)
{
	Layers.Reserve(8);
	EntryStack.Reserve(32);
	OwnedStackLayers.Reserve(8);
}

FParamStack::~FParamStack()
{
}

void FParamStack::SetParent(TWeakPtr<const FParamStack> InParent)
{
	WeakParentStack = InParent;
}

FParamStack& FParamStack::Get()
{
	return *GWeakStack.Pin().Get();
}

TWeakPtr<FParamStack> FParamStack::GetForCurrentThread()
{
	return GWeakStack;
}

void FParamStack::AddForPendingObject(const UObject* InObject, TWeakPtr<FParamStack> InStack)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_Write);
	if(!GPendingObjects.Contains(InObject))
	{
		GPendingObjects.Add(InObject, InStack);
	}
}

void FParamStack::RemoveForPendingObject(const UObject* InObject)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_Write);
	GPendingObjects.Remove(InObject);
}

bool FParamStack::AttachToCurrentThreadForPendingObject(const UObject* InObject, ECoalesce InCoalesce)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_ReadOnly);
	if (TWeakPtr<FParamStack>* PendingStack = GPendingObjects.Find(InObject))
	{
		AttachToCurrentThread(*PendingStack, InCoalesce);
		return true;
	}

	return false;
}

bool FParamStack::DetachFromCurrentThreadForPendingObject(const UObject* InObject, EDecoalesce InDecoalesce)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_ReadOnly);
	if (TWeakPtr<FParamStack>* PendingStack = GPendingObjects.Find(InObject))
	{
		DetachFromCurrentThread(InDecoalesce);
		return true;
	}

	return false;
}

void FParamStack::AttachToCurrentThread(TWeakPtr<FParamStack> InStack, ECoalesce InCoalesce)
{
	GWeakStack = InStack;
	
	if(InCoalesce == ECoalesce::Coalesce)
	{
		if(TSharedPtr<FParamStack> PinnedStack = GWeakStack.Pin())
		{
			PinnedStack->Coalesce();
		}
	}
}

TWeakPtr<FParamStack> FParamStack::DetachFromCurrentThread(EDecoalesce InDecoalesce)
{
	if(InDecoalesce == EDecoalesce::Decoalesce)
	{
		if(TSharedPtr<FParamStack> PinnedStack = GWeakStack.Pin())
		{
			PinnedStack->Decoalesce();
		}
	}
	
	TWeakPtr<FParamStack> Stack = GWeakStack;
	GWeakStack.Reset();
	return Stack;
}

FParamStack::FPushedLayerHandle FParamStack::PushLayer(const FParamStackLayerHandle& InLayerHandle)
{
	if (InLayerHandle.IsValid())
	{
		return PushLayerInternal(*InLayerHandle.Layer.Get());
	}

	return FPushedLayerHandle();
}

FParamStack::FPushedLayerHandle FParamStack::PushLayer(TConstArrayView<Private::FParamEntry> InParams)
{
	FParamStackLayer& OwnedLayer = OwnedStackLayers.Emplace_GetRef(InParams);
	OwnedLayer.OwnedStorageOffset = AllocAndCopyOwnedParamStorage(OwnedLayer.Params);
	OwnedLayer.OwningStack = this;
	return PushLayerInternal(OwnedLayer);
}

FParamStack::FPushedLayerHandle FParamStack::PushLayerInternal(FParamStackLayer& InLayer)
{
	if(InLayer.Params.Num() > 0)
	{
		FPushedLayer& NewPushedLayer = Layers.Emplace_GetRef(InLayer, *this);
		return FPushedLayerHandle(Layers.Num() - 1, NewPushedLayer.SerialNumber);
	}

	return FPushedLayerHandle();
}

void FParamStack::PopLayer(FPushedLayerHandle InHandle)
{
	if(InHandle.IsValid() && Layers.Num() > 0)
	{
		const FPushedLayer& TopLayer = Layers.Top();

		checkf(TopLayer.SerialNumber == InHandle.SerialNumber && (uint32)Layers.Num() - 1 == InHandle.Index, 
			TEXT("UE::AnimNext::FParamStack::PopLayer: Invalid layer handle supplied (Have: %u, %u, Expected: %u, %u)"), 
			InHandle.Index, InHandle.SerialNumber, (uint32)Layers.Num() - 1, TopLayer.SerialNumber);

		// Dont shrink allocs to avoid thrashing
		constexpr EAllowShrinking AllowShrinking = EAllowShrinking::No;

		// Remove params and indices from hash table
		const int32 NumLayerParams = TopLayer.Layer.Params.Num();
		int32 ParamStackIndex = EntryStack.Num() - 1;
		for(int32 ParamLayerIndex = 0; ParamLayerIndex < NumLayerParams; ++ParamLayerIndex)
		{
			LayerHash.Remove(EntryStack[ParamStackIndex]->GetHash(), ParamStackIndex);
			--ParamStackIndex;
		}

		EntryStack.RemoveAt(EntryStack.Num() - NumLayerParams, NumLayerParams, AllowShrinking);

		// If we own the layer, pop the owned stack
		if(TopLayer.Layer.OwnedStorageOffset != MAX_uint32 && TopLayer.Layer.OwningStack == this)
		{
			check(OwnedStackLayers.Num() && &TopLayer.Layer == &OwnedStackLayers[OwnedStackLayers.Num() - 1]);
			OwnedStackLayers.Pop(AllowShrinking);

			// Free any owned storage
			FreeOwnedParamStorage(TopLayer.Layer.OwnedStorageOffset);
		}

		// Pop the layer itself
		Layers.Pop(AllowShrinking);
	}
}

FParamStackLayerHandle FParamStack::MakeValueLayer(const FInstancedPropertyBag& InPropertyBag)
{
	FInstancedPropertyBag OwnedPropertyBag = InPropertyBag;
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FInstancedPropertyBagValueLayer>(MoveTemp(OwnedPropertyBag), true);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamStackLayerHandle FParamStack::MakeReferenceLayer(FInstancedPropertyBag& InPropertyBag)
{
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FInstancedPropertyBagReferenceLayer>(InPropertyBag, true);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamStackLayerHandle FParamStack::MakeRemappedLayer(const FParamStackLayerHandle& InLayer, const TMap<FName, FName>& InMapping)
{
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FRemappedLayer>(*InLayer.Layer.Get(), InMapping);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamStackLayerHandle FParamStack::MakeLayer(TConstArrayView<Private::FParamEntry> InParams)
{
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FParamStackLayer>(InParams);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamResult FParamStack::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility) const
{
	FParamTypeHandle ParamTypeHandle;
	return GetParamData(InId, InTypeHandle, OutParamData, ParamTypeHandle, InRequiredCompatibility);
}

FParamResult FParamStack::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_ParamStack_GetParam);

	const FParamResult Result = GetParamDataInternal(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if (Result.IsInScope())
	{
		return Result;
	}

	if(!bIsCoalesced)
	{
		if (TSharedPtr<const FParamStack> ParentStack = WeakParentStack.Pin())
		{
			return ParentStack->GetParamData(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
		}
	}

	return EParamResult::NotInScope;
}

FParamResult FParamStack::GetParamDataInternal(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	const Private::FParamEntry* ParamPtr = FindParam(InId);
	if (ParamPtr == nullptr)
	{
		return EParamResult::NotInScope;
	}

	const FParamResult Result = ParamPtr->GetParamData(InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if(Result.IsInScope())
	{
		return Result;
	}

	return EParamResult::NotInScope;
}

FParamResult FParamStack::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility)
{
	FParamTypeHandle ParamTypeHandle;
	return GetMutableParamData(InId, InTypeHandle, OutParamData, ParamTypeHandle, InRequiredCompatibility);
}

FParamResult FParamStack::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_ParamStack_GetParam);

	const FParamResult Result = GetMutableParamDataInternal(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if (Result.IsInScope())
	{
		return Result;
	}

	if(!bIsCoalesced)
	{
		if (TSharedPtr<const FParamStack> ParentStack = WeakParentStack.Pin())
		{
			// we use a dummy here because if the data is present in a parent, it must be immutable anyways and we will early out
			TConstArrayView<uint8> ParamData;
			FParamResult ParentResult = ParentStack->GetParamData(InId, InTypeHandle, ParamData, OutParamTypeHandle, InRequiredCompatibility);
			if (ParentResult.IsInScope())
			{
				// Parent data is immutable
				return ParentResult.Result & EParamResult::MutabilityError;
			}
		}
	}

	return EParamResult::NotInScope;
}

FParamResult FParamStack::GetMutableParamDataInternal(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility)
{
	Private::FParamEntry* ParamPtr = FindMutableParam(InId);
	if (ParamPtr == nullptr)
	{
		return EParamResult::NotInScope;
	}
	
	const FParamResult Result = ParamPtr->GetMutableParamData(InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if(Result.IsInScope())
	{
		return Result;
	}

	return EParamResult::NotInScope;
}

bool FParamStack::IsMutableParam(FParamId InId) const
{
	const Private::FParamEntry* ParamPtr = FindParam(InId);
	if (ParamPtr == nullptr)
	{
		return false;
	}
	
	return ParamPtr->IsMutable();
}

bool FParamStack::IsReferenceParam(FParamId InId) const
{
	const Private::FParamEntry* ParamPtr = FindParam(InId);
	if(ParamPtr == nullptr)
	{
		return false;
	}
	
	return ParamPtr->IsReference();
}

bool FParamStack::LayerContainsParam(const FParamStackLayerHandle& InHandle, FName InKey)
{
	const FParamId ParamIdToFind(InKey);
	const Private::FParamEntry* ParamPtr = InHandle.Layer->FindEntry(ParamIdToFind);
	return ParamPtr != nullptr;
}

uint32 FParamStack::AllocAndCopyOwnedParamStorage(TArrayView<Private::FParamEntry> InParams)
{
	const uint32 CurrentOffset = OwnedLayerParamStorage.Num();
	const uint32 PageSize = OwnedLayerParamStorage.MaxPerPage();

	for (Private::FParamEntry& Param : InParams)
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

	OwnedLayerParamStorage.SetNum(InOffset, EAllowShrinking::No);
}

uint32 FParamStack::MakeSerialNumber()
{
	++SerialNumber;
	if (SerialNumber == 0)
	{
		++SerialNumber;
	}
	return SerialNumber;
}

const Private::FParamEntry* FParamStack::FindParam(FParamId InId) const
{
	for(uint32 Index = LayerHash.First(InId.GetHash()); LayerHash.IsValid(Index); Index = LayerHash.Next(Index))
	{
		if (EntryStack[Index]->GetName() == InId.GetName())
		{
			return EntryStack[Index];
		}
	}
	return nullptr;
}
	
Private::FParamEntry* FParamStack::FindMutableParam(FParamId InId)
{
	return const_cast<Private::FParamEntry*>(FindParam(InId));
}

void FParamStack::Coalesce()
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_ParamStack_Coalesce);

	check(!bIsCoalesced);
	check(Layers.IsEmpty() && EntryStack.IsEmpty());

	// Find stacks to coalesce
	TArray<const FParamStack*, TInlineAllocator<8>> Stacks;
	if(WeakParentStack.IsValid())
	{
		int32 NumCoalescedLayers = 0;
		const FParamStack* RootStack = WeakParentStack.Pin().Get();
		while(RootStack)
		{
			NumCoalescedLayers += RootStack->Layers.Num();
			Stacks.Add(RootStack);
			RootStack = RootStack->WeakParentStack.Pin().Get();
		}

		CoalesceLayerHandles.Reserve(NumCoalescedLayers);
		Layers.Reserve(Layers.Num() + NumCoalescedLayers);
		for(int32 StackIndex = Stacks.Num() - 1; StackIndex >= 0; --StackIndex)
		{
			const FParamStack* Stack = Stacks[StackIndex];
			for(const FPushedLayer& PushedLayer : Stack->Layers)
			{
				CoalesceLayerHandles.Add(PushLayerInternal(PushedLayer.Layer));
			}
		}
	}

	bIsCoalesced = true;
}

void FParamStack::Decoalesce()
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_ParamStack_Decoalesce);

	check(bIsCoalesced);

	for(int32 LayerIndex = CoalesceLayerHandles.Num() - 1; LayerIndex >= 0; --LayerIndex)
	{
		PopLayer(CoalesceLayerHandles[LayerIndex]);
	}

	CoalesceLayerHandles.Reset();

	bIsCoalesced = false;
}

}

#undef LOCTEXT_NAMESPACE