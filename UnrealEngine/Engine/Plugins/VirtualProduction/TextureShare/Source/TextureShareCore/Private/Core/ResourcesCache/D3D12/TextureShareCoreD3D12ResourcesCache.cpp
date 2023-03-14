// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ResourcesCache/D3D12/TextureShareCoreD3D12ResourcesCache.h"
#include "Core/ResourcesCache/D3D12/TextureShareCoreD3D12ResourcesCacheItem.h"
#include "Core/TextureShareCoreSecurityAttributes.h"

#include "Module/TextureShareCoreLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreD3D12ResourcesCache
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreD3D12ResourcesCache::CreateSharedResource(const FTextureShareCoreObjectDesc& InObjectDesc, ID3D12Device* InD3D12Device, ID3D12Resource* InResourceD3D12, const FTextureShareCoreResourceDesc& InResourceDesc, FTextureShareCoreResourceHandle& OutResourceHandle)
{
	FObjectCachedResources& ObjectResources = GetObjectCachedResources(InObjectDesc);

	// Use exist resource from cache
	if (FTextureShareCoreResourcesCacheItem* ExistResourcesCacheItem = ObjectResources.FindByNativeResourcePtr(InResourceD3D12))
	{
		OutResourceHandle = ExistResourcesCacheItem->GetHandle();

		check(OutResourceHandle.EqualsFunc(InResourceDesc));

		return true;
	}

	// Create new resource
	TSharedPtr<FTextureShareCoreResourcesCacheItem> NewResourcesCacheItem = MakeShared<FTextureShareCoreD3D12ResourcesCacheItem>(InD3D12Device, InResourceD3D12, InResourceDesc, SecurityAttributes->GetSecurityAttributes(ETextureShareSecurityAttributesType::Resource));
	if (NewResourcesCacheItem.IsValid() && NewResourcesCacheItem->GetNativeResource())
	{
		ObjectResources.Resources.Add(NewResourcesCacheItem);
		OutResourceHandle = NewResourcesCacheItem->GetHandle();

		return true;
	}

	return false;
}

ID3D12Resource* FTextureShareCoreD3D12ResourcesCache::OpenSharedResource(const FTextureShareCoreObjectDesc& InObjectDesc, ID3D12Device* InD3D12Device, const FTextureShareCoreResourceHandle& InResourceHandle)
{
	FObjectCachedResources& ObjectResources = GetObjectCachedResources(InObjectDesc);

	// Use exist resource from cache
	if (FTextureShareCoreResourcesCacheItem* ExistResourcesCacheItem = ObjectResources.Find(InResourceHandle))
	{
		return static_cast<ID3D12Resource*>(ExistResourcesCacheItem->GetNativeResource());
	}

	// Create new resource
	TSharedPtr<FTextureShareCoreResourcesCacheItem> NewResourcesCacheItem = MakeShared<FTextureShareCoreD3D12ResourcesCacheItem>(InD3D12Device, InResourceHandle);
	if (NewResourcesCacheItem.IsValid() && NewResourcesCacheItem->GetNativeResource())
	{
		ObjectResources.Resources.Add(NewResourcesCacheItem);

		return static_cast<ID3D12Resource*>(NewResourcesCacheItem->GetNativeResource());
	}

	return nullptr;
}

bool FTextureShareCoreD3D12ResourcesCache::RemoveSharedResource(const FTextureShareCoreObjectDesc& InObjectDesc, ID3D12Resource* InResourceD3D12)
{
	return GetObjectCachedResources(InObjectDesc).RemoveByNativeResourcePtr(InResourceD3D12);
}
