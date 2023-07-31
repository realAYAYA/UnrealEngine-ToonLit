// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ResourcesCache/D3D12/TextureShareCoreD3D12ResourcesCacheItem.h"
#include "Module/TextureShareCoreLog.h"
#include "Templates/RefCounting.h"

#if PLATFORM_WINDOWS
THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <d3d12.h>
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareCoreD3D12ResourcesCacheItemHelpers
{
	static inline const FString GetD3D12ComErrorDescription(HRESULT Res)
	{
		const uint32 BufSize = 4096;
		WIDECHAR buffer[4096];
		if (::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
			nullptr,
			Res,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
			buffer,
			sizeof(buffer) / sizeof(*buffer),
			nullptr))
		{
			return buffer;
		}
		else
		{
			return TEXT("[cannot find d3d12 error description]");
		}
	}

	static inline FString GetUniqueD3D12ResourceHandleName(const FGuid& InResourceGuid)
	{
		return FString::Printf(TEXT("Global\\%s"), *InResourceGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	}
};
using namespace TextureShareCoreD3D12ResourcesCacheItemHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreD3D12ResourceCache::FCachedResourceD3D12
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareCoreD3D12ResourcesCacheItem::~FTextureShareCoreD3D12ResourcesCacheItem()
{
	if (bNeedReleaseD3D12Resource && D3D12Resource)
	{
		D3D12Resource->Release();
		D3D12Resource = nullptr;
	}
}

// Create shared resource
FTextureShareCoreD3D12ResourcesCacheItem::FTextureShareCoreD3D12ResourcesCacheItem(ID3D12Device* InD3D12Device, ID3D12Resource* InResourceD3D12, const FTextureShareCoreResourceDesc& InResourceDesc, const void* InSecurityAttributes)
{
	if (InD3D12Device && InResourceD3D12)
	{
		FGuid ResourceHandleGuid = FGuid::NewGuid();

		// Microsoft docs: Currently the only value this parameter accepts is GENERIC_ALL.
		const DWORD dwAccess = GENERIC_ALL;

		//NT HANDLE value to the resource to share. You can use this handle in calls to access the resource.
		HANDLE SharedNTHandle;
		{
			HRESULT Result = InD3D12Device->CreateSharedHandle(InResourceD3D12, (const SECURITY_ATTRIBUTES*)InSecurityAttributes, dwAccess, *GetUniqueD3D12ResourceHandleName(ResourceHandleGuid), &SharedNTHandle);
			if (FAILED(Result))
			{
				UE_LOG(LogTextureShareCoreD3D, Error, TEXT("D3D12CreateSharedHandle failed: 0x%X - %s"), Result, *GetD3D12ComErrorDescription(Result));
				return;
			}
		}

		if (SharedNTHandle)
		{
			bNeedReleaseNTHandle = true;

			Handle.ResourceDesc = InResourceDesc;
			Handle.SharedHandleGuid = ResourceHandleGuid;
			Handle.NTHandle = SharedNTHandle;
			Handle.SharedHandle = NULL;

			// Store valid texture ptr
			D3D12Resource = InResourceD3D12;
		}
	}
}

// Open shared resource
FTextureShareCoreD3D12ResourcesCacheItem::FTextureShareCoreD3D12ResourcesCacheItem(ID3D12Device* InD3D12Device, const FTextureShareCoreResourceHandle& InResourceHandle)
{
	Handle = InResourceHandle;

	if (InD3D12Device)
	{
		if (Handle.SharedHandleGuid.IsValid())
		{
			HANDLE NamedNTHandleHandle = nullptr;
			{
				HRESULT Result = InD3D12Device->OpenSharedHandleByName(*GetUniqueD3D12ResourceHandleName(InResourceHandle.SharedHandleGuid), GENERIC_ALL, &NamedNTHandleHandle);
				if (FAILED(Result))
				{
					UE_LOG(LogTextureShareCoreD3D, Error, TEXT("D3D12OpenSharedHandleByName failed: 0x%X - %s"), Result, *GetD3D12ComErrorDescription(Result));
					return;
				}
			}

			if (NamedNTHandleHandle)
			{
				HRESULT Result = InD3D12Device->OpenSharedHandle(NamedNTHandleHandle, __uuidof(ID3D12Resource), (LPVOID*)&D3D12Resource);
				CloseHandle(NamedNTHandleHandle);

				if (FAILED(Result))
				{
					UE_LOG(LogTextureShareCoreD3D, Error, TEXT("D3D12OpenSharedHandle failed: 0x%X - %s"), Result, *GetD3D12ComErrorDescription(Result));
					return;
				}
			}

		}

		if (D3D12Resource == nullptr && InResourceHandle.NTHandle)
		{
			HRESULT Result = InD3D12Device->OpenSharedHandle(InResourceHandle.NTHandle, __uuidof(ID3D12Resource), (LPVOID*)&D3D12Resource);
			
			if (FAILED(Result))
			{
				UE_LOG(LogTextureShareCoreD3D, Error, TEXT("D3D12OpenSharedNTHandle failed: 0x%X - %s"), Result, *GetD3D12ComErrorDescription(Result));
				return;
			}
		}

		if (D3D12Resource == nullptr && InResourceHandle.SharedHandle)
		{
			HRESULT Result = InD3D12Device->OpenSharedHandle(InResourceHandle.SharedHandle, __uuidof(ID3D12Resource), (LPVOID*)&D3D12Resource);

			if (FAILED(Result))
			{
				UE_LOG(LogTextureShareCoreD3D, Error, TEXT("D3D12OpenSharedHandle failed: 0x%X - %s"), Result, *GetD3D12ComErrorDescription(Result));
				return;
			}
		}

		// Release opened D3D12 resource in destructor
		bNeedReleaseD3D12Resource = D3D12Resource != nullptr;
	}
}
