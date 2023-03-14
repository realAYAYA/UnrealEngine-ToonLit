// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectCacheContext.h"
#include "UObject/UObjectIterator.h"
#include "UObject/ObjectKey.h"
#include "Materials/MaterialInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Misc/ScopeRWLock.h"
#include "Logging/LogCategory.h"
#include "Templates/UniquePtr.h"

#define LOCTEXT_NAMESPACE "ObjectCache"

DECLARE_LOG_CATEGORY_EXTERN(LogObjectCache, Log, All);
DEFINE_LOG_CATEGORY(LogObjectCache);

#if WITH_EDITOR
#include "ObjectCacheEventSink.h"
#include "Containers/LockFreeList.h"
#include "Containers/LockFreeFixedSizeAllocator.h"

static TAutoConsoleVariable<int32> CVarObjectReverseLookupMode(
	TEXT("Editor.ObjectReverseLookupMode"),
	1,
	TEXT("0 - Reverse lookup tables are computed every time they are needed (slower behavior) \n")
	TEXT("1 - Maintain permanent reverse lookup tables (faster behavior) \n")
	TEXT("2 - Comparison mode (slowest to do validation between both mode) \n"),
	ECVF_ReadOnly /* Can only be enabled from the command-line */
);

namespace ObjectCacheContextImpl { void Validate(); }

static FAutoConsoleCommand CVarObjectReverseLookupValidate(
	TEXT("Editor.ObjectReverseLookupValidate"),
	TEXT("Compare objects contained in the reverse lookup against the old scanning method to see if there is any discrepenties."),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			ObjectCacheContextImpl::Validate();
		}
	)
);

enum class EObjectReverseLookupMode
{
	Temporary = 0,
	Permanent = 1,
	Comparison = 2
};

EObjectReverseLookupMode GetObjectReverseLookupMode()
{
	static bool bInitialized = false;
	if (!bInitialized)
	{
		check(IsInGameThread());
		int32 Mode = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("-ObjectReverseLookupMode="), Mode))
		{
			CVarObjectReverseLookupMode->Set(Mode, ECVF_SetByCommandline);
		}
		bInitialized = true;
	}

	return (EObjectReverseLookupMode)CVarObjectReverseLookupMode.GetValueOnAnyThread();
}

template <typename FromType, typename ToType>
class FObjectReverseLookupCache
{
public:
	void RemoveFrom(FromType* InFrom)
	{
		FWriteScopeLock ScopeLock(Lock);

		TRawSet<ToType*>* ToObjects = FromToMapping.Find(InFrom);
		if (ToObjects)
		{
			for (ToType* ToObject : *ToObjects)
			{
				TRawSet<FromType*>* FromObjects = ToFromMapping.Find(ToObject);
				if (FromObjects)
				{
					UE_LOG(LogObjectCache, VeryVerbose, TEXT("Lookup mapping removed %s -> %s"), *InFrom->GetFullName(), *ToObject->GetFullName());
					FromObjects->Remove(InFrom);
				}
			}

			FromToMapping.Remove(InFrom);
		}
	}

	void Update(ToType* InTo, const TArray<FromType*>& NewFromObjects)
	{
		FWriteScopeLock ScopeLock(Lock);

		// Remove the old reverse lookup mappings as they can't be read from the
		// objects anymore.
		TRawSet<FromType*>* OldFromObjects = ToFromMapping.Find(InTo);
		if (OldFromObjects)
		{
			for (FromType* OldFrom : *OldFromObjects)
			{
				TRawSet<ToType*>* ToObjects = FromToMapping.Find(OldFrom);
				if (ToObjects)
				{
					ToObjects->Remove(InTo);
				}
			}
		}

		if (NewFromObjects.Num())
		{
			TRawSet<FromType*>& FromObjects = ToFromMapping.FindOrAdd(InTo);
			FromObjects.Reset();

			for (FromType* From : NewFromObjects)
			{
				UE_LOG(LogObjectCache, VeryVerbose, TEXT("Lookup mapping added %s -> %s"), *From->GetFullName(), *InTo->GetFullName());
				FromToMapping.FindOrAdd(From).Add(InTo);
				FromObjects.Add(From);
			}
		}
		else
		{
			ToFromMapping.Remove(InTo);
		}
	}

	TObjectCacheIterator<ToType> GetFrom(FromType* InFrom) const
	{
		FReadScopeLock ScopeLock(Lock);

		// Until we can get some kind of thread-safe CopyOnWrite behavior
		// we need to extract a copy to avoid race conditions.
		TArray<ToType*> OutTo;
		if (const TRawSet<ToType*>* Set = FromToMapping.Find(InFrom))
		{
			OutTo.Reserve(Set->Num());
			for (ToType* To : *Set)
			{
				OutTo.Add(To);
			}
		}

		return TObjectCacheIterator<ToType>(MoveTemp(OutTo));
	}

	TObjectCacheIterator<FromType> GetTo(ToType* InTo) const
	{
		FReadScopeLock ScopeLock(Lock);

		// Until we can get some kind of thread-safe CopyOnWrite behavior, 
		// we need to extract a copy to avoid race conditions.
		TArray<FromType*> OutFrom;
		if (const TRawSet<FromType*>* Set = ToFromMapping.Find(InTo))
		{
			OutFrom.Reserve(Set->Num());
			for (FromType* From : *Set)
			{
				OutFrom.Add(From);
			}
		}

		return TObjectCacheIterator<FromType>(MoveTemp(OutFrom));
	}

	int32 Compare(const FObjectReverseLookupCache<FromType, ToType>& Other)
	{
		FWriteScopeLock ScopeLockA(Other.Lock);
		FWriteScopeLock ScopeLockB(Lock);

		int32 ErrorCount = 0;
		for (const auto& Kvp : FromToMapping)
		{
			const TRawSet<ToType*>* ToObjects = Other.FromToMapping.Find(Kvp.Key);
			for (const auto& To : Kvp.Value)
			{
				bool bIsPresent = ToObjects ? ToObjects->Contains(To) : false;
				if (!bIsPresent)
				{
					UE_LOG(LogObjectCache, Display, TEXT("Missing a direct lookup from %s to %s mapping"), *Kvp.Key->GetFullName(), *To->GetFullName());
					ErrorCount++;
				}
			}
		}

		for (const auto& Kvp : ToFromMapping)
		{
			const TRawSet<FromType*>* FromObjects = Other.ToFromMapping.Find(Kvp.Key);
			for (const auto& From : Kvp.Value)
			{
				bool bIsPresent = FromObjects ? FromObjects->Contains(From) : false;
				if (!bIsPresent)
				{
					UE_LOG(LogObjectCache, Display, TEXT("Missing a reverse lookup from %s to %s mapping"), *Kvp.Key->GetFullName(), *From->GetFullName());
					ErrorCount++;
				}
			}
		}

		return ErrorCount;
	}
private:
	mutable FRWLock Lock;

	// We use raw pointer to compare UObject here both for performance reason
	// and because we can't use TObjectKey. Some objects are reallocated in-place
	// with a different serial number but without changing their pointer.
	// See: StaticAllocateObject for implementation detail.
	// This trick is used to avoid having to track down all referencers of a UObject*
	// just to update it, and is used for instance to replace the content of a UStaticMesh
	// during a reimport without having to update any UStaticMeshComponent.
	// Because it is the job of this class to maintain pointer to pointer lookups, we
	// can't use the serial number of any object in our map, and by extention, we can't
	// use TObjectKey nor FObjectKey to store our keys.
	// We also assume that any UObject pointer in these maps are always valid
	// because we require to be notified when those objects are destroyed.
	template<typename ElementType>
	struct TRawObjectKeyFuncs : BaseKeyFuncs<ElementType, void*, false>
	{
		typedef typename TTypeTraits<void*>::ConstPointerType KeyInitType;
		typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element) {	return (void*)Element; }
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B) { return A == B; }
		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key) { return GetTypeHash(Key); }
	}; 
	
	template <typename InElementType>
	using TRawSet = TSet<InElementType, TRawObjectKeyFuncs<InElementType>>;

	template <typename InKeyType, typename InValueType>
	using TRawMap = TMap<InKeyType, InValueType, FDefaultSetAllocator, TDefaultMapKeyFuncs<void*, InValueType, false>>;

	TRawMap<FromType*, TRawSet<ToType*>> FromToMapping;
	TRawMap<ToType*, TRawSet<FromType*>> ToFromMapping;
};

FObjectReverseLookupCache<UTexture, UMaterialInterface>            GTextureToMaterialLookupCache;
FObjectReverseLookupCache<UStaticMesh, UStaticMeshComponent>       GStaticMeshToComponentLookupCache;
FObjectReverseLookupCache<UMaterialInterface, UPrimitiveComponent> GMaterialToPrimitiveLookupCache;

namespace ObjectCacheContextImpl {

	void GetReferencedTextures(UMaterialInterface* MaterialInterface, TSet<UTexture*>& OutReferencedTextures);
	void Validate()
	{
		// Scan and compare UStaticMesh -> UStaticMeshComponent
		int32 ErrorCount = 0;
		{
			FObjectReverseLookupCache<UStaticMesh, UStaticMeshComponent> TempLookup;
			for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
			{
				if (It->GetStaticMesh())
				{
					TempLookup.Update(*It, { It->GetStaticMesh() } );
				}
			}
			ErrorCount += TempLookup.Compare(GStaticMeshToComponentLookupCache);
		}

		// Scan and compare UTexture -> UMaterialInterface
		{
			FObjectReverseLookupCache<UTexture, UMaterialInterface> TempLookup;
			for (TObjectIterator<UMaterialInterface> It; It; ++It)
			{
				TSet<UTexture*> ReferencedTextures;
				GetReferencedTextures(*It, ReferencedTextures);
				TempLookup.Update(*It, ReferencedTextures.Array());
			}
			ErrorCount += TempLookup.Compare(GTextureToMaterialLookupCache);
		}

		// Scan and compare UMaterialInterface -> UPrimitiveComponent
		{
			FObjectReverseLookupCache<UMaterialInterface, UPrimitiveComponent> TempLookup;
			for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
			{
				UPrimitiveComponent* Component = *It;
				if (Component->IsRenderStateCreated() && Component->SceneProxy && !Component->IsRenderStateDirty())
				{
					TArray<UMaterialInterface*> UsedMaterials;
					Component->GetUsedMaterials(UsedMaterials);
					TempLookup.Update(Component, UsedMaterials);
				}
			}
			ErrorCount += TempLookup.Compare(GMaterialToPrimitiveLookupCache);
		}

		UE_LOG(LogObjectCache, Display, TEXT("Object Cache verification found %d discrepenties"), ErrorCount);
	}

} // namespace ObjectCacheContextImpl

#endif // #if WITH_EDITOR

namespace ObjectCacheContextImpl {

	void GetReferencedTextures(UMaterialInterface* MaterialInterface, TSet<UTexture*>& OutReferencedTextures)
	{
		TSet<UTexture*> Textures;
		for (UObject* TextureObject : MaterialInterface->GetReferencedTextures())
		{
			UTexture* Texture = Cast<UTexture>(TextureObject);
			if (Texture)
			{
				OutReferencedTextures.Add(Texture);
			}
		}

		// GetReferencedTextures() doesn't return overrides anymore and we need all referenced textures.
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);
		while (MaterialInstance)
		{
			for (const FTextureParameterValue& TextureParam : MaterialInstance->TextureParameterValues)
			{
				if (TextureParam.ParameterValue)
				{
					OutReferencedTextures.Add(TextureParam.ParameterValue);
				}
			}

			MaterialInstance = Cast<UMaterialInstance>(MaterialInstance->Parent);
		}
	}

} // namespace ObjectCacheContextImpl

TObjectCacheIterator<UPrimitiveComponent> FObjectCacheContext::GetPrimitiveComponents()
{
	if (!PrimitiveComponents.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputePrimitiveComponents);

		TArray<UPrimitiveComponent*> Array;
		Array.Reserve(4096);
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			Array.Add(*It);
		}
		PrimitiveComponents = MoveTemp(Array);
	}

	return TObjectCacheIterator<UPrimitiveComponent>(MakeArrayView(PrimitiveComponents.GetValue()));
}

TObjectCacheIterator<UStaticMeshComponent> FObjectCacheContext::GetStaticMeshComponents()
{
	if (!StaticMeshComponents.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeStaticMeshComponents);

		TArray<UStaticMeshComponent*> Array;
		Array.Reserve(4096);
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			Array.Add(*It);
		}
		StaticMeshComponents = MoveTemp(Array);
	}
	return TObjectCacheIterator<UStaticMeshComponent>(MakeArrayView(StaticMeshComponents.GetValue()));
}

TObjectCacheIterator<UTexture> FObjectCacheContext::GetUsedTextures(UMaterialInterface* MaterialInterface)
{
#if WITH_EDITOR
	const EObjectReverseLookupMode ObjectCacheContextMode = GetObjectReverseLookupMode();
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Permanent)
	{
		return GTextureToMaterialLookupCache.GetTo(MaterialInterface);
	}
#endif

	TSet<UTexture*>* Textures = MaterialUsedTextures.Find(MaterialInterface);
	if (Textures == nullptr)
	{
		Textures = &MaterialUsedTextures.Add(MaterialInterface);
		ObjectCacheContextImpl::GetReferencedTextures(MaterialInterface, *Textures);
	}

#if WITH_EDITOR
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Comparison)
	{
		TSet<UTexture*> LookupResult;
		for (UTexture* Texture : GTextureToMaterialLookupCache.GetTo(MaterialInterface))
		{
			LookupResult.Add(Texture);
			checkf(Textures->Contains(Texture), TEXT("Permanent Object Cache has an additionnal texture %s on material %s"), *Texture->GetFullName(), *MaterialInterface->GetFullName());
		}

		for (UTexture* Texture : *Textures)
		{
			checkf(LookupResult.Contains(Texture), TEXT("Permanent Object Cache is missing a texture %s on material %s"), *Texture->GetFullName(), *MaterialInterface->GetFullName());
		}
	}
#endif

	return TObjectCacheIterator<UTexture>(Textures->Array());
}

TObjectCacheIterator<USkinnedMeshComponent> FObjectCacheContext::GetSkinnedMeshComponents()
{
	if (!SkinnedMeshComponents.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeSkinnedMeshComponents);

		TArray<USkinnedMeshComponent*> Array;
		Array.Reserve(4096);
		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
		{
			Array.Add(*It);
		}
		SkinnedMeshComponents = MoveTemp(Array);
	}
	return TObjectCacheIterator<USkinnedMeshComponent>(MakeArrayView(SkinnedMeshComponents.GetValue()));
}


TObjectCacheIterator<UMaterialInterface> FObjectCacheContext::GetUsedMaterials(UPrimitiveComponent* Component)
{
#if WITH_EDITOR
	const EObjectReverseLookupMode ObjectCacheContextMode = GetObjectReverseLookupMode();
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Permanent)
	{
		return GMaterialToPrimitiveLookupCache.GetTo(Component);
	}
#endif

	TSet<UMaterialInterface*>* Materials = PrimitiveComponentToMaterial.Find(Component);
	if (Materials == nullptr)
	{
		Materials = &PrimitiveComponentToMaterial.Add(Component);
		TArray<UMaterialInterface*> OutMaterials;
		Component->GetUsedMaterials(OutMaterials, true);
		Materials->Append(OutMaterials);
	}

#if WITH_EDITOR
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Comparison && Component->IsRenderStateCreated() && Component->SceneProxy && !Component->IsRenderStateDirty())
	{
		TSet<UMaterialInterface*> LookupResult;
		for (UMaterialInterface* Material : GMaterialToPrimitiveLookupCache.GetTo(Component))
		{
			LookupResult.Add(Material);
			if (!Materials->Contains(Material))
			{
				UE_LOG(LogObjectCache, Warning, TEXT("Permanent Object Cache has an additionnal material %s for component %"), *Material->GetFullName(), *Component->GetFullName());
			}
		}

		for (UMaterialInterface* Material : *Materials)
		{
			if (!LookupResult.Contains(Material))
			{
				UE_LOG(LogObjectCache, Warning, TEXT("Permanent Object Cache is missing a material %s on component %s"), *Material->GetFullName(), *Component->GetFullName());
			}
		}
	}
#endif

	return TObjectCacheIterator<UMaterialInterface>(Materials->Array());
}

TObjectCacheIterator<UStaticMeshComponent> FObjectCacheContext::GetStaticMeshComponents(UStaticMesh* InStaticMesh)
{
#if WITH_EDITOR
	const EObjectReverseLookupMode ObjectCacheContextMode = GetObjectReverseLookupMode();
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Permanent)
	{
		return GStaticMeshToComponentLookupCache.GetFrom(InStaticMesh);
	}
#endif

	if (!StaticMeshToComponents.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeStaticMeshToComponents);

		TMap<TObjectKey<UStaticMesh>, TSet<UStaticMeshComponent*>> TempMap;
		TempMap.Reserve(8192);
		for (UStaticMeshComponent* Component : GetStaticMeshComponents())
		{
			TempMap.FindOrAdd(Component->GetStaticMesh()).Add(Component);
		}
		StaticMeshToComponents = MoveTemp(TempMap);
	}

	static TSet<UStaticMeshComponent*> EmptySet;
	TSet<UStaticMeshComponent*>* Set = StaticMeshToComponents.GetValue().Find(InStaticMesh);
	const TSet<UStaticMeshComponent*>& ComputeResult = Set ? *Set : EmptySet;

#if WITH_EDITOR
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Comparison)
	{
		TSet<UStaticMeshComponent*> LookupResult;
		for (UStaticMeshComponent* Component : GStaticMeshToComponentLookupCache.GetFrom(InStaticMesh))
		{
			LookupResult.Add(Component);
			checkf(ComputeResult.Contains(Component), TEXT("Permanent Object Cache has an additionnal component %s for staticmesh %s"), *Component->GetFullName(), *InStaticMesh->GetFullName());
		}

		for (UStaticMeshComponent* Component : ComputeResult)
		{
			checkf(LookupResult.Contains(Component), TEXT("Permanent Object Cache is missing a component %p %s for staticmesh %p %s"), Component , *Component->GetFullName(), InStaticMesh ,*InStaticMesh->GetFullName());
		}
	}
#endif

	return TObjectCacheIterator<UStaticMeshComponent>(ComputeResult.Array());
}

TObjectCacheIterator<UMaterialInterface> FObjectCacheContext::GetMaterialsAffectedByTexture(UTexture* InTexture)
{
#if WITH_EDITOR
	const EObjectReverseLookupMode ObjectCacheContextMode = GetObjectReverseLookupMode();
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Permanent)
	{
		return GTextureToMaterialLookupCache.GetFrom(InTexture);
	}
#endif

	if (!TextureToMaterials.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeMaterialsAffectedByTexture);

		TMap<TObjectKey<UTexture>, TSet<UMaterialInterface*>> TempMap;
		TempMap.Reserve(8192);
		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{
			UMaterialInterface* MaterialInterface = *It;
			for (UTexture* Texture : GetUsedTextures(MaterialInterface))
			{
				TempMap.FindOrAdd(Texture).Add(MaterialInterface);
			}
		}
		TextureToMaterials = MoveTemp(TempMap);
	}

	static TSet<UMaterialInterface*> EmptySet;
	TSet<UMaterialInterface*>* Set = TextureToMaterials.GetValue().Find(InTexture);
	const TSet<UMaterialInterface*>& ComputeResult = Set ? *Set : EmptySet;

#if WITH_EDITOR
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Comparison)
	{
		TSet<UMaterialInterface*> LookupResult;
		for (UMaterialInterface* Material : GTextureToMaterialLookupCache.GetFrom(InTexture))
		{
			LookupResult.Add(Material);
			checkf(ComputeResult.Contains(Material), TEXT("Permanent Object Cache has an additionnal material %s for texture %s"), *Material->GetFullName(), *InTexture->GetFullName());
		}

		for (UMaterialInterface* Material : ComputeResult)
		{
			checkf(LookupResult.Contains(Material), TEXT("Permanent Object Cache is missing a material %s for texture %s"), *Material->GetFullName(), *InTexture->GetFullName());
		}
	}
#endif

	return TObjectCacheIterator<UMaterialInterface>(ComputeResult.Array());
}

TObjectCacheIterator<USkinnedMeshComponent> FObjectCacheContext::GetSkinnedMeshComponents(USkinnedAsset* InSkinnedAsset)
{
	if (!SkinnedAssetToComponents.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeSkeletalMeshToComponents);

		TMap<TObjectKey<USkinnedAsset>, TSet<USkinnedMeshComponent*>> TempMap;
		TempMap.Reserve(8192);
		for (USkinnedMeshComponent* Component : GetSkinnedMeshComponents())
		{
			TempMap.FindOrAdd(Component->GetSkinnedAsset()).Add(Component);
		}
		SkinnedAssetToComponents = MoveTemp(TempMap);
	}

	static TSet<USkinnedMeshComponent*> EmptySet;
	TSet<USkinnedMeshComponent*>* Set = SkinnedAssetToComponents.GetValue().Find(InSkinnedAsset);
	const TSet<USkinnedMeshComponent*>& Result = Set ? *Set : EmptySet;

	return TObjectCacheIterator<USkinnedMeshComponent>(Result.Array());
}

TObjectCacheIterator<UPrimitiveComponent> FObjectCacheContext::GetPrimitivesAffectedByMaterial(UMaterialInterface* InMaterialInterface)
{
#if WITH_EDITOR
	const EObjectReverseLookupMode ObjectCacheContextMode = GetObjectReverseLookupMode();
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Permanent)
	{
		return GMaterialToPrimitiveLookupCache.GetFrom(InMaterialInterface);
	}
#endif

	if (!MaterialToPrimitives.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputePrimitivesAffectedByMaterial);

		TMap<TObjectKey<UMaterialInterface>, TSet<UPrimitiveComponent*>> TempMap;
		for (UPrimitiveComponent* Component : GetPrimitiveComponents())
		{
			if (Component->IsRegistered() && Component->IsRenderStateCreated() && Component->SceneProxy)
			{
				for (UMaterialInterface* MaterialInterface : GetUsedMaterials(Component))
				{
					if (MaterialInterface)
					{
						TempMap.FindOrAdd(MaterialInterface).Add(Component);
					}
				}
			}
		}

		MaterialToPrimitives = MoveTemp(TempMap);
	}

	static TSet<UPrimitiveComponent*> EmptySet;
	TSet<UPrimitiveComponent*>* Set = MaterialToPrimitives.GetValue().Find(InMaterialInterface);
	const TSet<UPrimitiveComponent*>& ComputeResult = Set ? *Set : EmptySet;
	
#if WITH_EDITOR
	if (ObjectCacheContextMode == EObjectReverseLookupMode::Comparison)
	{
		TSet<UPrimitiveComponent*> LookupResult;
		for (UPrimitiveComponent* Component : GMaterialToPrimitiveLookupCache.GetFrom(InMaterialInterface))
		{
			LookupResult.Add(Component);
			if(!ComputeResult.Contains(Component))
			{
				UE_LOG(LogObjectCache, Warning, TEXT("Permanent Object Cache has an additionnal component %s for material %"), *Component->GetFullName(), *InMaterialInterface->GetFullName());
			}
		}

		for (UPrimitiveComponent* Component : ComputeResult)
		{
			if (Component->IsRenderStateCreated() && Component->SceneProxy && !Component->IsRenderStateDirty())
			{
				if (!LookupResult.Contains(Component))
				{
					UE_LOG(LogObjectCache, Warning, TEXT("Permanent Object Cache is missing a component %s for material %s"), *Component->GetFullName(), *InMaterialInterface->GetFullName());
				}
			}
		}
	}
#endif

	return TObjectCacheIterator<UPrimitiveComponent>(ComputeResult.Array());
}

namespace ObjectCacheContextScopeImpl
{
	static thread_local TUniquePtr<FObjectCacheContext> Current = nullptr;
}

FObjectCacheContextScope::FObjectCacheContextScope()
{
	using namespace ObjectCacheContextScopeImpl;
	if (Current == nullptr)
	{
		Current.Reset(new FObjectCacheContext());
		bIsOwner = true;
	}
}

FObjectCacheContextScope::~FObjectCacheContextScope()
{
	using namespace ObjectCacheContextScopeImpl;
	if (bIsOwner)
	{
		Current.Reset();
	}
}

FObjectCacheContext& FObjectCacheContextScope::GetContext() 
{ 
	using namespace ObjectCacheContextScopeImpl;
	return *Current; 
}

#if WITH_EDITOR

struct FObjectCacheEventSinkPrivate
{
	static void BeginQueueNotifyEvents();
	static void ProcessQueuedNotifyEvents();
	static void EndQueueNotifyEvents();

	static void NotifyUsedMaterialsChanged_Concurrent(const UPrimitiveComponent* PrimitiveComponent, const TArray<UMaterialInterface*>& UsedMaterials);
	static void NotifyRenderStateChanged_Concurrent(const UPrimitiveComponent*);
	static void NotifyReferencedTextureChanged_Concurrent(UMaterialInterface*);
	static void NotifyStaticMeshChanged_Concurrent(UStaticMeshComponent*);
	static void NotifyMaterialDestroyed_Concurrent(UMaterialInterface*);

	enum ECacheEventType
	{
		EMaterialDestroyed,
		EMaterialsChanged,
		ERenderStateChanged,
		EReferencedTextureChanged,
		EStaticMeshChanged
	};

	struct FNotifyEvent
	{
		ECacheEventType EventType;
		UMaterialInterface* MaterialInterface;
		const UPrimitiveComponent* PrimitiveComponent;
		UStaticMeshComponent* StaticMeshComponent;
		TArray<UMaterialInterface*> UsedMaterials;
	};

	static std::atomic<bool> bShouldQueueSinkEvents;

	using TEventAllocator = TLockFreeClassAllocator<FNotifyEvent, PLATFORM_CACHE_LINE_SIZE>;
	using TEventList = TLockFreePointerListFIFO<FNotifyEvent, PLATFORM_CACHE_LINE_SIZE>;
	static TEventAllocator& GetAllocator();
	static TEventList& GetNotifyEvents();
	static void AddNotifyEvent(FObjectCacheEventSinkPrivate::ECacheEventType EventType, UMaterialInterface* MaterialInterface, const UPrimitiveComponent* PrimitiveComponent, UStaticMeshComponent* StaticMeshComponent, const TArray<UMaterialInterface*>* UsedMaterials);
};

void FObjectCacheEventSink::BeginQueueNotifyEvents()
{
	FObjectCacheEventSinkPrivate::BeginQueueNotifyEvents();
}

void FObjectCacheEventSink::ProcessQueuedNotifyEvents()
{
	FObjectCacheEventSinkPrivate::ProcessQueuedNotifyEvents();
}

void FObjectCacheEventSink::EndQueueNotifyEvents()
{
	FObjectCacheEventSinkPrivate::EndQueueNotifyEvents();
}

void FObjectCacheEventSink::NotifyMaterialDestroyed_Concurrent(UMaterialInterface* MaterialInterface)
{
	if (GetObjectReverseLookupMode() != EObjectReverseLookupMode::Temporary)
	{
		if (FObjectCacheEventSinkPrivate::bShouldQueueSinkEvents)
		{
			FObjectCacheEventSinkPrivate::AddNotifyEvent(FObjectCacheEventSinkPrivate::EMaterialDestroyed, MaterialInterface, nullptr, nullptr, nullptr);
		}
		else
		{
			FObjectCacheEventSinkPrivate::NotifyMaterialDestroyed_Concurrent(MaterialInterface);
		}
	}
}

void FObjectCacheEventSink::NotifyUsedMaterialsChanged_Concurrent(const UPrimitiveComponent* PrimitiveComponent, const TArray<UMaterialInterface*>& UsedMaterials)
{
	if (GetObjectReverseLookupMode() != EObjectReverseLookupMode::Temporary)
	{
		if (FObjectCacheEventSinkPrivate::bShouldQueueSinkEvents)
		{
			FObjectCacheEventSinkPrivate::AddNotifyEvent(FObjectCacheEventSinkPrivate::EMaterialsChanged, nullptr, PrimitiveComponent, nullptr, &UsedMaterials);
		}
		else
		{
			FObjectCacheEventSinkPrivate::NotifyUsedMaterialsChanged_Concurrent(PrimitiveComponent, UsedMaterials);
		}
	}
}

void FObjectCacheEventSink::NotifyRenderStateChanged_Concurrent(UActorComponent* ActorComponent)
{
	if (GetObjectReverseLookupMode() != EObjectReverseLookupMode::Temporary)
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(ActorComponent))
		{
			if (FObjectCacheEventSinkPrivate::bShouldQueueSinkEvents)
			{
				FObjectCacheEventSinkPrivate::AddNotifyEvent(FObjectCacheEventSinkPrivate::ERenderStateChanged, nullptr, Primitive, nullptr, nullptr);
			}
			else
			{
				FObjectCacheEventSinkPrivate::NotifyRenderStateChanged_Concurrent(Primitive);
			}
		}
	}
}

void FObjectCacheEventSink::NotifyReferencedTextureChanged_Concurrent(UMaterialInterface* MaterialInterface)
{
	if (GetObjectReverseLookupMode() != EObjectReverseLookupMode::Temporary)
	{
		if (FObjectCacheEventSinkPrivate::bShouldQueueSinkEvents)
		{
			FObjectCacheEventSinkPrivate::AddNotifyEvent(FObjectCacheEventSinkPrivate::EReferencedTextureChanged, MaterialInterface, nullptr, nullptr, nullptr);
		}
		else
		{
			FObjectCacheEventSinkPrivate::NotifyReferencedTextureChanged_Concurrent(MaterialInterface);
		}
	}
}

void FObjectCacheEventSink::NotifyStaticMeshChanged_Concurrent(UStaticMeshComponent* StaticMeshComponent)
{
	if (GetObjectReverseLookupMode() != EObjectReverseLookupMode::Temporary)
	{
		if (FObjectCacheEventSinkPrivate::bShouldQueueSinkEvents)
		{
			FObjectCacheEventSinkPrivate::AddNotifyEvent(FObjectCacheEventSinkPrivate::EStaticMeshChanged, nullptr, nullptr, StaticMeshComponent, nullptr);
		}
		else
		{
			FObjectCacheEventSinkPrivate::NotifyStaticMeshChanged_Concurrent(StaticMeshComponent);
		}
	}
}

std::atomic<bool> FObjectCacheEventSinkPrivate::bShouldQueueSinkEvents(false);

FObjectCacheEventSinkPrivate::TEventAllocator& FObjectCacheEventSinkPrivate::GetAllocator()
{
	static FObjectCacheEventSinkPrivate::TEventAllocator TheAllocator;
	return TheAllocator;
}

FObjectCacheEventSinkPrivate::TEventList& FObjectCacheEventSinkPrivate::GetNotifyEvents()
{
	static TEventList NotifyEvents;
	return NotifyEvents;
}

void FObjectCacheEventSinkPrivate::AddNotifyEvent(FObjectCacheEventSinkPrivate::ECacheEventType EventType, UMaterialInterface* MaterialInterface, const UPrimitiveComponent* PrimitiveComponent, UStaticMeshComponent* StaticMeshComponent, const TArray<UMaterialInterface*>* UsedMaterials)
{

	FNotifyEvent* Event = GetAllocator().New();
	Event->EventType = EventType;
	Event->MaterialInterface = MaterialInterface;
	Event->PrimitiveComponent = PrimitiveComponent;
	Event->StaticMeshComponent = StaticMeshComponent;

	if (EventType == EMaterialsChanged)
	{
		check(UsedMaterials);
		Event->UsedMaterials = *UsedMaterials;
	}

	GetNotifyEvents().Push(Event);
}

void FObjectCacheEventSinkPrivate::BeginQueueNotifyEvents()
{
	check(!bShouldQueueSinkEvents);
	bShouldQueueSinkEvents = true;
}

void FObjectCacheEventSinkPrivate::ProcessQueuedNotifyEvents()
{
	check(bShouldQueueSinkEvents);

	TEventList& Events = GetNotifyEvents();
	TEventAllocator& Allocator = GetAllocator();

	while (FNotifyEvent* Event = Events.Pop())
	{
		switch (Event->EventType)
		{
		case EMaterialDestroyed:
			NotifyMaterialDestroyed_Concurrent(Event->MaterialInterface);
			break;
		case EMaterialsChanged:
			NotifyUsedMaterialsChanged_Concurrent(Event->PrimitiveComponent, Event->UsedMaterials);
			break;
		case ERenderStateChanged:
			NotifyRenderStateChanged_Concurrent(Event->PrimitiveComponent);
			break;
		case EReferencedTextureChanged:
			NotifyReferencedTextureChanged_Concurrent(Event->MaterialInterface);
			break;
		case EStaticMeshChanged:
			NotifyStaticMeshChanged_Concurrent(Event->StaticMeshComponent);
			break;
		}

		Allocator.Free(Event);
	}
}

void FObjectCacheEventSinkPrivate::EndQueueNotifyEvents()
{
	check(bShouldQueueSinkEvents);
	ProcessQueuedNotifyEvents();
	bShouldQueueSinkEvents = false;
}

void FObjectCacheEventSinkPrivate::NotifyMaterialDestroyed_Concurrent(UMaterialInterface* MaterialInterface)
{
	// If a material is destroyed, remove it from the cache so we don't return it anymore
	// This can happen if a component doesn't update its render state after modifying its used materials
	// (i.e. LandscapeComponent won't update it's render state when modifying MobileMaterialInterfaces during cook since they aren't meaningful for rendering)
	// The net result is that the SceneProxy->UsedMaterialsForVerification will continue to hold an invalid material interface object but if it's not used for rendering, no harm will be done...
	GMaterialToPrimitiveLookupCache.RemoveFrom(MaterialInterface);

	// Remove any Material to Texture that might be present in the cache for this Material
	GTextureToMaterialLookupCache.Update(MaterialInterface, {});
}

void FObjectCacheEventSinkPrivate::NotifyUsedMaterialsChanged_Concurrent(const UPrimitiveComponent* PrimitiveComponent, const TArray<UMaterialInterface*>& UsedMaterials)
{
	GMaterialToPrimitiveLookupCache.Update(const_cast<UPrimitiveComponent*>(PrimitiveComponent), UsedMaterials);
}

void FObjectCacheEventSinkPrivate::NotifyRenderStateChanged_Concurrent(const UPrimitiveComponent* PrimitiveComponent)
{
	// Clear mappings whenever the render state changes until NotifyUsedMaterialsChanged is called during SceneProxy creation
	GMaterialToPrimitiveLookupCache.Update(const_cast<UPrimitiveComponent*>(PrimitiveComponent), {});
}

void FObjectCacheEventSinkPrivate::NotifyReferencedTextureChanged_Concurrent(UMaterialInterface* MaterialInterface)
{
	if (MaterialInterface->HasAnyFlags(RF_BeginDestroyed))
	{
		GTextureToMaterialLookupCache.Update(MaterialInterface, {});
	}
	else
	{
		TSet<UTexture*> Textures;
		ObjectCacheContextImpl::GetReferencedTextures(MaterialInterface, Textures);
		GTextureToMaterialLookupCache.Update(MaterialInterface, Textures.Array());
	}
}

void FObjectCacheEventSinkPrivate::NotifyStaticMeshChanged_Concurrent(UStaticMeshComponent* StaticMeshComponent)
{
	// Stop tracking components that are being destroyed
	if (StaticMeshComponent->HasAnyFlags(RF_BeginDestroyed) || StaticMeshComponent->GetStaticMesh() == nullptr)
	{
		GStaticMeshToComponentLookupCache.Update(StaticMeshComponent, {});
	}
	else
	{
		GStaticMeshToComponentLookupCache.Update(StaticMeshComponent, { StaticMeshComponent->GetStaticMesh() });
	}
}

#endif

#undef LOCTEXT_NAMESPACE