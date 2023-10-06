// Copyright Epic Games, Inc. All Rights Reserved.

#include "CEFWebBrowserWindowRHIHelper.h"
#include "WebBrowserLog.h"

#if WITH_CEF3

#include "CEF/CEFWebBrowserWindow.h"
#if WITH_ENGINE
#include "RHI.h"
#if PLATFORM_WINDOWS
#include "Slate/SlateTextures.h"
#include "RenderingThread.h"
#endif
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "d3d11.h"
#include "d3d11_1.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif		// PLATFORM_WINDOWS


FCEFWebBrowserWindowRHIHelper::~FCEFWebBrowserWindowRHIHelper()
{
}

bool FCEFWebBrowserWindowRHIHelper::BUseRHIRenderer()
{
#if WITH_ENGINE
	if (GDynamicRHI != nullptr && FCEFWebBrowserWindow::CanSupportAcceleratedPaint())
	{
		return RHIGetInterfaceType() == ERHIInterfaceType::D3D11;
	}
#endif
	return false;
}

void FCEFWebBrowserWindowRHIHelper::UpdateCachedGeometry(const FGeometry& AllottedGeometryIn)
{
	AllottedGeometry = AllottedGeometryIn;
}

TOptional<FSlateRenderTransform> FCEFWebBrowserWindowRHIHelper::GetWebBrowserRenderTransform() const
{
	return FSlateRenderTransform(Concatenate(FScale2D(1, -1), FVector2D(0, AllottedGeometry.GetLocalSize().Y)));
}

FSlateUpdatableTexture *FCEFWebBrowserWindowRHIHelper::CreateTexture(void* SharedHandle)
{
#if WITH_ENGINE
	check(BUseRHIRenderer());
#if PLATFORM_WINDOWS
	ID3D11Device1* dev1 = nullptr;
	ID3D11Device* D3D11Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
	HRESULT Hr = D3D11Device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&dev1));
	if (FAILED(Hr))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("FCEFWebBrowserWindowRHIHelper::CreateTexture() - - ID3D11Device::QueryInterface"));
		return nullptr;
	}

	ID3D11Texture2D* tex = nullptr;
	Hr = dev1->OpenSharedResource1(SharedHandle, __uuidof(ID3D11Texture2D), (void**)(&tex));
	if (FAILED(Hr))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("FCEFWebBrowserWindowRHIHelper::CreateTexture() - - ID3D11Device::OpenSharedResource"));
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC TexDesc;
	tex->GetDesc(&TexDesc);
	FSlateTexture2DRHIRef* NewTexture = new FSlateTexture2DRHIRef(TexDesc.Width, TexDesc.Height, PF_R8G8B8A8, nullptr, TexCreate_Dynamic, true);
	if (IsInRenderingThread())
	{
		NewTexture->InitResource(FRHICommandListImmediate::Get());
	}
	else
	{
		BeginInitResource(NewTexture);
	}

	return NewTexture;
#else
	return nullptr;
#endif
#else
	return nullptr;
#endif
}

void FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture(void *SharedHandle, FSlateUpdatableTexture* SlateTexture, const FIntRect& DirtyIn)
{
#if WITH_ENGINE
	check(BUseRHIRenderer());
	FIntRect Dirty = DirtyIn;

#if PLATFORM_WINDOWS
	ID3D11Device1* dev1 = nullptr;
	ID3D11Device* D3D11Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
	HRESULT Hr = D3D11Device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&dev1));
	if (FAILED(Hr))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture() - ID3D11Device::QueryInterface"), Hr);
		return;
	}

	ID3D11Texture2D* Tex = nullptr;
	Hr = dev1->OpenSharedResource1(SharedHandle, __uuidof(ID3D11Texture2D), (void**)(&Tex));
	if (FAILED(Hr))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture() - ID3D11Device::OpenSharedResource"), Hr);
		return;
	}

	D3D11_TEXTURE2D_DESC TexDesc;
	Tex->GetDesc(&TexDesc);
	if (SlateTexture->GetSlateResource()->GetWidth() != TexDesc.Width
		|| SlateTexture->GetSlateResource()->GetHeight() != TexDesc.Height)
	{
		SlateTexture->ResizeTexture(TexDesc.Width, TexDesc.Height);
		Dirty = FIntRect();
	}

	TRefCountPtr<IDXGIKeyedMutex> KeyedMutex;
	Hr = Tex->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);
	if (FAILED(Hr))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture() - ID3D11Texture2D::IDXGIKeyedMutex"), Hr);
		return;
	}

	ENQUEUE_RENDER_COMMAND(CEFAcceleratedPaint)(
		[KeyedMutex, SlateTexture, Dirty, Tex](FRHICommandList& RHICmdList)
		{
			if (KeyedMutex)
			{
				if (KeyedMutex->AcquireSync(1, 16) == S_OK)
				{
					D3D11_BOX Region;
					Region.front = 0;
					Region.back = 1;
					if (Dirty.Area() > 0)
					{
						Region.left = Dirty.Min.X;
						Region.top = Dirty.Min.Y;
						Region.right = Dirty.Max.X;
						Region.bottom = Dirty.Max.Y;
					}
					else
					{
						Region.left = 0;
						Region.right = SlateTexture->GetSlateResource()->GetWidth();
						Region.top = 0;
						Region.bottom = SlateTexture->GetSlateResource()->GetHeight();
					}

					ID3D11DeviceContext* DX11DeviceContext = nullptr;
					ID3D11Device* D3D11Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
					D3D11Device->GetImmediateContext(&DX11DeviceContext);
					check(DX11DeviceContext);

					FSlateTexture2DRHIRef* SlateRHITexture = static_cast<FSlateTexture2DRHIRef*>(SlateTexture);
					check(SlateRHITexture);
					auto Slate2DRef = SlateRHITexture->GetRHIRef();
					if (Slate2DRef && Slate2DRef.IsValid())
					{
						auto Slate2DTexture = Slate2DRef->GetTexture2D();
						if (Slate2DTexture)
						{
							ID3D11Texture2D* D3DTexture = (ID3D11Texture2D*)Slate2DTexture->GetNativeResource();

							DX11DeviceContext->CopySubresourceRegion(D3DTexture, 0, Region.left, Region.top, Region.front, Tex, 0, &Region);
						}
					}

					KeyedMutex->ReleaseSync(0);
				}
				else
				{
					UE_LOG(LogWebBrowser, Verbose, TEXT("FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture() - failed getting sync"), E_FAIL);
				}
			}
			else
			{
				UE_LOG(LogWebBrowser, Verbose, TEXT("FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture() - missing KeyedMutex"), E_FAIL);
				return;
			}
	});
#else
	UE_LOG(LogWebBrowser, Error, TEXT("FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture() - missing implementation"));
#endif // PLATFORM_WINDOWS
#else
	UE_LOG(LogWebBrowser, Error, TEXT("FCEFWebBrowserWindowRHIHelper::UpdateSharedHandleTexture() - unsupported usage, RHI renderer but missing engine"));
#endif // WITH_ENGINE
}

#endif
