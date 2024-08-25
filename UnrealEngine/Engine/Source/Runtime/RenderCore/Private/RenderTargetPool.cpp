// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderTargetPool.h"
#include "Hash/CityHash.h"
#include "Misc/CoreMisc.h"
#include "Trace/Trace.inl"
#include "ProfilingDebugging/CountersTrace.h"
#include "RHI.h"
#include "RenderCore.h"
#include "RHICommandList.h"

/** The global render targets pool. */
TGlobalResource<FRenderTargetPool> GRenderTargetPool;

DEFINE_LOG_CATEGORY_STATIC(LogRenderTargetPool, Warning, All);

CSV_DEFINE_CATEGORY(RenderTargetPool, !UE_SERVER);

TRACE_DECLARE_INT_COUNTER(RenderTargetPoolCount, TEXT("RenderTargetPool/Count"));
TRACE_DECLARE_MEMORY_COUNTER(RenderTargetPoolSize, TEXT("RenderTargetPool/Size"));

UE_TRACE_EVENT_BEGIN(Cpu, FRenderTargetPool_CreateTexture, NoSync)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
UE_TRACE_EVENT_END()

TRefCountPtr<IPooledRenderTarget> CreateRenderTarget(FRHITexture* Texture, const TCHAR* Name)
{
	check(Texture);

	FSceneRenderTargetItem Item;
	Item.TargetableTexture = Texture;
	Item.ShaderResourceTexture = Texture;

	FPooledRenderTargetDesc Desc = Translate(Texture->GetDesc());
	Desc.DebugName = Name;

	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
	GRenderTargetPool.CreateUntrackedElement(Desc, PooledRenderTarget, Item);
	return MoveTemp(PooledRenderTarget);
}

bool CacheRenderTarget(FRHITexture* Texture, const TCHAR* Name, TRefCountPtr<IPooledRenderTarget>& OutPooledRenderTarget)
{
	if (!OutPooledRenderTarget || OutPooledRenderTarget->GetRHI() != Texture)
	{
		OutPooledRenderTarget = CreateRenderTarget(Texture, Name);
		return true;
	}
	return false;
}

RENDERCORE_API void DumpRenderTargetPoolMemory(FOutputDevice& OutputDevice)
{
	GRenderTargetPool.DumpMemoryUsage(OutputDevice);
}

static FAutoConsoleCommandWithOutputDevice GDumpRenderTargetPoolMemoryCmd(
	TEXT("r.DumpRenderTargetPoolMemory"),
	TEXT("Dump allocation information for the render target pool."),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic(DumpRenderTargetPoolMemory)
);

static uint32 ComputeSizeInKB(FPooledRenderTarget& Element)
{
	return (Element.ComputeMemorySize() + 1023) / 1024;
}

TRefCountPtr<IPooledRenderTarget> FRenderTargetPool::FindFreeElement(FRHICommandListBase& RHICmdList, FRHITextureCreateInfo Desc, const TCHAR* Name)
{
	FPooledRenderTarget* Found = 0;
	uint32 FoundIndex = -1;

	// FastVRAM is no longer supported by the render target pool.
	EnumRemoveFlags(Desc.Flags, ETextureCreateFlags::FastVRAM | ETextureCreateFlags::FastVRAMPartialAlloc);

	// We always want SRV access
	Desc.Flags |= TexCreate_ShaderResource;

	// Render target pool always forces textures into non streaming memory.
	// UE-TODO: UE-188415 fix flags that we force in Render Target Pool
	//Desc.Flags |= ETextureCreateFlags::ForceIntoNonStreamingMemoryTracking;

	const uint32 DescHash = GetTypeHash(Desc);

	UE::TScopeLock Lock(Mutex);

	for (uint32 Index = 0, Num = (uint32)PooledRenderTargets.Num(); Index < Num; ++Index)
	{
		if (PooledRenderTargetHashes[Index] == DescHash)
		{
			FPooledRenderTarget* Element = PooledRenderTargets[Index];

		#if DO_CHECK
			{
				checkf(Element, TEXT("Hash was not cleared from the list."));

				const FRHITextureCreateInfo ElementDesc = Translate(Element->GetDesc());
				checkf(ElementDesc == Desc, TEXT("Invalid hash or collision when attempting to allocate %s"), Element->GetDesc().DebugName);
			}
		#endif

			if (Element->IsFree())
			{
				Found = Element;
				FoundIndex = Index;
				break;
			}
		}
	}

	if (!Found)
	{
#if CPUPROFILERTRACE_ENABLED
		UE_TRACE_LOG_SCOPED_T(Cpu, FRenderTargetPool_CreateTexture, CpuChannel)
			<< FRenderTargetPool_CreateTexture.Name(Name);
#endif

		const ERHIAccess AccessInitial = ERHIAccess::SRVMask;
		FRHITextureCreateDesc CreateDesc(Desc, AccessInitial, Name);
		const static FLazyName ClassName(TEXT("FPooledRenderTarget"));
		CreateDesc.SetClassName(ClassName);

		Found = new FPooledRenderTarget(
			RHICmdList.CreateTexture(CreateDesc),
			Translate(CreateDesc),
			this);

		PooledRenderTargets.Add(Found);
		PooledRenderTargetHashes.Add(DescHash);

		if (EnumHasAnyFlags(Desc.Flags, TexCreate_UAV))
		{
			// The render target desc is invalid if a UAV is requested with an RHI that doesn't support the high-end feature level.
			check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5 || GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1);

			if (GRHISupportsUAVFormatAliasing)
			{
				EPixelFormat AliasFormat = Desc.UAVFormat != PF_Unknown
					? Desc.UAVFormat
					: Desc.Format;

				Found->RenderTargetItem.UAV = RHICmdList.CreateUnorderedAccessView(Found->GetRHI(), 0, (uint8)AliasFormat, 0, 0);
			}
			else
			{
				checkf(Desc.UAVFormat == PF_Unknown || Desc.UAVFormat == Desc.Format, TEXT("UAV aliasing is not supported by the current RHI."));
				Found->RenderTargetItem.UAV = RHICmdList.CreateUnorderedAccessView(Found->GetRHI(), 0);
			}
		}

		AllocationLevelInKB += ComputeSizeInKB(*Found);
		TRACE_COUNTER_ADD(RenderTargetPoolCount, 1);
		TRACE_COUNTER_SET(RenderTargetPoolSize, (int64)AllocationLevelInKB * 1024);

		FoundIndex = PooledRenderTargets.Num() - 1;
		Found->Desc.DebugName = Name;
	}

	Found->Desc.DebugName = Name;
	Found->UnusedForNFrames = 0;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RHICmdList.BindDebugLabelName(Found->GetRHI(), Name);
#endif

	return TRefCountPtr<IPooledRenderTarget>(MoveTemp(Found));
}

bool FRenderTargetPool::FindFreeElement(FRHICommandListBase& RHICmdList, const FRHITextureCreateInfo& Desc, TRefCountPtr<IPooledRenderTarget>& Out, const TCHAR* Name)
{
	if (!Desc.IsValid())
	{
		// no need to do anything
		return true;
	}

	// Querying a render target that have no mip levels makes no sens.
	check(Desc.NumMips > 0);

	FPooledRenderTarget* Current = nullptr;
	bool bFreeCurrent = false;

	// if we can keep the current one, do that
	if (Out)
	{
		Current = (FPooledRenderTarget*)Out.GetReference();

		if (Translate(Out->GetDesc()) == Desc)
		{
			// we can reuse the same, but the debug name might have changed
			Current->Desc.DebugName = Name;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (Current->GetRHI())
			{
				RHICmdList.BindDebugLabelName(Current->GetRHI(), Name);
			}
#endif
			check(!Out->IsFree());
			return true;
		}
		else
		{
			// release old reference, it might free a RT we can use
			Out = 0;
			bFreeCurrent = Current->IsFree();
		}
	}

	UE::TScopeLock Lock(Mutex);

	if (bFreeCurrent)
	{
		AllocationLevelInKB -= ComputeSizeInKB(*Current);
		TRACE_COUNTER_SUBTRACT(RenderTargetPoolCount, 1);
		TRACE_COUNTER_SET(RenderTargetPoolSize, (int64)AllocationLevelInKB * 1024);
		int32 Index = FindIndex(Current);
		check(Index >= 0);
		FreeElementAtIndex(Index);
	}

	Out = FindFreeElement(RHICmdList, Desc, Name);
	return false;
}

void FRenderTargetPool::CreateUntrackedElement(const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget>& Out, const FSceneRenderTargetItem& Item)
{
	FPooledRenderTarget* Result = new FPooledRenderTarget(Item.GetRHI(), Desc, nullptr);
	Result->RenderTargetItem = Item;
	Out = Result;
}

void FRenderTargetPool::GetStats(uint32& OutWholeCount, uint32& OutWholePoolInKB, uint32& OutUsedInKB) const
{
	UE::TScopeLock Lock(Mutex);
	OutWholeCount = (uint32)PooledRenderTargets.Num();
	OutUsedInKB = 0;
	OutWholePoolInKB = 0;

	for (uint32 i = 0; i < (uint32)PooledRenderTargets.Num(); ++i)
	{
		FPooledRenderTarget* Element = PooledRenderTargets[i];

		if (Element)
		{
			uint32 SizeInKB = ComputeSizeInKB(*Element);

			OutWholePoolInKB += SizeInKB;

			if (!Element->IsFree())
			{
				OutUsedInKB += SizeInKB;
			}
		}
	}

	// if this triggers uncomment the code in VerifyAllocationLevel() and debug the issue, we might leak memory or not release when we could
	ensure(AllocationLevelInKB == OutWholePoolInKB);
}

void FRenderTargetPool::TickPoolElements()
{
	UE::TScopeLock Lock(Mutex);

	uint32 DeferredAllocationLevelInKB = 0;
	for (FPooledRenderTarget* Element : DeferredDeleteArray)
	{
		DeferredAllocationLevelInKB += ComputeSizeInKB(*Element);
	}

	DeferredDeleteArray.Reset();

	uint32 MinimumPoolSizeInKB;
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RenderTargetPoolMin"));

		MinimumPoolSizeInKB = FMath::Clamp(CVar->GetValueOnRenderThread(), 0, 2000) * 1024;
	}

	CompactPool();

	uint32 UnusedAllocationLevelInKB = 0;
	for (uint32 i = 0; i < (uint32)PooledRenderTargets.Num(); ++i)
	{
		FPooledRenderTarget* Element = PooledRenderTargets[i];

		if (Element)
		{
			Element->OnFrameStart();
			if (Element->UnusedForNFrames > 2)
			{
				UnusedAllocationLevelInKB += ComputeSizeInKB(*Element);
			}
		}
	}

	uint32 TotalFrameUsageInKb = AllocationLevelInKB + DeferredAllocationLevelInKB ;

	CSV_CUSTOM_STAT(RenderTargetPool, UnusedMB, UnusedAllocationLevelInKB / 1024.0f, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RenderTargetPool, PeakUsedMB, (TotalFrameUsageInKb - UnusedAllocationLevelInKB) / 1024.f, ECsvCustomStatOp::Set);
	
	// we need to release something, take the oldest ones first
	while (AllocationLevelInKB > MinimumPoolSizeInKB)
	{
		// -1: not set
		int32 OldestElementIndex = -1;

		// find oldest element we can remove
		for (uint32 i = 0, Num = (uint32)PooledRenderTargets.Num(); i < Num; ++i)
		{
			FPooledRenderTarget* Element = PooledRenderTargets[i];

			if (Element && Element->UnusedForNFrames > 2)
			{
				if (OldestElementIndex != -1)
				{
					if (PooledRenderTargets[OldestElementIndex]->UnusedForNFrames < Element->UnusedForNFrames)
					{
						OldestElementIndex = i;
					}
				}
				else
				{
					OldestElementIndex = i;
				}
			}
		}

		if (OldestElementIndex != -1)
		{
			AllocationLevelInKB -= ComputeSizeInKB(*PooledRenderTargets[OldestElementIndex]);
			TRACE_COUNTER_SUBTRACT(RenderTargetPoolCount, 1);
			TRACE_COUNTER_SET(RenderTargetPoolSize, (int64)AllocationLevelInKB * 1024);

			// we assume because of reference counting the resource gets released when not needed any more
			// we don't use Remove() to not shuffle around the elements for better transparency on RenderTargetPoolEvents
			FreeElementAtIndex(OldestElementIndex);
		}
		else
		{
			// There is no element we can remove but we are over budget, better we log that.
			// Options:
			//   * Increase the pool
			//   * Reduce rendering features or resolution
			//   * Investigate allocations, order or reusing other render targets can help
			//   * Ignore (editor case, might start using slow memory which can be ok)
			if (!bCurrentlyOverBudget)
			{
				UE_CLOG(IsRunningClientOnly() && MinimumPoolSizeInKB != 0, LogRenderTargetPool, Warning, TEXT("r.RenderTargetPoolMin exceeded %d/%d MB (ok in editor, bad on fixed memory platform)"), (AllocationLevelInKB + 1023) / 1024, MinimumPoolSizeInKB / 1024);
				bCurrentlyOverBudget = true;
			}
			// at this point we need to give up
			break;
		}
	}

	if (AllocationLevelInKB <= MinimumPoolSizeInKB)
	{
		if (bCurrentlyOverBudget)
		{
			UE_CLOG(MinimumPoolSizeInKB != 0, LogRenderTargetPool, Display, TEXT("r.RenderTargetPoolMin resolved %d/%d MB"), (AllocationLevelInKB + 1023) / 1024, MinimumPoolSizeInKB / 1024);
			bCurrentlyOverBudget = false;
		}
	}

#if STATS
	uint32 Count, SizeKB, UsedKB;
	GetStats(Count, SizeKB, UsedKB);
	CSV_CUSTOM_STAT_GLOBAL(RenderTargetPoolSize, float(SizeKB) / 1024.0f, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_GLOBAL(RenderTargetPoolUsed, float(UsedKB) / 1024.0f, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_GLOBAL(RenderTargetPoolCount, int32(Count), ECsvCustomStatOp::Set);
	SET_MEMORY_STAT(STAT_RenderTargetPoolSize, int64(SizeKB) * 1024ll);
	SET_MEMORY_STAT(STAT_RenderTargetPoolUsed, int64(UsedKB) * 1024ll);
	SET_DWORD_STAT(STAT_RenderTargetPoolCount, Count);
#endif // STATS
}

int32 FRenderTargetPool::FindIndex(IPooledRenderTarget* In) const
{
	UE::TScopeLock Lock(Mutex);

	if (In)
	{
		for (uint32 i = 0, Num = (uint32)PooledRenderTargets.Num(); i < Num; ++i)
		{
			const FPooledRenderTarget* Element = PooledRenderTargets[i];

			if (Element == In)
			{
				return i;
			}
		}
	}

	// not found
	return -1;
}

void FRenderTargetPool::FreeElementAtIndex(int32 Index)
{
	// we don't use Remove() to not shuffle around the elements for better transparency on RenderTargetPoolEvents
	PooledRenderTargets[Index] = 0;
	PooledRenderTargetHashes[Index] = 0;
}

void FRenderTargetPool::FreeUnusedResource(TRefCountPtr<IPooledRenderTarget>& In)
{
	UE::TScopeLock Lock(Mutex);

	int32 Index = FindIndex(In);

	if (Index != -1)
	{
		FPooledRenderTarget* Element = PooledRenderTargets[Index];

		// Ref count will always be at least 2
		ensure(Element->GetRefCount() >= 2);
		In = nullptr;

		if (Element->IsFree())
		{
			AllocationLevelInKB -= ComputeSizeInKB(*Element);
			TRACE_COUNTER_SUBTRACT(RenderTargetPoolCount, 1);
			TRACE_COUNTER_SET(RenderTargetPoolSize, (int64)AllocationLevelInKB * 1024);

			// we assume because of reference counting the resource gets released when not needed any more
			DeferredDeleteArray.Add(PooledRenderTargets[Index]);
			FreeElementAtIndex(Index);
		}
	}
}

void FRenderTargetPool::FreeUnusedResources()
{
	UE::TScopeLock Lock(Mutex);

	for (uint32 i = 0, Num = (uint32)PooledRenderTargets.Num(); i < Num; ++i)
	{
		FPooledRenderTarget* Element = PooledRenderTargets[i];

		if (Element && Element->IsFree())
		{
			AllocationLevelInKB -= ComputeSizeInKB(*Element);
			TRACE_COUNTER_SUBTRACT(RenderTargetPoolCount, 1);
			TRACE_COUNTER_SET(RenderTargetPoolSize, (int64)AllocationLevelInKB * 1024);

			// we assume because of reference counting the resource gets released when not needed any more
			// we don't use Remove() to not shuffle around the elements for better transparency on RenderTargetPoolEvents
			DeferredDeleteArray.Add(PooledRenderTargets[i]);
			FreeElementAtIndex(i);
		}
	}
}

void FRenderTargetPool::DumpMemoryUsage(FOutputDevice& OutputDevice)
{
	UE::TScopeLock Lock(Mutex);

	uint32 UnusedAllocationInKB = 0;

	OutputDevice.Logf(TEXT("Pooled Render Targets:"));
	for (int32 i = 0; i < PooledRenderTargets.Num(); ++i)
	{
		FPooledRenderTarget* Element = PooledRenderTargets[i];

		if (Element)
		{
			uint32 ElementAllocationInKB = ComputeSizeInKB(*Element);
			if (Element->UnusedForNFrames > 2)
			{
				UnusedAllocationInKB += ElementAllocationInKB;
			}

			OutputDevice.Logf(
				TEXT("  %6.3fMB %4dx%4d%s%s %2dmip(s) %s (%s) Unused frames: %d"),
				ElementAllocationInKB / 1024.0f,
				Element->Desc.Extent.X,
				Element->Desc.Extent.Y,
				Element->Desc.Depth > 1 ? *FString::Printf(TEXT("x%3d"), Element->Desc.Depth) : (Element->Desc.IsCubemap() ? TEXT("cube") : TEXT("    ")),
				Element->Desc.bIsArray ? *FString::Printf(TEXT("[%3d]"), Element->Desc.ArraySize) : TEXT("     "),
				Element->Desc.NumMips,
				Element->Desc.DebugName,
				GPixelFormats[Element->Desc.Format].Name,
				Element->UnusedForNFrames
			);
		}
	}
	uint32 NumTargets = 0;
	uint32 UsedKB = 0;
	uint32 PoolKB = 0;
	GetStats(NumTargets, PoolKB, UsedKB);
	OutputDevice.Logf(TEXT("%.3fMB total, %.3fMB used, %.3fMB unused, %d render targets"), PoolKB / 1024.f, UsedKB / 1024.f, UnusedAllocationInKB / 1024.f, NumTargets);

	uint32 DeferredTotal = 0;
	OutputDevice.Logf(TEXT("Deferred Render Targets:"));
	for (int32 i = 0; i < DeferredDeleteArray.Num(); ++i)
	{
		FPooledRenderTarget* Element = DeferredDeleteArray[i];

		if (Element)
		{
			OutputDevice.Logf(
				TEXT("  %6.3fMB %4dx%4d%s%s %2dmip(s) %s (%s)"),
				ComputeSizeInKB(*Element) / 1024.0f,
				Element->Desc.Extent.X,
				Element->Desc.Extent.Y,
				Element->Desc.Depth > 1 ? *FString::Printf(TEXT("x%3d"), Element->Desc.Depth) : (Element->Desc.IsCubemap() ? TEXT("cube") : TEXT("    ")),
				Element->Desc.bIsArray ? *FString::Printf(TEXT("[%3d]"), Element->Desc.ArraySize) : TEXT("     "),
				Element->Desc.NumMips,
				Element->Desc.DebugName,
				GPixelFormats[Element->Desc.Format].Name
			);
			uint32 SizeInKB = ComputeSizeInKB(*Element);
			DeferredTotal += SizeInKB;
		}
	}
	OutputDevice.Logf(TEXT("%.3fMB Deferred total"), DeferredTotal / 1024.f);
}

void FRenderTargetPool::ReleaseRHI()
{
	UE::TScopeLock Lock(Mutex);
	DeferredDeleteArray.Empty();
	PooledRenderTargets.Empty();
}

// for debugging purpose
FPooledRenderTarget* FRenderTargetPool::GetElementById(uint32 Id) const
{
	// is used in game and render thread

	if (Id >= (uint32)PooledRenderTargets.Num())
	{
		return 0;
	}

	return PooledRenderTargets[Id];
}

void FRenderTargetPool::CompactPool()
{
	for (uint32 i = 0, Num = (uint32)PooledRenderTargets.Num(); i < Num; )
	{
		FPooledRenderTarget* Element = PooledRenderTargets[i];

		if (!Element)
		{
			PooledRenderTargets.RemoveAtSwap(i);
			PooledRenderTargetHashes.RemoveAtSwap(i);
			--Num;
		}
		else
		{
			++i;
		}
	}
}

bool FPooledRenderTarget::OnFrameStart()
{
	// If there are any references to the pooled render target other than the pool itself, then it may not be freed.
	if (!IsFree())
	{
		check(!UnusedForNFrames);
		return false;
	}

	++UnusedForNFrames;

	// this logic can be improved
	if (UnusedForNFrames > 10)
	{
		// release
		return true;
	}

	return false;
}

uint32 FPooledRenderTarget::ComputeMemorySize() const
{
	uint32 Size = 0;
	if (Desc.Is2DTexture())
	{
		Size += RHIComputeMemorySize(RenderTargetItem.TargetableTexture);
		if (RenderTargetItem.ShaderResourceTexture != RenderTargetItem.TargetableTexture)
		{
			Size += RHIComputeMemorySize(RenderTargetItem.ShaderResourceTexture);
		}
	}
	else if (Desc.Is3DTexture())
	{
		Size += RHIComputeMemorySize(RenderTargetItem.TargetableTexture);
		if (RenderTargetItem.ShaderResourceTexture != RenderTargetItem.TargetableTexture)
		{
			Size += RHIComputeMemorySize(RenderTargetItem.ShaderResourceTexture);
		}
	}
	else
	{
		Size += RHIComputeMemorySize(RenderTargetItem.TargetableTexture);
		if (RenderTargetItem.ShaderResourceTexture != RenderTargetItem.TargetableTexture)
		{
			Size += RHIComputeMemorySize(RenderTargetItem.ShaderResourceTexture);
		}
	}
	return Size;
}

bool FPooledRenderTarget::IsFree() const
{
	uint32 RefCount = GetRefCount();
	check(RefCount >= 1);

	// If the only reference to the pooled render target is from the pool, then it's unused.
	return RefCount == 1;
}
