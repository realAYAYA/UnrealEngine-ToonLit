// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformWebBrowser.h"

#if PLATFORM_IOS
#include "IOS/IOSView.h"
#include "IOS/IOSAppDelegate.h"
#include "Widgets/SLeafWidget.h"
#include "MobileJS/MobileJSScripting.h"
#include "PlatformHttp.h"

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#include "ExternalTexture.h"
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"


class SIOSWebBrowserWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SIOSWebBrowserWidget)
		: _InitialURL("about:blank")
		, _UseTransparency(false)
	{ }

	SLATE_ARGUMENT(FString, InitialURL);
	SLATE_ARGUMENT(bool, UseTransparency);
	SLATE_ARGUMENT(TSharedPtr<FWebBrowserWindow>, WebBrowserWindow);

	SLATE_END_ARGS()

		SIOSWebBrowserWidget()
		: WebViewWrapper(nil)
	{}

	void Construct(const FArguments& Args)
	{
		bool bSupportsMetalMRT = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);

		bool bSupportsMetal = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
		// At this point we MUST be a Metal renderer.
		check(bSupportsMetal);

		WebViewWrapper = [IOSWebViewWrapper alloc];
		[WebViewWrapper create : TSharedPtr<SIOSWebBrowserWidget>(this) useTransparency : Args._UseTransparency supportsMetal : bSupportsMetal supportsMetalMRT : bSupportsMetalMRT];

		WebBrowserWindowPtr = Args._WebBrowserWindow;
		IsIOS3DBrowser = false;

#if !PLATFORM_TVOS
		TextureSamplePool = new FWebBrowserTextureSamplePool();
		WebBrowserTextureSamplesQueue = MakeShared<FWebBrowserTextureSampleQueue, ESPMode::ThreadSafe>();
		WebBrowserTexture = nullptr;
		WebBrowserMaterial = nullptr;
		WebBrowserBrush = nullptr;

		// create external texture
		WebBrowserTexture = NewObject<UWebBrowserTexture>((UObject*)GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

		if (WebBrowserTexture != nullptr)
		{
			WebBrowserTexture->UpdateResource();
			WebBrowserTexture->AddToRoot();
		}

		// create wrapper material
		IWebBrowserSingleton* WebBrowserSingleton = IWebBrowserModule::Get().GetSingleton();

		UMaterialInterface* DefaultWBMaterial = Args._UseTransparency ? WebBrowserSingleton->GetDefaultTranslucentMaterial() : WebBrowserSingleton->GetDefaultMaterial();
		if (WebBrowserSingleton && DefaultWBMaterial)
		{
			// create wrapper material
			WebBrowserMaterial = UMaterialInstanceDynamic::Create(DefaultWBMaterial, nullptr);

			if (WebBrowserMaterial)
			{
				WebBrowserMaterial->SetTextureParameterValue("SlateUI", WebBrowserTexture);
				WebBrowserMaterial->AddToRoot();

				// create Slate brush
				WebBrowserBrush = MakeShareable(new FSlateBrush());
				{
					WebBrowserBrush->SetResourceObject(WebBrowserMaterial);
				}
			}
		}
#endif
		LoadURL(Args._InitialURL);
	}


	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (WebViewWrapper != nil)
		{
			if (WebBrowserWindowPtr.IsValid())
			{
				WebBrowserWindowPtr.Pin()->SetTickLastFrame();
				if (WebBrowserWindowPtr.Pin()->GetParentWindow().IsValid())
				{
					bool ShouldSet3DBrowser = WebBrowserWindowPtr.Pin()->GetParentWindow().Get()->IsVirtualWindow();
					if (IsIOS3DBrowser != ShouldSet3DBrowser)
					{
						IsIOS3DBrowser = ShouldSet3DBrowser;
						[WebViewWrapper set3D : IsIOS3DBrowser];
					}
				}
			}

			UIView* View = [IOSAppDelegate GetDelegate].IOSView;
			CGFloat contentScaleFactor = View.contentScaleFactor;
			FVector2D Position = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() / contentScaleFactor;
			FVector2D Size = TransformVector(AllottedGeometry.GetAccumulatedRenderTransform(), AllottedGeometry.GetLocalSize()) / contentScaleFactor;
			CGRect NewFrame;
			NewFrame.origin.x = FMath::RoundToInt(Position.X);
			NewFrame.origin.y = FMath::RoundToInt(Position.Y);
			NewFrame.size.width = FMath::RoundToInt(Size.X);
			NewFrame.size.height = FMath::RoundToInt(Size.Y);

			[WebViewWrapper updateframe : NewFrame];

#if !PLATFORM_TVOS
			if (IsIOS3DBrowser)
			{
				if (WebBrowserTexture)
				{
					TSharedPtr<FWebBrowserTextureSample, ESPMode::ThreadSafe> WebBrowserTextureSample;
					WebBrowserTextureSamplesQueue->Peek(WebBrowserTextureSample);

					WebBrowserTexture->TickResource(WebBrowserTextureSample);
				}

				if (WebBrowserTexture != nullptr)
				{
					struct FWriteWebBrowserParams
					{
						IOSWebViewWrapper* NativeWebBrowserPtr;
						FGuid PlayerGuid;
						FIntPoint Size;
					};

					FIntPoint viewportSize = WebBrowserWindowPtr.Pin()->GetViewportSize();

					FWriteWebBrowserParams Params = { WebViewWrapper, WebBrowserTexture->GetExternalTextureGuid(), viewportSize };

					ENQUEUE_RENDER_COMMAND(WriteWebBrowser)(
						[Params](FRHICommandListImmediate& RHICmdList)
						{
							IOSWebViewWrapper* NativeWebBrowser = Params.NativeWebBrowserPtr;

							if (NativeWebBrowser == nil)
							{
								return;
							}

							FTextureRHIRef VideoTexture = [NativeWebBrowser GetVideoTexture];
							if (VideoTexture == nullptr)
							{
								const FRHITextureCreateDesc Desc =
									FRHITextureCreateDesc::Create2D(TEXT("SIOSWebBrowserWidget_VideoTexture"), Params.Size, PF_R8G8B8A8)
									.SetFlags(ETextureCreateFlags::External);

								VideoTexture = RHICreateTexture(Desc);
								[NativeWebBrowser SetVideoTexture : VideoTexture];
								//UE_LOG(LogIOS, Log, TEXT("NativeWebBrowser SetVideoTexture:VideoTexture!"));

								if (VideoTexture == nullptr)
								{
									UE_LOG(LogIOS, Warning, TEXT("RHICreateTexture failed!"));
									return;
								}

								[NativeWebBrowser SetVideoTextureValid : false];

							}

							if ([NativeWebBrowser UpdateVideoFrame : VideoTexture->GetNativeResource()])
							{
								// if region changed, need to reregister UV scale/offset
								//UE_LOG(LogIOS, Log, TEXT("UpdateVideoFrame RT: %s"), *Params.PlayerGuid.ToString());
							}

							if (![NativeWebBrowser IsVideoTextureValid])
							{
								FSamplerStateInitializerRHI SamplerStateInitializer(SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp);
								FSamplerStateRHIRef SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
								FExternalTextureRegistry::Get().RegisterExternalTexture(Params.PlayerGuid, VideoTexture, SamplerStateRHI);
								//UE_LOG(LogIOS, Log, TEXT("Fetch RT: Register Guid: %s"), *Params.PlayerGuid.ToString());

								[NativeWebBrowser SetVideoTextureValid : true];
							}
						});
				}
			}
#endif
		}

	}

	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
	{
#if !PLATFORM_TVOS
		bool bIsVisible = !WebBrowserWindowPtr.IsValid() || WebBrowserWindowPtr.Pin()->IsVisible();
		
		if (bIsVisible && IsIOS3DBrowser && WebBrowserBrush.IsValid())
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), WebBrowserBrush.Get(), ESlateDrawEffect::None);
		}
#endif
		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(640, 480);
	}

	void LoadURL(const FString& InNewURL)
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper loadurl : [NSURL URLWithString : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*InNewURL)]]];
		}
	}

	void LoadString(const FString& InContents, const FString& InDummyURL)
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper loadstring : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*InContents)] dummyurl : [NSURL URLWithString : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*InDummyURL)]]];
		}
	}
	
	void StopLoad()
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper stopLoading];
		}
	}

	void Reload()
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper reload];
		}
	}

	void Close()
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper close];
			WebViewWrapper = nil;
		}
		WebBrowserWindowPtr.Reset();
	}

	void GoBack()
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper goBack];
		}
	}

	void GoForward()
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper goForward];
		}
	}


	bool CanGoBack()
	{
		if (WebViewWrapper != nil)
		{
			return [WebViewWrapper canGoBack];
		}
		return false;
	}

	bool CanGoForward()
	{
		if (WebViewWrapper != nil)
		{
			return [WebViewWrapper canGoForward];
		}
		return false;
	}

	void SetWebBrowserVisibility(bool InIsVisible)
	{
		if (WebViewWrapper != nil)
		{
			UE_LOG(LogIOS, Warning, TEXT("SetWebBrowserVisibility %d!"), InIsVisible);

			[WebViewWrapper setVisibility : InIsVisible];
		}
	}

	bool HandleShouldOverrideUrlLoading(const FString& Url)
	{
		if (WebBrowserWindowPtr.IsValid())
		{
			// Capture vars needed for AsyncTask
			FString UrlString = Url;
			TWeakPtr<FWebBrowserWindow> AsyncWebBrowserWindowPtr = WebBrowserWindowPtr;

			// Notify on the game thread
			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			{
				TSharedPtr<FWebBrowserWindow> BrowserWindow = AsyncWebBrowserWindowPtr.Pin();
				if (BrowserWindow.IsValid())
				{
					if (BrowserWindow->OnBeforeBrowse().IsBound())
					{
						FWebNavigationRequest RequestDetails;
						RequestDetails.bIsRedirect = false;
						RequestDetails.bIsMainFrame = true; // shouldOverrideUrlLoading is only called on the main frame

						BrowserWindow->OnBeforeBrowse().Execute(Url, RequestDetails);
						BrowserWindow->SetTitle("");
					}
				}
				return true;
			}];
		}
		return true;
	}

	void HandleReceivedTitle(const FString& Title)
	{
		if (WebBrowserWindowPtr.IsValid())
		{
			TSharedPtr<FWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
			if (BrowserWindow.IsValid() && !BrowserWindow->GetTitle().Equals(Title))
			{
				BrowserWindow->SetTitle(Title);
			}
		}
	}

	void ProcessScriptMessage(const FString& InMessage)
	{
		if (WebBrowserWindowPtr.IsValid())
		{
			FString Message = InMessage;
			TWeakPtr<FWebBrowserWindow> AsyncWebBrowserWindowPtr = WebBrowserWindowPtr;

			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			{
				TSharedPtr<FWebBrowserWindow> BrowserWindow = AsyncWebBrowserWindowPtr.Pin();
				if (BrowserWindow.IsValid())
				{
					TArray<FString> Params;
					Message.ParseIntoArray(Params, TEXT("/"), false);
					if (Params.Num() > 0)
					{
						for (int I = 0; I < Params.Num(); I++)
						{
							Params[I] = FPlatformHttp::UrlDecode(Params[I]);
						}

						FString Command = Params[0];
						Params.RemoveAt(0, 1);
						BrowserWindow->OnJsMessageReceived(Command, Params, "");
					}
					else
					{
						GLog->Logf(ELogVerbosity::Error, TEXT("Invalid message from browser view: %s"), *Message);
					}
				}
				return true;
			}];
		}
	}

	void HandlePageLoad(const FString& InCurrentUrl, bool bIsLoading)
	{
		if (WebBrowserWindowPtr.IsValid())
		{
			TSharedPtr<FWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
			if (BrowserWindow.IsValid())
			{
				BrowserWindow->NotifyDocumentLoadingStateChange(InCurrentUrl, bIsLoading);
			}
		}
	}

	void HandleReceivedError(int ErrorCode, const FString& InCurrentUrl)
	{
		if (WebBrowserWindowPtr.IsValid())
		{
			TSharedPtr<FWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
			if (BrowserWindow.IsValid())
			{
				BrowserWindow->NotifyDocumentError(InCurrentUrl, ErrorCode);
			}
		}
	}

	void ExecuteJavascript(const FString& Script)
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper executejavascript : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*Script)]];
		}
	}


	~SIOSWebBrowserWidget()
	{
		Close();
	}

protected:
	mutable __strong IOSWebViewWrapper* WebViewWrapper;
private:
	TWeakPtr<FWebBrowserWindow> WebBrowserWindowPtr;

	/** Enable 3D appearance */
	bool IsIOS3DBrowser;

#if !PLATFORM_TVOS
	/** The external texture to render the webbrowser output. */
	UWebBrowserTexture* WebBrowserTexture;

	/** The material for the external texture. */
	UMaterialInstanceDynamic* WebBrowserMaterial;

	/** The Slate brush that renders the material. */
	TSharedPtr<FSlateBrush> WebBrowserBrush;

	/** The sample queue. */
	TSharedPtr<FWebBrowserTextureSampleQueue, ESPMode::ThreadSafe> WebBrowserTextureSamplesQueue;

	/** Texture sample object pool. */
	FWebBrowserTextureSamplePool* TextureSamplePool;
#endif
};

@implementation IOSWebViewWrapper

#if !PLATFORM_TVOS
@synthesize WebView;
@synthesize WebViewContainer;
#endif
@synthesize NextURL;
@synthesize NextContent;

-(void)create:(TSharedPtr<SIOSWebBrowserWidget>)InWebBrowserWidget useTransparency : (bool)InUseTransparency
supportsMetal : (bool)InSupportsMetal supportsMetalMRT : (bool)InSupportsMetalMRT;
{
	WebBrowserWidget = InWebBrowserWidget;
	NextURL = nil;
	NextContent = nil;
	VideoTexture = nil;
	bNeedsAddToView = true;
	IsIOS3DBrowser = false;
	bVideoTextureValid = false;
	bSupportsMetal = InSupportsMetal;
	bSupportsMetalMRT = InSupportsMetalMRT;

#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		WebViewContainer = [[UIView alloc]initWithFrame:CGRectMake(1, 1, 100, 100)];
		[self.WebViewContainer setOpaque : NO];
		[self.WebViewContainer setBackgroundColor : [UIColor clearColor]];

		WKWebViewConfiguration *theConfiguration = [[WKWebViewConfiguration alloc] init];
		NSString* MessageHandlerName = [NSString stringWithFString : FMobileJSScripting::JSMessageHandler];
		[theConfiguration.userContentController addScriptMessageHandler:self name: MessageHandlerName];

		WebView = [[WKWebView alloc]initWithFrame:CGRectMake(1, 1, 100, 100)  configuration : theConfiguration];
		[self.WebViewContainer addSubview : WebView];
		WebView.navigationDelegate = self;
		WebView.UIDelegate = self;

		WebView.scrollView.bounces = NO;

		if (InUseTransparency)
		{
			[self.WebView setOpaque : NO];
			[self.WebView setBackgroundColor : [UIColor clearColor]];
		}
		else
		{
			[self.WebView setOpaque : YES];
		}

		[theConfiguration release];
		[self setDefaultVisibility];
	});
#endif
}

-(void)close;
{
#if !PLATFORM_TVOS
	WebView.navigationDelegate = nil;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebViewContainer removeFromSuperview];
		[self.WebView removeFromSuperview];
		
		[WebView release];
		[WebViewContainer release];

		WebView = nil;
		WebViewContainer = nil;
	});
#endif
}

-(void)dealloc;
{
#if !PLATFORM_TVOS
	if (WebView != nil)
	{
		WebView.navigationDelegate = nil;
		[WebView release];
		WebView = nil;
	};

	[WebViewContainer release];
	WebViewContainer = nil;
#endif
	[NextContent release];
	NextContent = nil;
	[NextURL release];
	NextURL = nil;

	[super dealloc];
}

-(void)updateframe:(CGRect)InFrame;
{
	self.DesiredFrame = InFrame;

#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (WebView != nil)
		{
			WebViewContainer.frame = self.DesiredFrame;
			WebView.frame = WebViewContainer.bounds;
			if (bNeedsAddToView)
			{
				bNeedsAddToView = false;
				[[IOSAppDelegate GetDelegate].IOSView addSubview : WebViewContainer];
			}
			else
			{
				if (NextContent != nil)
				{
					// Load web content from string
					[self.WebView loadHTMLString : NextContent baseURL : NextURL];
					NextContent = nil;
					NextURL = nil;
				}
				else
					if (NextURL != nil)
					{
						// Load web content from URL
						NSURLRequest *nsrequest = [NSURLRequest requestWithURL : NextURL];
						[self.WebView loadRequest : nsrequest];
						NextURL = nil;
					}
			}
		}
	});
#endif
}

-(NSString *)UrlDecode:(NSString *)stringToDecode
{
	NSString *result = [stringToDecode stringByReplacingOccurrencesOfString : @"+" withString:@" "];
	result = [result stringByRemovingPercentEncoding];
	return result;
}

#if !PLATFORM_TVOS
-(void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage : (WKScriptMessage *)message
{
	if ([message.body isKindOfClass : [NSString class]])
	{
		NSString *Message = message.body;
		if (Message != nil)
		{
			//NSLog(@"Received message %@", Message);
			WebBrowserWidget->ProcessScriptMessage(Message);
		}

	}
}
#endif

-(void)executejavascript:(NSString*)InJavaScript
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
	//	NSLog(@"executejavascript %@", InJavaScript);
		[self.WebView evaluateJavaScript : InJavaScript completionHandler : nil];
	});
#endif
}

-(void)loadurl:(NSURL*)InURL;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		self.NextURL = InURL;
	});
}

-(void)loadstring:(NSString*)InString dummyurl : (NSURL*)InURL;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		self.NextContent = InString;
		self.NextURL = InURL;
	});
}

-(void)set3D:(bool)InIsIOS3DBrowser;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (IsIOS3DBrowser != InIsIOS3DBrowser)
		{
			//default is 2D
			IsIOS3DBrowser = InIsIOS3DBrowser;
			[self setDefaultVisibility];
		}
	});
}

-(void)setDefaultVisibility;
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (IsIOS3DBrowser)
		{
			[self.WebViewContainer setHidden : YES];
		}
		else
		{
			[self.WebViewContainer setHidden : NO];
		}
	});
#endif
}

-(void)setVisibility:(bool)InIsVisible;
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (InIsVisible)
		{
			[self setDefaultVisibility];
		}
		else
		{
			[self.WebViewContainer setHidden : YES];
		}
	});
#endif
}

-(void)stopLoading;
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebView stopLoading];
	});
#endif
}

-(void)reload;
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebView reload];
	});
#endif
}

-(void)goBack;
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebView goBack];
	});
#endif
}

-(void)goForward;
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebView goForward];
	});
#endif
}

-(bool)canGoBack;
{
#if PLATFORM_TVOS
	return false;
#else
	return [self.WebView canGoBack];
#endif
}

-(bool)canGoForward;
{
#if PLATFORM_TVOS
	return false;
#else
	return [self.WebView canGoForward];
#endif
}

-(FTextureRHIRef)GetVideoTexture;
{
	return VideoTexture;
}

-(void)SetVideoTexture:(FTextureRHIRef)Texture;
{
	VideoTexture = Texture;
}

-(void)SetVideoTextureValid:(bool)Condition;
{
	bVideoTextureValid = Condition;
}

-(bool)IsVideoTextureValid;
{
	return bVideoTextureValid;
}

-(bool)UpdateVideoFrame:(void*)ptr;
{
#if !PLATFORM_TVOS
	@synchronized(self) // Briefly block render thread
	{
		id<MTLTexture> ptrToMetalTexture = (id<MTLTexture>)ptr;
		NSUInteger width = [ptrToMetalTexture width];
		NSUInteger height = [ptrToMetalTexture height];

		[self updateWebViewMetalTexture : ptrToMetalTexture];
	}
#endif
	return true;
}

-(void)updateWebViewMetalTexture:(id<MTLTexture>)texture
{
#if !PLATFORM_TVOS
	@autoreleasepool {
		UIGraphicsBeginImageContextWithOptions(WebView.frame.size, NO, 1.0f);
		[WebView drawViewHierarchyInRect : WebView.bounds afterScreenUpdates : NO];
		UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
		UIGraphicsEndImageContext();
		NSUInteger width = [texture width];
		NSUInteger height = [texture height];
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, 4 * width, colorSpace, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
		CGContextDrawImage(context, CGRectMake(0, 0, width, height), image.CGImage);
		[texture replaceRegion : MTLRegionMake2D(0, 0, width, height)
			mipmapLevel : 0
			withBytes : CGBitmapContextGetData(context)
			bytesPerRow : 4 * width];
		CGColorSpaceRelease(colorSpace);
		CGContextRelease(context);
		image = nil;
	}
#endif
}

#if !PLATFORM_TVOS
- (void)webView:(WKWebView*)InWebView decidePolicyForNavigationAction : (WKNavigationAction*)InNavigationAction decisionHandler : (void(^)(WKNavigationActionPolicy))InDecisionHandler
{
	NSURLRequest *request = InNavigationAction.request;
	FString UrlStr([[request URL]absoluteString]);
	
	WebBrowserWidget->HandleShouldOverrideUrlLoading(UrlStr);
	InDecisionHandler(WKNavigationActionPolicyAllow);
}

-(void)webView:(WKWebView *)InWebView didCommitNavigation : (WKNavigation *)InNavigation
{
	NSString* CurrentUrl = [self.WebView URL].absoluteString;
	NSString* Title = [self.WebView title];
	
//	NSLog(@"didCommitNavigation: %@", CurrentUrl);
	WebBrowserWidget->HandleReceivedTitle(Title);
	WebBrowserWidget->HandlePageLoad(CurrentUrl, true);
}

-(void)webView:(WKWebView *)InWebView didFinishNavigation : (WKNavigation *)InNavigation
{
	NSString* CurrentUrl = [self.WebView URL].absoluteString;
	NSString* Title = [self.WebView title];
	// NSLog(@"didFinishNavigation: %@", CurrentUrl);
	WebBrowserWidget->HandleReceivedTitle(Title);
	WebBrowserWidget->HandlePageLoad(CurrentUrl, false);
}
-(void)webView:(WKWebView *)InWebView didFailNavigation : (WKNavigation *)InNavigation withError : (NSError*)InError
{
	if (InError.domain == NSURLErrorDomain && InError.code == NSURLErrorCancelled)
	{
		//ignore this one, interrupted load
		return;
	}
	NSString* CurrentUrl = [InError.userInfo objectForKey : @"NSErrorFailingURLStringKey"];
//	NSLog(@"didFailNavigation: %@, error %@", CurrentUrl, InError);
	WebBrowserWidget->HandleReceivedError(InError.code, CurrentUrl);
}
#endif
@end

namespace {
	static const FString JSGetSourceCommand = TEXT("GetSource");
	static const FString JSMessageGetSourceScript =
		TEXT("	window.webkit.messageHandlers.") + FMobileJSScripting::JSMessageHandler + TEXT(".postMessage('")+ JSGetSourceCommand +
		TEXT("/' + encodeURIComponent(document.documentElement.innerHTML));");

}

FWebBrowserWindow::FWebBrowserWindow(FString InUrl, TOptional<FString> InContentsToLoad, bool InShowErrorMessage, bool InThumbMouseButtonNavigation, bool InUseTransparency, bool bInJSBindingToLoweringEnabled)
	: CurrentUrl(MoveTemp(InUrl))
	, ContentsToLoad(MoveTemp(InContentsToLoad))
	, bUseTransparency(InUseTransparency)
	, DocumentState(EWebBrowserDocumentState::NoDocument)
	, ErrorCode(0)
	, Scripting(new FMobileJSScripting(bInJSBindingToLoweringEnabled))
	, IOSWindowSize(FIntPoint(500, 500))
	, bIsDisabled(false)
	, bIsVisible(true)
	, bTickedLastFrame(true)
{
}

FWebBrowserWindow::~FWebBrowserWindow()
{
	CloseBrowser(true, false);
}

void FWebBrowserWindow::LoadURL(FString NewURL)
{
	BrowserWidget->LoadURL(NewURL);
}

void FWebBrowserWindow::LoadString(FString Contents, FString DummyURL)
{
	BrowserWidget->LoadString(Contents, DummyURL);
}

TSharedRef<SWidget> FWebBrowserWindow::CreateWidget()
{
	TSharedRef<SIOSWebBrowserWidget> BrowserWidgetRef =
		SNew(SIOSWebBrowserWidget)
		.UseTransparency(bUseTransparency)
		.InitialURL(CurrentUrl)
		.WebBrowserWindow(SharedThis(this));

	BrowserWidget = BrowserWidgetRef;

	Scripting->SetWindow(SharedThis(this));

	return BrowserWidgetRef;
}

void FWebBrowserWindow::SetViewportSize(FIntPoint WindowSize, FIntPoint WindowPos)
{
	IOSWindowSize = WindowSize;
}

FIntPoint FWebBrowserWindow::GetViewportSize() const
{
	return IOSWindowSize;
}

FSlateShaderResource* FWebBrowserWindow::GetTexture(bool bIsPopup /*= false*/)
{
	return nullptr;
}

bool FWebBrowserWindow::IsValid() const
{
	return false;
}

bool FWebBrowserWindow::IsInitialized() const
{
	return true;
}

bool FWebBrowserWindow::IsClosing() const
{
	return false;
}

EWebBrowserDocumentState FWebBrowserWindow::GetDocumentLoadingState() const
{
	return DocumentState;
}

FString FWebBrowserWindow::GetTitle() const
{
	return Title;
}

FString FWebBrowserWindow::GetUrl() const
{
	return CurrentUrl;
}

bool FWebBrowserWindow::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyUp(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyChar(const FCharacterEvent& InCharacterEvent)
{
	return false;
}

FReply FWebBrowserWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
}

void FWebBrowserWindow::SetSupportsMouseWheel(bool bValue)
{

}

bool FWebBrowserWindow::GetSupportsMouseWheel() const
{
	return false;
}

FReply FWebBrowserWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}


void FWebBrowserWindow::OnFocus(bool SetFocus, bool bIsPopup)
{
}

void FWebBrowserWindow::OnCaptureLost()
{
}

bool FWebBrowserWindow::CanGoBack() const
{
	return BrowserWidget->CanGoBack();
}

void FWebBrowserWindow::GoBack()
{
	BrowserWidget->GoBack();
}

bool FWebBrowserWindow::CanGoForward() const
{
	return BrowserWidget->CanGoForward();
}

void FWebBrowserWindow::GoForward()
{
	BrowserWidget->GoForward();
}

bool FWebBrowserWindow::IsLoading() const
{
	return DocumentState != EWebBrowserDocumentState::Loading;
}

void FWebBrowserWindow::Reload()
{
	BrowserWidget->Reload();
}

void FWebBrowserWindow::StopLoad()
{
	BrowserWidget->StopLoad();
}

void FWebBrowserWindow::GetSource(TFunction<void(const FString&)> Callback) const
{
	//@todo: decide what to do about multiple pending requests
	GetPageSourceCallback.Emplace(Callback);

	// Ugly hack: Work around the fact that ExecuteJavascript is non-const.
	const_cast<FWebBrowserWindow*>(this)->ExecuteJavascript(JSMessageGetSourceScript);
}

int FWebBrowserWindow::GetLoadError()
{
	return ErrorCode;
}

void FWebBrowserWindow::NotifyDocumentError(const FString& InCurrentUrl, int InErrorCode)
{
	if (!CurrentUrl.Equals(InCurrentUrl, ESearchCase::CaseSensitive))
	{
		CurrentUrl = InCurrentUrl;
		UrlChangedEvent.Broadcast(CurrentUrl);
	}

	ErrorCode = InErrorCode;
	DocumentState = EWebBrowserDocumentState::Error;
	DocumentStateChangedEvent.Broadcast(DocumentState);
}

void FWebBrowserWindow::NotifyDocumentLoadingStateChange(const FString& InCurrentUrl, bool IsLoading)
{
	// Ignore a load completed notification if there was an error.
	// For load started, reset any errors from previous page load.
	if (IsLoading || DocumentState != EWebBrowserDocumentState::Error)
	{
		if (!CurrentUrl.Equals(InCurrentUrl, ESearchCase::CaseSensitive))
		{
			CurrentUrl = InCurrentUrl;
			UrlChangedEvent.Broadcast(CurrentUrl);
		}

		if (!IsLoading && !InCurrentUrl.StartsWith("javascript:"))
		{
			Scripting->PageLoaded(SharedThis(this));
		}
		ErrorCode = 0;
		DocumentState = IsLoading
			? EWebBrowserDocumentState::Loading
			: EWebBrowserDocumentState::Completed;
		DocumentStateChangedEvent.Broadcast(DocumentState);
	}

}

void FWebBrowserWindow::SetIsDisabled(bool bValue)
{
	bIsDisabled = bValue;
}

TSharedPtr<SWindow> FWebBrowserWindow::GetParentWindow() const
{
	return ParentWindow;
}

void FWebBrowserWindow::SetParentWindow(TSharedPtr<SWindow> Window)
{
	ParentWindow = Window;
}

void FWebBrowserWindow::ExecuteJavascript(const FString& Script)
{
	BrowserWidget->ExecuteJavascript(Script);
}

void FWebBrowserWindow::CloseBrowser(bool bForce, bool bBlockTillClosed /* ignored */)
{
	BrowserWidget->Close();
}

bool FWebBrowserWindow::OnJsMessageReceived(const FString& Command, const TArray<FString>& Params, const FString& Origin)
{
	if (Command.Equals(JSGetSourceCommand, ESearchCase::CaseSensitive) && GetPageSourceCallback.IsSet() && Params.Num() == 1)
	{
		GetPageSourceCallback.GetValue()(Params[0]);
		GetPageSourceCallback.Reset();
		return true;
	}
	return Scripting->OnJsMessageReceived(Command, Params, Origin);
}

void FWebBrowserWindow::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent /*= true*/)
{
	Scripting->BindUObject(Name, Object, bIsPermanent);
}

void FWebBrowserWindow::UnbindUObject(const FString& Name, UObject* Object /*= nullptr*/, bool bIsPermanent /*= true*/)
{
	Scripting->UnbindUObject(Name, Object, bIsPermanent);
}

void FWebBrowserWindow::CheckTickActivity()
{
	if (bIsVisible != bTickedLastFrame)
	{
		bIsVisible = bTickedLastFrame;
		BrowserWidget->SetWebBrowserVisibility(bIsVisible);
	}

	bTickedLastFrame = false;
}

void FWebBrowserWindow::SetTickLastFrame()
{
	bTickedLastFrame = !bIsDisabled;
}

bool FWebBrowserWindow::IsVisible()
{
	return bIsVisible;
}

#endif
