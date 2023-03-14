// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/TextureShareAPI.h"
#include "Module/TextureShareLog.h"
#include "Object/TextureShareObject.h"

#include "Misc/TextureShareStrings.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreObject.h"

#include "Framework/Application/SlateApplication.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareAPIHelper
{
	static const FName RendererModuleName(TEXT("Renderer"));

	static ITextureShareCoreAPI& TextureShareCoreAPI()
	{
		static ITextureShareCoreAPI& TextureShareCoreAPISingleton = ITextureShareCore::Get().GetTextureShareCoreAPI();
		return TextureShareCoreAPISingleton;
	}

	static ETextureShareDeviceType GetTextureShareDeviceType()
	{
		switch(RHIGetInterfaceType())
		{
		case ERHIInterfaceType::D3D11:  return ETextureShareDeviceType::D3D11;
		case ERHIInterfaceType::D3D12:  return ETextureShareDeviceType::D3D12;
		case ERHIInterfaceType::Vulkan: return ETextureShareDeviceType::Vulkan;
		default:
			break;
		}

		return ETextureShareDeviceType::Undefined;
	}
};
using namespace TextureShareAPIHelper;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareAPI
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareAPI::FTextureShareAPI()
{
	UE_LOG(LogTextureShare, Log, TEXT("TextureShare API has been instantiated"));
}

FTextureShareAPI::~FTextureShareAPI()
{
	FScopeLock Lock(&ThreadDataCS);

	RemoveTextureShareObjectInstances();
	UnregisterCallbacks();

	UE_LOG(LogTextureShare, Log, TEXT("TextureShare API has been destroyed"));
}

//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<ITextureShareObject, ESPMode::ThreadSafe> FTextureShareAPI::GetOrCreateObject(const FString& ShareName)
{
	check(IsInGameThread());

	FScopeLock Lock(&ThreadDataCS);

	const FString& ShareNameLwr = ShareName.ToLower();
	if (const TSharedPtr<FTextureShareObject, ESPMode::ThreadSafe>* ExistObjectPtr = Objects.Find(ShareNameLwr))
	{
		return *ExistObjectPtr;
	}

	//Create new
	TSharedPtr<ITextureShareCoreObject, ESPMode::ThreadSafe> CoreObject = TextureShareCoreAPI().GetOrCreateCoreObject(ShareName);
	if (CoreObject.IsValid())
	{
		// Set current DeviceType
		CoreObject->SetDeviceType(GetTextureShareDeviceType());

		// Create game thread object
		const TSharedPtr<FTextureShareObject, ESPMode::ThreadSafe> NewObject(new FTextureShareObject(CoreObject.ToSharedRef()));
		if (NewObject.IsValid())
		{
			// Register for game thread
			Objects.Emplace(ShareNameLwr, NewObject);

			//Register for render thread
			ENQUEUE_RENDER_COMMAND(TextureShare_CreateObjectProxy)(
				[TextureShareAPI = this, ShareNameLwr, NewObjectProxy = NewObject->GetObjectProxyRef()](FRHICommandListImmediate& RHICmdList)
			{
				TextureShareAPI->ObjectProxies.Emplace(ShareNameLwr, NewObjectProxy);
			});

			// Register UE callbacks to access to scene textures and final backbuffer
			RegisterCallbacks();

			UE_LOG(LogTextureShare, Log, TEXT("Created new TextureShare object '%s'"), *ShareName);

			return NewObject;
		}

		// Failed
		CoreObject.Reset();
		TextureShareCoreAPI().RemoveCoreObject(ShareName);
	}

	UE_LOG(LogTextureShare, Error, TEXT("CreateTextureShareObject '%s' failed"), *ShareName);

	return nullptr;
}

bool FTextureShareAPI::RemoveObject(const FString& ShareName)
{
	// May call from both threads
	FScopeLock Lock(&ThreadDataCS);

	const FString& ShareNameLwr = ShareName.ToLower();
	if (!Objects.Contains(ShareNameLwr))
	{
		UE_LOG(LogTextureShare, Error, TEXT("Can't remove TextureShare '%s' - not exist"), *ShareName);
		return false;
	}

	TSharedPtr<FTextureShareObject, ESPMode::ThreadSafe> Object = Objects[ShareNameLwr];
	Objects[ShareNameLwr].Reset();
	Objects.Remove(ShareNameLwr);

	ENQUEUE_RENDER_COMMAND(TextureShare_RemoveObjectProxy)(
		[TextureShareAPI = this, ShareNameLwr](FRHICommandListImmediate& RHICmdList)
	{
		TextureShareAPI->ObjectProxies.Remove(ShareNameLwr);
	});

	UE_LOG(LogTextureShare, Log, TEXT("Removed TextureShare object '%s'"), *ShareName);

	return true;
}

bool FTextureShareAPI::IsObjectExist(const FString& ShareName) const
{
	check(IsInGameThread());

	FScopeLock Lock(&ThreadDataCS);

	return Objects.Contains(ShareName.ToLower());
}

//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<ITextureShareObject, ESPMode::ThreadSafe> FTextureShareAPI::GetObject(const FString& ShareName) const
{
	check(IsInGameThread());

	FScopeLock Lock(&ThreadDataCS);

	if (const TSharedPtr<FTextureShareObject, ESPMode::ThreadSafe>* ExistObject = Objects.Find(ShareName.ToLower()))
	{
		return *ExistObject;
	}

	return nullptr;
}

TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe> FTextureShareAPI::GetObjectProxy_RenderThread(const FString& ShareName) const
{
	check(IsInRenderingThread());

	if (const TSharedPtr<FTextureShareObjectProxy, ESPMode::ThreadSafe>* ExistObject = ObjectProxies.Find(ShareName.ToLower()))
	{
		return *ExistObject;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareAPI::GetInterprocessObjects(const FString& InShareName, TArray<FTextureShareCoreObjectDesc>& OutInterprocessObjects) const
{
	TArraySerializable<FTextureShareCoreObjectDesc> InterprocessObjects;
	if (TextureShareCoreAPI().GetInterprocessObjects(InShareName, InterprocessObjects))
	{
		OutInterprocessObjects.Empty();
		OutInterprocessObjects.Append(InterprocessObjects);

		return true;
	}

	return false;
}

const FTextureShareCoreObjectProcessDesc& FTextureShareAPI::GetProcessDesc() const
{
	return TextureShareCoreAPI().GetProcessDesc();
}

void FTextureShareAPI::SetProcessName(const FString& InProcessId)
{
	TextureShareCoreAPI().SetProcessName(InProcessId);
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareAPI::RemoveTextureShareObjectInstances()
{
	// May call from both threads
	FScopeLock Lock(&ThreadDataCS);

	// Remove all objects
	Objects.Empty();

	// Remove all proxy
	ENQUEUE_RENDER_COMMAND(TextureShare_RemoveAll)(
		[TextureShareAPI = this](FRHICommandListImmediate& RHICmdList)
	{
		TextureShareAPI->ObjectProxies.Empty();
	});
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareAPI::OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	check(IsInRenderingThread());

	for (TPair<FString, TSharedPtr<FTextureShareObjectProxy, ESPMode::ThreadSafe>>& ProxyIt : ObjectProxies)
	{
		if (ProxyIt.Value.IsValid())
		{
			const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe>& SceneViewExtension = ProxyIt.Value->GetViewExtension_RenderThread();
			if (SceneViewExtension.IsValid())
			{
				SceneViewExtension->OnResolvedSceneColor_RenderThread(GraphBuilder, SceneTextures);
			}
		}
	}
}

void FTextureShareAPI::OnBackBufferReadyToPresent_RenderThread(SWindow& InWindow, const FTexture2DRHIRef& InBackbuffer)
{
	check(IsInRenderingThread());

	for (TPair<FString, TSharedPtr<FTextureShareObjectProxy, ESPMode::ThreadSafe>>& ProxyIt : ObjectProxies)
	{
		if (ProxyIt.Value.IsValid())
		{
			const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe>& SceneViewExtension = ProxyIt.Value->GetViewExtension_RenderThread();
			if (SceneViewExtension.IsValid())
			{
				SceneViewExtension->OnBackBufferReadyToPresent_RenderThread(InWindow, InBackbuffer);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareAPI::OnWorldBeginPlay(UWorld& InWorld)
{ }

void FTextureShareAPI::OnWorldEndPlay(UWorld& InWorld)
{ }

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareAPI::RegisterCallbacks()
{
	if (!ResolvedSceneColorCallbackHandle.IsValid())
	{
		if (IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName))
		{
			UE_LOG(LogTextureShare, Verbose, TEXT("Add Renderer module callbacks"));
			ResolvedSceneColorCallbackHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FTextureShareAPI::OnResolvedSceneColor_RenderThread);
		}
	}

	if (!OnBackBufferReadyToPresentHandle.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			OnBackBufferReadyToPresentHandle = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(this, &FTextureShareAPI::OnBackBufferReadyToPresent_RenderThread);
		}
	}
}

void FTextureShareAPI::UnregisterCallbacks()
{
	if (ResolvedSceneColorCallbackHandle.IsValid())
	{
		if (IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName))
		{
			UE_LOG(LogTextureShare, Verbose, TEXT("Remove Renderer module callbacks"));
			RendererModule->GetResolvedSceneColorCallbacks().Remove(ResolvedSceneColorCallbackHandle);
		}

		ResolvedSceneColorCallbackHandle.Reset();
	}

	if (OnBackBufferReadyToPresentHandle.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(OnBackBufferReadyToPresentHandle);
		}
	}
}
