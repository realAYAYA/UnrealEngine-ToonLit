// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal;

import android.os.Build;
import android.os.Bundle;
import android.content.Context;

import android.view.View;
import android.view.ViewGroup;
import android.view.KeyEvent;
import android.webkit.ConsoleMessage;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.JsResult;
import android.webkit.JsPromptResult;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Paint;
import android.graphics.Color;
import android.webkit.WebBackForwardList;
import java.io.ByteArrayInputStream;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Semaphore;

import android.util.Log;
import android.opengl.*;
import android.view.Surface;	
import android.graphics.Canvas;
import android.graphics.SurfaceTexture;

import android.opengl.*;
import android.widget.LinearLayout;
import android.util.AttributeSet;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.Matrix;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.os.Message;



// Simple layout to apply absolute positioning for the WebView
class WebViewPositionLayout extends ViewGroup
{
	public WebViewPositionLayout(Context context, WebViewControl inWebViewControl)
	{
        super(context);
		webViewControl = inWebViewControl;
	}

	@Override
	protected void onLayout(boolean changed, int left, int top, int right, int bottom)
	{
		webViewControl.webView.layout(webViewControl.curX,webViewControl.curY,webViewControl.curX+webViewControl.curW,webViewControl.curY+webViewControl.curH);
	}

	private WebViewControl webViewControl;
}

// Wrapper for the layout and WebView for the C++ to call
class WebViewControl
{

	private static int initialWidth = 500;
	private static int initialHeight = 500;
	private static final String TAG = "WebViewControl";

	private boolean SwizzlePixels = true;
	private boolean VulkanRenderer = false;
	private volatile boolean WaitOnBitmapRender = false;

	private Bitmap TargetBitmap = null;
	private Canvas TargetCanvas = null;
	private java.nio.Buffer TargetBitmapFrameData = null;


	private BitmapRenderer mBitmapRenderer = null;
	private OESTextureRenderer mOESTextureRenderer = null;

	public class FrameUpdateInfo 
	{
		public java.nio.Buffer Buffer;
		public Bitmap Bitmap;
		public boolean FrameReady;
		public boolean RegionChanged;
		public float UScale;
		public float UOffset;
		public float VScale;
		public float VOffset;
	}

	public WebViewControl(long inNativePtr, int width, int height, boolean swizzlePixels, boolean vulkanRenderer, final boolean bEnableRemoteDebugging, final boolean bUseTransparency, final boolean bEnableDomStorage, final boolean bShouldUseBitmapRender)
	{
		final WebViewControl w = this;

		initialWidth = width;
		initialHeight = height;
		SwizzlePixels = swizzlePixels;
		VulkanRenderer = vulkanRenderer;
		WaitOnBitmapRender = false;

		nativePtr = inNativePtr;

		if (bShouldUseBitmapRender)
		{
			TargetBitmap = Bitmap.createBitmap(initialWidth, initialHeight, Config.ARGB_8888);
			TargetCanvas = new Canvas(TargetBitmap);
		}
		else
		{
			TargetBitmap = null;
			TargetCanvas = null;
		}

		//GameActivity.Log.debug("WebViewControl width=" + width + ", height=" + height);

		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				// enable remote debugging if requested and supported by the current platform
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
				{
					WebView.setWebContentsDebuggingEnabled(bEnableRemoteDebugging && !GameActivity.Get().nativeIsShippingBuild());
				}
				
				// create the WebView
				webView = new GLWebView(GameActivity.Get());
				webView.setWebViewClient(new ViewClient());
				webView.setWebChromeClient(new ChromeClient());
				webView.getSettings().setJavaScriptEnabled(true);
				webView.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
				webView.getSettings().setAllowFileAccess( true );
				webView.getSettings().setAllowContentAccess( true );
				webView.getSettings().setAllowFileAccessFromFileURLs(true);
				webView.getSettings().setAllowUniversalAccessFromFileURLs(true);
				webView.getSettings().setSupportMultipleWindows(true);
				webView.getSettings().setDomStorageEnabled(bEnableDomStorage);
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
				{
					webView.getSettings().setMixedContentMode(0); // 0 = MIXED_CONTENT_ALWAYS_ALLOW
				}

				webView.getSettings().setCacheMode( WebSettings.LOAD_DEFAULT );
				webView.getSettings().setLoadWithOverviewMode(true);
				webView.getSettings().setUseWideViewPort(true);

				//3D is the default if not using Bitmap
				webView.SetBitmapDraw(TargetCanvas);
				webView.SetAndroid3DBrowser(TargetBitmap == null);

				if (bUseTransparency)
				{
					webView.setBackgroundColor(Color.TRANSPARENT);
				}

				// Wrap the webview in a layout that will do absolute positioning for us
				positionLayout = new WebViewPositionLayout(GameActivity.Get(), w);
				ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.FILL_PARENT, ViewGroup.LayoutParams.FILL_PARENT);

				positionLayout.addView(webView, params);

				bShown = false;
				NextURL = null;
				NextContent = null;
				curX = curY = curW = curH = 0;
			}
		});
	}
	
	boolean PendingSetAndroid3DBrowser;
	public void SetAndroid3DBrowser(boolean InIsAndroid3DBrowser)
	{
		PendingSetAndroid3DBrowser = (TargetBitmap == null) ? InIsAndroid3DBrowser : false;
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.SetAndroid3DBrowser(PendingSetAndroid3DBrowser);
			}
		});
	}

	boolean PendingSetVisibility;
	public void SetVisibility(boolean InIsVisible)
	{
		PendingSetVisibility = InIsVisible;
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.setVisibility(PendingSetVisibility? View.VISIBLE: View.GONE);
			}
		});
	}
	
	public boolean didResolutionChange()
	{
		if (null != mOESTextureRenderer)
		{
			return mOESTextureRenderer.resolutionChanged();
		}
		if (null != mBitmapRenderer)
		{
			return mBitmapRenderer.resolutionChanged();
		}
		return false;
	}

	public void release()
	{
		if (null != mOESTextureRenderer)
		{
			while (WaitOnBitmapRender) ;
			releaseOESTextureRenderer();
		}
		if (null != mBitmapRenderer)
		{
			while (WaitOnBitmapRender) ;
			releaseBitmapRenderer();
		}

		if (TargetCanvas != null)
		{
			TargetCanvas = null;
		}
		if (TargetBitmap != null)
		{
			TargetBitmap.recycle();
			TargetBitmap = null;
		}

		Close();
	}

	public void ExecuteJavascript(final String script)
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if(webView != null)
				{
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
					{
						webView.evaluateJavascript(script, null);
					}
					else
					{
						// This is executed directly here instead of setting NextURL as otherwise calling ExecuteJavascript would only be possible once per tick.
						webView.loadUrl("javascript:"+script);
					}
				}
			}
		});
	}
	public void LoadURL(final String url)
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				NextURL = url;
				NextContent = null;
			}
		});
	}

	public void LoadString(final String contents, final String url)
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				NextURL = url;
				NextContent = contents;
			}
		});
	}

	public void StopLoad()
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.stopLoading();
			}
		});
	}

	public void Reload()
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.reload();
			}
		});
	}

	public void GoBackOrForward(final int Steps)
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.goBackOrForward(Steps);
			}
		});
	}

	public boolean CanGoBackOrForward(int Steps)
	{
		return webView.canGoBackOrForward(Steps);
	}

	public void SendTouchEvent(int event, float x, float y)
	{
		final int actionType = event;
		final long actionTime = SystemClock.uptimeMillis();
		final float actionX = webView.getLeft() + (x * webView.getWidth());
		final float actionY = webView.getTop() + (y * webView.getHeight());
		//GameActivity.Log.debug("SendTouchEvent(event=" + event + ", x=" + x + ", y=" + y + ") = " + actionX + ", " + actionY);
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				long eventTime = SystemClock.uptimeMillis();
				webView.onTouchEvent(MotionEvent.obtain(
					actionTime,
					eventTime,
					actionType,
					actionX,
					actionY,
					0
				));
			}
		});
	}

	private static class KeyTableData
	{
		public final int key;
		public final int meta;

		KeyTableData (int keycode, int metadata)
		{
			key = keycode;
			meta = metadata;
		}
	}
	
	private static final KeyTableData[] KeyTableList = {
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 0
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 1
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 2
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 3
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 4
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 5
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 6
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 7
		new KeyTableData(KeyEvent.KEYCODE_DEL, 0),	// 8
		new KeyTableData(KeyEvent.KEYCODE_TAB, 0),	// 9
		new KeyTableData(KeyEvent.KEYCODE_ENTER, 0),	// 10
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 11
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 12
		new KeyTableData(KeyEvent.KEYCODE_ENTER, 0),	// 13
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 14
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 15
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 16
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 17
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 18
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 19
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 20
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 21
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 22
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 23
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 24
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 25
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 26
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 27
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 28
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 29
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 30
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0),	// 31
		new KeyTableData(KeyEvent.KEYCODE_SPACE, 0),	// 32
		new KeyTableData(KeyEvent.KEYCODE_1, KeyEvent.META_SHIFT_ON),	// 33
		new KeyTableData(KeyEvent.KEYCODE_APOSTROPHE, KeyEvent.META_SHIFT_ON),	// 34
		new KeyTableData(KeyEvent.KEYCODE_POUND, 0),	// 35
		new KeyTableData(KeyEvent.KEYCODE_4, KeyEvent.META_SHIFT_ON),	// 36
		new KeyTableData(KeyEvent.KEYCODE_5, KeyEvent.META_SHIFT_ON),	// 37
		new KeyTableData(KeyEvent.KEYCODE_7, KeyEvent.META_SHIFT_ON),	// 38
		new KeyTableData(KeyEvent.KEYCODE_APOSTROPHE, 0),	// 39
		new KeyTableData(KeyEvent.KEYCODE_NUMPAD_LEFT_PAREN, 0),	// 40
		new KeyTableData(KeyEvent.KEYCODE_NUMPAD_RIGHT_PAREN, 0),	// 41
		new KeyTableData(KeyEvent.KEYCODE_STAR, 0),	// 42
		new KeyTableData(KeyEvent.KEYCODE_PLUS, 0),	// 43
		new KeyTableData(KeyEvent.KEYCODE_COMMA, 0),	// 44
		new KeyTableData(KeyEvent.KEYCODE_MINUS, 0),	// 45
		new KeyTableData(KeyEvent.KEYCODE_PERIOD, 0),	// 46
		new KeyTableData(KeyEvent.KEYCODE_SLASH, 0),	// 47
		new KeyTableData(KeyEvent.KEYCODE_0, 0),	// 48
		new KeyTableData(KeyEvent.KEYCODE_1, 0),	// 49
		new KeyTableData(KeyEvent.KEYCODE_2, 0),	// 50
		new KeyTableData(KeyEvent.KEYCODE_3, 0),	// 51
		new KeyTableData(KeyEvent.KEYCODE_4, 0),	// 52
		new KeyTableData(KeyEvent.KEYCODE_5, 0),	// 53
		new KeyTableData(KeyEvent.KEYCODE_6, 0),	// 54
		new KeyTableData(KeyEvent.KEYCODE_7, 0),	// 55
		new KeyTableData(KeyEvent.KEYCODE_8, 0),	// 56
		new KeyTableData(KeyEvent.KEYCODE_9, 0),	// 57
		new KeyTableData(KeyEvent.KEYCODE_SEMICOLON, KeyEvent.META_SHIFT_ON),	// 58
		new KeyTableData(KeyEvent.KEYCODE_SEMICOLON, 0),	// 59
		new KeyTableData(KeyEvent.KEYCODE_COMMA, KeyEvent.META_SHIFT_ON),	// 60
		new KeyTableData(KeyEvent.KEYCODE_EQUALS, 0),	// 61
		new KeyTableData(KeyEvent.KEYCODE_PERIOD, KeyEvent.META_SHIFT_ON),	// 62
		new KeyTableData(KeyEvent.KEYCODE_SLASH, KeyEvent.META_SHIFT_ON),	// 63
		new KeyTableData(KeyEvent.KEYCODE_AT, 0),	// 64
		new KeyTableData(KeyEvent.KEYCODE_A, KeyEvent.META_SHIFT_ON),	// 65
		new KeyTableData(KeyEvent.KEYCODE_B, KeyEvent.META_SHIFT_ON),	// 66
		new KeyTableData(KeyEvent.KEYCODE_C, KeyEvent.META_SHIFT_ON),	// 67
		new KeyTableData(KeyEvent.KEYCODE_D, KeyEvent.META_SHIFT_ON),	// 68
		new KeyTableData(KeyEvent.KEYCODE_E, KeyEvent.META_SHIFT_ON),	// 69
		new KeyTableData(KeyEvent.KEYCODE_F, KeyEvent.META_SHIFT_ON),	// 70
		new KeyTableData(KeyEvent.KEYCODE_G, KeyEvent.META_SHIFT_ON),	// 71
		new KeyTableData(KeyEvent.KEYCODE_H, KeyEvent.META_SHIFT_ON),	// 72
		new KeyTableData(KeyEvent.KEYCODE_I, KeyEvent.META_SHIFT_ON),	// 73
		new KeyTableData(KeyEvent.KEYCODE_J, KeyEvent.META_SHIFT_ON),	// 74
		new KeyTableData(KeyEvent.KEYCODE_K, KeyEvent.META_SHIFT_ON),	// 75
		new KeyTableData(KeyEvent.KEYCODE_L, KeyEvent.META_SHIFT_ON),	// 76
		new KeyTableData(KeyEvent.KEYCODE_M, KeyEvent.META_SHIFT_ON),	// 77
		new KeyTableData(KeyEvent.KEYCODE_N, KeyEvent.META_SHIFT_ON),	// 78
		new KeyTableData(KeyEvent.KEYCODE_O, KeyEvent.META_SHIFT_ON),	// 79
		new KeyTableData(KeyEvent.KEYCODE_P, KeyEvent.META_SHIFT_ON),	// 80
		new KeyTableData(KeyEvent.KEYCODE_Q, KeyEvent.META_SHIFT_ON),	// 81
		new KeyTableData(KeyEvent.KEYCODE_R, KeyEvent.META_SHIFT_ON),	// 82
		new KeyTableData(KeyEvent.KEYCODE_S, KeyEvent.META_SHIFT_ON),	// 83
		new KeyTableData(KeyEvent.KEYCODE_T, KeyEvent.META_SHIFT_ON),	// 84
		new KeyTableData(KeyEvent.KEYCODE_U, KeyEvent.META_SHIFT_ON),	// 85
		new KeyTableData(KeyEvent.KEYCODE_V, KeyEvent.META_SHIFT_ON),	// 86
		new KeyTableData(KeyEvent.KEYCODE_W, KeyEvent.META_SHIFT_ON),	// 87
		new KeyTableData(KeyEvent.KEYCODE_X, KeyEvent.META_SHIFT_ON),	// 88
		new KeyTableData(KeyEvent.KEYCODE_Y, KeyEvent.META_SHIFT_ON),	// 89
		new KeyTableData(KeyEvent.KEYCODE_Z, KeyEvent.META_SHIFT_ON),	// 90
		new KeyTableData(KeyEvent.KEYCODE_LEFT_BRACKET, 0),	// 91
		new KeyTableData(KeyEvent.KEYCODE_BACKSLASH, 0),	// 92
		new KeyTableData(KeyEvent.KEYCODE_RIGHT_BRACKET, 0),	// 93
		new KeyTableData(KeyEvent.KEYCODE_6, KeyEvent.META_SHIFT_ON),	// 94
		new KeyTableData(KeyEvent.KEYCODE_MINUS, KeyEvent.META_SHIFT_ON),	// 95
		new KeyTableData(KeyEvent.KEYCODE_GRAVE, 0),	// 96
		new KeyTableData(KeyEvent.KEYCODE_A, 0),	// 97
		new KeyTableData(KeyEvent.KEYCODE_B, 0),	// 98
		new KeyTableData(KeyEvent.KEYCODE_C, 0),	// 99
		new KeyTableData(KeyEvent.KEYCODE_D, 0),	// 100
		new KeyTableData(KeyEvent.KEYCODE_E, 0),	// 101
		new KeyTableData(KeyEvent.KEYCODE_F, 0),	// 102
		new KeyTableData(KeyEvent.KEYCODE_G, 0),	// 103
		new KeyTableData(KeyEvent.KEYCODE_H, 0),	// 104
		new KeyTableData(KeyEvent.KEYCODE_I, 0),	// 105
		new KeyTableData(KeyEvent.KEYCODE_J, 0),	// 106
		new KeyTableData(KeyEvent.KEYCODE_K, 0),	// 107
		new KeyTableData(KeyEvent.KEYCODE_L, 0),	// 108
		new KeyTableData(KeyEvent.KEYCODE_M, 0),	// 109
		new KeyTableData(KeyEvent.KEYCODE_N, 0),	// 110
		new KeyTableData(KeyEvent.KEYCODE_O, 0),	// 111
		new KeyTableData(KeyEvent.KEYCODE_P, 0),	// 112
		new KeyTableData(KeyEvent.KEYCODE_Q, 0),	// 113
		new KeyTableData(KeyEvent.KEYCODE_R, 0),	// 114
		new KeyTableData(KeyEvent.KEYCODE_S, 0),	// 115
		new KeyTableData(KeyEvent.KEYCODE_T, 0),	// 116
		new KeyTableData(KeyEvent.KEYCODE_U, 0),	// 117
		new KeyTableData(KeyEvent.KEYCODE_V, 0),	// 118
		new KeyTableData(KeyEvent.KEYCODE_W, 0),	// 119
		new KeyTableData(KeyEvent.KEYCODE_X, 0),	// 120
		new KeyTableData(KeyEvent.KEYCODE_Y, 0),	// 121
		new KeyTableData(KeyEvent.KEYCODE_Z, 0),	// 122
		new KeyTableData(KeyEvent.KEYCODE_LEFT_BRACKET, KeyEvent.META_SHIFT_ON),	// 123
		new KeyTableData(KeyEvent.KEYCODE_BACKSLASH, KeyEvent.META_SHIFT_ON),	// 124
		new KeyTableData(KeyEvent.KEYCODE_RIGHT_BRACKET, KeyEvent.META_SHIFT_ON),	// 125
		new KeyTableData(KeyEvent.KEYCODE_GRAVE, KeyEvent.META_SHIFT_ON),	// 126
		new KeyTableData(KeyEvent.KEYCODE_UNKNOWN, 0)	// 127
	};

	public boolean SendKeyEvent(boolean bDown, int keycode)
	{
		if (keycode < 0 || keycode > 127)
		{
			return false;
		}

		final int actionType = bDown ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP;
		final long actionTime = SystemClock.uptimeMillis();
		final int actionCode = KeyTableList[keycode].key;
		final int actionMeta = KeyTableList[keycode].meta;
//		GameActivity.Log.debug("SendTouchEvent(event=" + (bDown ? "Down" : "Up") + ", key=" + keycode + ", code=" + actionCode + ", meta=" + actionMeta);
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				long eventTime = SystemClock.uptimeMillis();
				webView.dispatchKeyEvent(new KeyEvent(actionTime, eventTime, actionType, actionCode, 0, actionMeta));
			}
		});
		return true;
	}

	// called from C++ paint event
	public void Update(final int x, final int y, final int width, final int height)
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (bClosed)
				{
					// queued up run()s can occur after Close() called; don't want to show it again
					return;
				}
				if (!bShown)
				{
					bShown = true;
					
					// add to the activitiy, on top of the SurfaceView
					ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.FILL_PARENT, ViewGroup.LayoutParams.FILL_PARENT);			
					GameActivity.Get().addContentView(positionLayout, params);
					if(!webView.IsAndroid3DBrowser && TargetBitmap == null)
					{
						//GameActivity.Log.warn("request focus create");
						webView.requestFocus();
					}
				}
				else
				{
					if(webView != null)
					{
						if(NextContent != null)
						{
							webView.loadDataWithBaseURL(NextURL, NextContent, "text/html", "UTF-8", null);
							NextURL = null;
							NextContent = null;
						}
						else
						if(NextURL != null)
						{

							int colPos = NextURL.indexOf(':');

							boolean bNeedsPrefix = colPos < 0;;

							if(!bNeedsPrefix  && !NextURL.equalsIgnoreCase("about:blank"))
							{
								try
								{
									String UrlAddress = NextURL.substring(colPos + 1);

									//check if the address contains only numbers
									bNeedsPrefix = UrlAddress.matches("[0-9]+"); // it's a port number, and URLs like "google.com:80" also need the "http://" prefix

									//try to correct malformed protocols, like "http:www.google.com"
									if(!bNeedsPrefix)
									{
										String UrlProtocol = NextURL.substring(0, colPos);
																			
										if((NextURL.equalsIgnoreCase("http") || NextURL.equalsIgnoreCase("https")) && !UrlAddress.startsWith("/"))
										{
											NextURL = UrlProtocol + "://" + UrlAddress;
										}
									}

								}
								catch(IndexOutOfBoundsException e)
								{}
							}

							if(bNeedsPrefix)
							{
								//default scheme is http://
								NextURL = "http://" + NextURL;
							}

							webView.loadUrl(NextURL);
							NextURL = null;
						}
						else
						if(!webView.IsAndroid3DBrowser && TargetBitmap == null)
						{
							//GameActivity.Log.warn("request focus");
							webView.requestFocus();
						}

					}		
				}
				if( (webView != null) && (x != curX || y != curY || width != curW || height != curH))
				{
					curX = x;
					curY = y;
					curW = width;
					curH = height;
					positionLayout.requestLayout();
				}
			}
		});
	}

	public void Close()
	{
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (bShown)
				{
					ViewGroup parent = (ViewGroup)webView.getParent();
					if (parent != null)
					{
						parent.removeView(webView);
					}
					parent = (ViewGroup)positionLayout.getParent();
					if (parent != null)
					{
						parent.removeView(positionLayout);
					}
					bShown = false;
				}
				bClosed = true;
			}
		});
	}

	
	// ======================================================================================
	private boolean CreateBitmapRenderer()
	{
		releaseBitmapRenderer();

		mBitmapRenderer = new BitmapRenderer(SwizzlePixels, VulkanRenderer);
		if (!mBitmapRenderer.isValid())
		{
			mBitmapRenderer = null;
			return false;
		}
		
		// set this here as the size may have been set before the GL resources were created.
		mBitmapRenderer.setSize(initialWidth, initialHeight);
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.setSurface(mBitmapRenderer.getSurface());
			}
		});

		return true;
	}

	void releaseBitmapRenderer()
	{
		if (null != mBitmapRenderer)
		{
			mBitmapRenderer.release();
			mBitmapRenderer = null;
		}
	}

	public void initBitmapRenderer()
	{
		// if not already allocated.
		// Create bitmap renderer's gl resources in the renderer thread.
		if (null == mBitmapRenderer)
		{
			if (!CreateBitmapRenderer())
			{
				GameActivity.Log.warn("initBitmapRenderer failed to alloc mBitmapRenderer ");
				release();
			  }
		}
	}

	public FrameUpdateInfo getVideoLastFrameBitmap()
	{
		if (TargetBitmap == null)
		{
			return null;
		}

		// trigger a draw (it will go to onDraw to do the actual update)
		webView.draw(TargetCanvas);

		FrameUpdateInfo frameUpdateInfo = new FrameUpdateInfo();

		frameUpdateInfo.Bitmap = TargetBitmap;
		frameUpdateInfo.FrameReady = true;
		frameUpdateInfo.RegionChanged = false;
		return frameUpdateInfo;
	}

	public FrameUpdateInfo getVideoLastFrameData()
	{
		if (TargetBitmap != null)
		{
			if (null == TargetBitmapFrameData)
			{
				TargetBitmapFrameData = java.nio.ByteBuffer.allocateDirect(initialWidth * initialHeight * 4);
			}

			// note RGBA vs BGRA so not really correct
			TargetBitmapFrameData.position(0);
			TargetBitmap.copyPixelsToBuffer(TargetBitmapFrameData);
			TargetBitmapFrameData.position(0);

			FrameUpdateInfo frameUpdateInfo = new FrameUpdateInfo();

			frameUpdateInfo.Buffer = TargetBitmapFrameData;
			frameUpdateInfo.FrameReady = true;
			frameUpdateInfo.RegionChanged = false;
			return frameUpdateInfo;
		}

		initBitmapRenderer();
		if (null != mBitmapRenderer)
		{
			WaitOnBitmapRender = true;
			FrameUpdateInfo frameInfo = mBitmapRenderer.updateFrameData();
			WaitOnBitmapRender = false;
			return frameInfo;
		}
		else
		{
			return null;
		}
	}

	public FrameUpdateInfo getVideoLastFrame(int destTexture)
	{
		initBitmapRenderer();
		if (null != mBitmapRenderer)
		{
			WaitOnBitmapRender = true;
			FrameUpdateInfo frameInfo = mBitmapRenderer.updateFrameData(destTexture);
			WaitOnBitmapRender = false;
			return frameInfo;
		}
		else
		{
			return null;
		}
	}

/*
		All this internal surface view does is manage the
		offscreen bitmap that the media player decoding can
		render into for eventual extraction to the UE4 buffers.
*/
	class BitmapRenderer
		implements android.graphics.SurfaceTexture.OnFrameAvailableListener
	{
		private java.nio.Buffer mFrameData = null;
		private int mLastFramePosition = -1;
		private android.graphics.SurfaceTexture mSurfaceTexture = null;
		private int mTextureWidth = -1;
		private int mTextureHeight = -1;
		private android.view.Surface mSurface = null;
		private boolean mFrameAvailable = false;
		private int mTextureID = -1;
		private int mFBO = -1;
		private int mBlitVertexShaderID = -1;
		private int mBlitFragmentShaderID = -1;
		private float[] mTransformMatrix = new float[16];
		private boolean mTriangleVerticesDirty = true;
		private boolean mTextureSizeChanged = true;
		private boolean mUseOwnContext = true;
		private boolean mVulkanRenderer = false;
		private boolean mSwizzlePixels = false;

		private int GL_TEXTURE_EXTERNAL_OES = 0x8D65;

		private EGLDisplay mEglDisplay;
		private EGLContext mEglContext;
		private EGLSurface mEglSurface;

		private EGLDisplay mSavedDisplay;
		private EGLContext mSavedContext;
		private EGLSurface mSavedSurfaceDraw;
		private EGLSurface mSavedSurfaceRead;

		private boolean mCreatedEGLDisplay = false;

		public BitmapRenderer(boolean swizzlePixels, boolean isVulkan)
		{
			mSwizzlePixels = swizzlePixels;
			mVulkanRenderer = isVulkan;

			mEglSurface = EGL14.EGL_NO_SURFACE;
			mEglContext = EGL14.EGL_NO_CONTEXT;
			mEglDisplay = EGL14.EGL_NO_DISPLAY;
			mUseOwnContext = true;

			if (mVulkanRenderer)
			{
				mSwizzlePixels = true;
			}
			else
			{
				String RendererString = GLES20.glGetString(GLES20.GL_RENDERER);

				// Do not use shared context if Adreno before 400 or on older Android than Marshmallow
				if (RendererString.contains("Adreno (TM) "))
				{
					int AdrenoVersion = Integer.parseInt(RendererString.substring(12));
					if (AdrenoVersion < 400 || android.os.Build.VERSION.SDK_INT < 22)
					{
						GameActivity.Log.debug("WebViewControl: disabled shared GL context on " + RendererString);
						mUseOwnContext = false;
					}
				}
			}

			if (mUseOwnContext)
			{
				initContext();
				saveContext();
				makeCurrent();
				initSurfaceTexture();
				restoreContext();
			}
			else
			{
				initSurfaceTexture();
			}
		}

		private void initContext()
		{
			mEglDisplay = EGL14.EGL_NO_DISPLAY;
			EGLContext shareContext = EGL14.EGL_NO_CONTEXT;

			if (!mVulkanRenderer)
			{
				mEglDisplay = EGL14.eglGetCurrentDisplay();
				shareContext = EGL14.eglGetCurrentContext();
			}
			else
			{
				mEglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
				if (mEglDisplay == EGL14.EGL_NO_DISPLAY)
				{
					GameActivity.Log.error("unable to get EGL14 display");
					return;
				}
				int[] version = new int[2];
				if (!EGL14.eglInitialize(mEglDisplay, version, 0, version, 1))
				{
					mEglDisplay = null;
					GameActivity.Log.error("unable to initialize EGL14 display");
					return;
				}				
				
				mCreatedEGLDisplay = true;
			}

			int[] configSpec = new int[]
			{
				EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
				EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
				EGL14.EGL_NONE
			};
			EGLConfig[] configs = new EGLConfig[1];
			int[] num_config = new int[1];
			EGL14.eglChooseConfig(mEglDisplay, configSpec, 0, configs, 0, 1, num_config, 0);
			int[] contextAttribs = new int[]
			{
				EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
				EGL14.EGL_NONE
			};
			mEglContext = EGL14.eglCreateContext(mEglDisplay, configs[0], shareContext, contextAttribs, 0);

			if (EGL14.eglQueryString(mEglDisplay, EGL14.EGL_EXTENSIONS).contains("EGL_KHR_surfaceless_context"))
			{
				mEglSurface = EGL14.EGL_NO_SURFACE;
			}
			else
			{
				int[] pbufferAttribs = new int[]
				{
					EGL14.EGL_NONE
				};
				mEglSurface = EGL14.eglCreatePbufferSurface(mEglDisplay, configs[0], pbufferAttribs, 0);
			}
		}

		private void saveContext()
		{
			mSavedDisplay = EGL14.eglGetCurrentDisplay();
			mSavedContext = EGL14.eglGetCurrentContext();
			mSavedSurfaceDraw = EGL14.eglGetCurrentSurface(EGL14.EGL_DRAW);
			mSavedSurfaceRead = EGL14.eglGetCurrentSurface(EGL14.EGL_READ);
		}

		private void makeCurrent()
		{
			EGL14.eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext);
		}

		private void restoreContext()
		{
			EGL14.eglMakeCurrent(mSavedDisplay, mSavedSurfaceDraw, mSavedSurfaceRead, mSavedContext);
		}

		private void initSurfaceTexture()
		{
			int[] textures = new int[1];
			GLES20.glGenTextures(1, textures, 0);
			mTextureID = textures[0];
			if (mTextureID <= 0)
			{
				GameActivity.Log.error("mTextureID <= 0");
				release();
				return;
			}

			mSurfaceTexture = new android.graphics.SurfaceTexture(mTextureID);
			mSurfaceTexture.setDefaultBufferSize(initialWidth, initialHeight);

			mSurfaceTexture.setOnFrameAvailableListener(this);
			mSurface = new android.view.Surface(mSurfaceTexture);

			int[] glInt = new int[1];

			GLES20.glGenFramebuffers(1,glInt,0);
			mFBO = glInt[0];
			if (mFBO <= 0)
			{
				GameActivity.Log.error("mFBO <= 0");
				release();
				return;
			}

			// Special shaders for blit of movie texture.
			mBlitVertexShaderID = createShader(GLES20.GL_VERTEX_SHADER, mBlitVextexShader);
			if (mBlitVertexShaderID == 0)
			{
				GameActivity.Log.error("mBlitVertexShaderID == 0");
				release();
				return;
			}
			int mBlitFragmentShaderID = createShader(GLES20.GL_FRAGMENT_SHADER,
				mSwizzlePixels ? mBlitFragmentShaderBGRA : mBlitFragmentShaderRGBA);
			if (mBlitFragmentShaderID == 0)
			{
				GameActivity.Log.error("mBlitFragmentShaderID == 0");
				release();
				return;
			}
			mProgram = GLES20.glCreateProgram();
			if (mProgram <= 0)
			{
				GameActivity.Log.error("mProgram <= 0");
				release();
				return;
			}
			GLES20.glAttachShader(mProgram, mBlitVertexShaderID);
			GLES20.glAttachShader(mProgram, mBlitFragmentShaderID);
			GLES20.glLinkProgram(mProgram);
			int[] linkStatus = new int[1];
			GLES20.glGetProgramiv(mProgram, GLES20.GL_LINK_STATUS, linkStatus, 0);
			if (linkStatus[0] != GLES20.GL_TRUE)
			{
				GameActivity.Log.error("Could not link program: ");
				GameActivity.Log.error(GLES20.glGetProgramInfoLog(mProgram));
				GLES20.glDeleteProgram(mProgram);
				mProgram = 0;
				release();
				return;
			}
			mPositionAttrib = GLES20.glGetAttribLocation(mProgram, "Position");
			mTexCoordsAttrib = GLES20.glGetAttribLocation(mProgram, "TexCoords");
			mTextureUniform = GLES20.glGetUniformLocation(mProgram, "VideoTexture");

			GLES20.glGenBuffers(1,glInt,0);
			mBlitBuffer = glInt[0];
			if (mBlitBuffer <= 0)
			{
				GameActivity.Log.error("mBlitBuffer <= 0");
				release();
				return;
			}

			// Create blit mesh.
			mTriangleVertices = java.nio.ByteBuffer.allocateDirect(
				mTriangleVerticesData.length * FLOAT_SIZE_BYTES)
					.order(java.nio.ByteOrder.nativeOrder()).asFloatBuffer();
			mTriangleVerticesDirty = true;
			// Set up GL state
			if (mUseOwnContext)
			{
				GLES20.glDisable(GLES20.GL_BLEND);
				GLES20.glDisable(GLES20.GL_CULL_FACE);
				GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
				GLES20.glDisable(GLES20.GL_STENCIL_TEST);
				GLES20.glDisable(GLES20.GL_DEPTH_TEST);
				GLES20.glDisable(GLES20.GL_DITHER);
				GLES20.glColorMask(true,true,true,true);
			}
		}
		
		private void UpdateVertexData()
		{
			if (!mTriangleVerticesDirty || mBlitBuffer <= 0)
			{
				return;
			}

			// fill it in
			mTriangleVertices.position(0);
			mTriangleVertices.put(mTriangleVerticesData).position(0);

			// save VBO state
			int[] glInt = new int[1];
			GLES20.glGetIntegerv(GLES20.GL_ARRAY_BUFFER_BINDING, glInt, 0);
			int previousVBO = glInt[0];
			
			GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, mBlitBuffer);
			GLES20.glBufferData(GLES20.GL_ARRAY_BUFFER,
				mTriangleVerticesData.length*FLOAT_SIZE_BYTES,
				mTriangleVertices, GLES20.GL_STATIC_DRAW);

			// restore VBO state
			GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, previousVBO);

			mTriangleVerticesDirty = false;
		}

		public boolean isValid()
		{
			return mSurfaceTexture != null;
		}

		private int createShader(int shaderType, String source)
		{
			int shader = GLES20.glCreateShader(shaderType);
			if (shader != 0)
			{
				GLES20.glShaderSource(shader, source);
				GLES20.glCompileShader(shader);
				int[] compiled = new int[1];
				GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0);
				if (compiled[0] == 0)
				{
					GameActivity.Log.error("Could not compile shader " + shaderType + ":");
					GameActivity.Log.error(GLES20.glGetShaderInfoLog(shader));
					GLES20.glDeleteShader(shader);
					shader = 0;
				}
			}
			return shader;
		}

		public void onFrameAvailable(android.graphics.SurfaceTexture st)
		{
			synchronized(this)
			{
				mFrameAvailable = true;
			}
		}

		public android.graphics.SurfaceTexture getSurfaceTexture()
		{
			return mSurfaceTexture;
		}

		public android.view.Surface getSurface()
		{
			return mSurface;
		}

		public int getExternalTextureId()
		{
			return mTextureID;
		}

		// NOTE: Synchronized with updateFrameData to prevent frame
		// updates while the surface may need to get reallocated.
		public void setSize(int width, int height)
		{
			synchronized(this)
			{
				if (width != mTextureWidth ||
					height != mTextureHeight)
				{
					mTextureWidth = width;
					mTextureHeight = height;
					mFrameData = null;
					mTextureSizeChanged = true;
				}
			}
		}

		public boolean resolutionChanged()
		{
			boolean changed;
			synchronized(this)
			{
				changed = mTextureSizeChanged;
				mTextureSizeChanged = false;
			}
			return changed;
		}

		private static final int FLOAT_SIZE_BYTES = 4;
		private static final int TRIANGLE_VERTICES_DATA_STRIDE_BYTES = 4 * FLOAT_SIZE_BYTES;
		private static final int TRIANGLE_VERTICES_DATA_POS_OFFSET = 0;
		private static final int TRIANGLE_VERTICES_DATA_UV_OFFSET = 2;
		private float[] mTriangleVerticesData = {
			// X, Y, U, V
			-1.0f, -1.0f, 0.f, 0.f,
			1.0f, -1.0f, 1.f, 0.f,
			-1.0f, 1.0f, 0.f, 1.f,
			1.0f, 1.0f, 1.f, 1.f,
			};

		private java.nio.FloatBuffer mTriangleVertices;

		private final String mBlitVextexShader =
			"attribute vec2 Position;\n" +
			"attribute vec2 TexCoords;\n" +
			"varying vec2 TexCoord;\n" +
			"void main() {\n" +
			"	TexCoord = TexCoords;\n" +
			"	gl_Position = vec4(Position, 0.0, 1.0);\n" +
			"}\n";

		// NOTE: We read the fragment as BGRA so that in the end, when
		// we glReadPixels out of the FBO, we get them in that order
		// and avoid having to swizzle the pixels in the CPU.
		private final String mBlitFragmentShaderBGRA =
			"#extension GL_OES_EGL_image_external : require\n" +
			"uniform samplerExternalOES VideoTexture;\n" +
			"varying highp vec2 TexCoord;\n" +
			"void main()\n" +
			"{\n" +
			"	gl_FragColor = texture2D(VideoTexture, TexCoord).bgra;\n" +
			"}\n";
		private final String mBlitFragmentShaderRGBA =
			"#extension GL_OES_EGL_image_external : require\n" +
			"uniform samplerExternalOES VideoTexture;\n" +
			"varying highp vec2 TexCoord;\n" +
			"void main()\n" +
			"{\n" +
			"	gl_FragColor = texture2D(VideoTexture, TexCoord).rgba;\n" +
			"}\n";

		private int mProgram;
		private int mPositionAttrib;
		private int mTexCoordsAttrib;
		private int mBlitBuffer;
		private int mTextureUniform;

		public FrameUpdateInfo updateFrameData()
		{
			synchronized(this)
			{
				if (null == mFrameData && mTextureWidth > 0 && mTextureHeight > 0)
				{
					mFrameData = java.nio.ByteBuffer.allocateDirect(mTextureWidth*mTextureHeight*4);
				}
				// Copy surface texture to frame data.
				if (!copyFrameTexture(0, mFrameData))
				{
					return null;
				}
			}

			FrameUpdateInfo frameUpdateInfo = new FrameUpdateInfo();

			frameUpdateInfo.Buffer = mFrameData;
			frameUpdateInfo.FrameReady = true;
			frameUpdateInfo.RegionChanged = false;
			return frameUpdateInfo;
		}

		public FrameUpdateInfo updateFrameData(int destTexture)
		{
			synchronized(this)
			{
				// Copy surface texture to destination texture.
				if (!copyFrameTexture(destTexture, null))
				{
					return null;
				}
			}

			FrameUpdateInfo frameUpdateInfo = new FrameUpdateInfo();

			frameUpdateInfo.Buffer = null;
			frameUpdateInfo.FrameReady = true;
			frameUpdateInfo.RegionChanged = false;
			return frameUpdateInfo;
		}

		// Copy the surface texture to another texture, or to raw data.
		// Note: copying to raw data creates a temporary FBO texture.
		private boolean copyFrameTexture(int destTexture, java.nio.Buffer destData)
		{
			if (!mFrameAvailable)
			{
				// We only return fresh data when we generate it. At other
				// time we return nothing to indicate that there was nothing
				// new to return. The media player deals with this by keeping
				// the last frame around and using that for rendering.
				return false;
			}
			mFrameAvailable = false;
			if (null == mSurfaceTexture)
			{
				// Can't update if there's no surface to update into.
				return false;
			}

			int[] glInt = new int[1];
			boolean[] glBool = new boolean[1];

			// Either use own context or save states
			boolean previousBlend=false, previousCullFace=false, previousScissorTest=false, previousStencilTest=false, previousDepthTest=false, previousDither=false;
			int previousFBO=0, previousVBO=0, previousMinFilter=0, previousMagFilter=0;
			int[] previousViewport = new int[4];
			if (mUseOwnContext)
			{
				// Received reports of these not being preserved when changing contexts
				GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
				GLES20.glGetTexParameteriv(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, glInt, 0);
				previousMinFilter = glInt[0];
				GLES20.glGetTexParameteriv(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, glInt, 0);
				previousMagFilter = glInt[0];

				saveContext();
				makeCurrent();
			}
			else
			{
				// Clear gl errors as they can creep in from the UE4 renderer.
				GLES20.glGetError();

				previousBlend = GLES20.glIsEnabled(GLES20.GL_BLEND);
				previousCullFace = GLES20.glIsEnabled(GLES20.GL_CULL_FACE);
				previousScissorTest = GLES20.glIsEnabled(GLES20.GL_SCISSOR_TEST);
				previousStencilTest = GLES20.glIsEnabled(GLES20.GL_STENCIL_TEST);
				previousDepthTest = GLES20.glIsEnabled(GLES20.GL_DEPTH_TEST);
				previousDither = GLES20.glIsEnabled(GLES20.GL_DITHER);
				GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, glInt, 0);
				previousFBO = glInt[0];
				GLES20.glGetIntegerv(GLES20.GL_ARRAY_BUFFER_BINDING, glInt, 0);
				previousVBO = glInt[0];
				GLES20.glGetIntegerv(GLES20.GL_VIEWPORT, previousViewport, 0);

				GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
				GLES20.glGetTexParameteriv(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, glInt, 0);
				previousMinFilter = glInt[0];
				GLES20.glGetTexParameteriv(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, glInt, 0);
				previousMagFilter = glInt[0];

				glVerify("save state");
			}

			// Get the latest video texture frame.
			mSurfaceTexture.updateTexImage();

			mSurfaceTexture.getTransformMatrix(mTransformMatrix);
				
			float UMin = mTransformMatrix[12];
			float UMax = UMin + mTransformMatrix[0];
			float VMin = mTransformMatrix[13];
			float VMax = VMin + mTransformMatrix[5];
				
			if (mTriangleVerticesData[2] != UMin ||
				mTriangleVerticesData[6] != UMax ||
				mTriangleVerticesData[11] != VMin ||
				mTriangleVerticesData[3] != VMax)
			{
				//GameActivity.Log.debug("Matrix:");
				//GameActivity.Log.debug(mTransformMatrix[0] + " " + mTransformMatrix[1] + " " + mTransformMatrix[2] + " " + mTransformMatrix[3]);
				//GameActivity.Log.debug(mTransformMatrix[4] + " " + mTransformMatrix[5] + " " + mTransformMatrix[6] + " " + mTransformMatrix[7]);
				//GameActivity.Log.debug(mTransformMatrix[8] + " " + mTransformMatrix[9] + " " + mTransformMatrix[10] + " " + mTransformMatrix[11]);
				//GameActivity.Log.debug(mTransformMatrix[12] + " " + mTransformMatrix[13] + " " + mTransformMatrix[14] + " " + mTransformMatrix[15]);
				mTriangleVerticesData[ 2] = mTriangleVerticesData[10] = UMin;
				mTriangleVerticesData[ 6] = mTriangleVerticesData[14] = UMax;
				mTriangleVerticesData[11] = mTriangleVerticesData[15] = VMin;
				mTriangleVerticesData[ 3] = mTriangleVerticesData[ 7] = VMax;
				mTriangleVerticesDirty = true;
				//GameActivity.Log.debug("U = " + mTriangleVerticesData[2] + ", " + mTriangleVerticesData[6]);
				//GameActivity.Log.debug("V = " + mTriangleVerticesData[11] + ", " + mTriangleVerticesData[3]);
			}

			if (null != destData)
			{
				// Rewind data so that we can write to it.
				destData.position(0);
			}

			if (!mUseOwnContext)
			{
				GLES20.glDisable(GLES20.GL_BLEND);
				GLES20.glDisable(GLES20.GL_CULL_FACE);
				GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
				GLES20.glDisable(GLES20.GL_STENCIL_TEST);
				GLES20.glDisable(GLES20.GL_DEPTH_TEST);
				GLES20.glDisable(GLES20.GL_DITHER);
				GLES20.glColorMask(true,true,true,true);

				glVerify("reset state");
			}

			GLES20.glViewport(0, 0, mTextureWidth, mTextureHeight);

			glVerify("set viewport");

			// Set-up FBO target texture..
			int FBOTextureID = 0;
			if (null != destData)
			{
				// Create temporary FBO for data copy.
				GLES20.glGenTextures(1,glInt,0);
				FBOTextureID = glInt[0];
			}
			else
			{
				// Use the given texture as the FBO.
				FBOTextureID = destTexture;
			}
			// Set the FBO to draw into the texture one-to-one.
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, FBOTextureID);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
			// Create the temp FBO data if needed.
			if (null != destData)
			{
				//int w = 1<<(32-Integer.numberOfLeadingZeros(mTextureWidth-1));
				//int h = 1<<(32-Integer.numberOfLeadingZeros(mTextureHeight-1));
				GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0,
					GLES20.GL_RGBA,
					mTextureWidth, mTextureHeight,
					0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
			}

			glVerify("set-up FBO texture");

			// Set to render to the FBO.
			GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, mFBO);

			GLES20.glFramebufferTexture2D(
				GLES20.GL_FRAMEBUFFER,
				GLES20.GL_COLOR_ATTACHMENT0,
				GLES20.GL_TEXTURE_2D, FBOTextureID, 0);

			// check status
			int status = GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER);
			if (status != GLES20.GL_FRAMEBUFFER_COMPLETE)
			{
				GameActivity.Log.warn("Failed to complete framebuffer attachment ("+status+")");
			}

			// The special shaders to render from the video texture.
			GLES20.glUseProgram(mProgram);

			// Set the mesh that renders the video texture.
			UpdateVertexData();
			GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, mBlitBuffer);
			GLES20.glEnableVertexAttribArray(mPositionAttrib);
			GLES20.glVertexAttribPointer(mPositionAttrib, 2, GLES20.GL_FLOAT, false,
				TRIANGLE_VERTICES_DATA_STRIDE_BYTES, 0);
			GLES20.glEnableVertexAttribArray(mTexCoordsAttrib);
			GLES20.glVertexAttribPointer(mTexCoordsAttrib, 2, GLES20.GL_FLOAT, false,
				TRIANGLE_VERTICES_DATA_STRIDE_BYTES,
				TRIANGLE_VERTICES_DATA_UV_OFFSET*FLOAT_SIZE_BYTES);

			glVerify("setup movie texture read");

			GLES20.glClear( GLES20.GL_COLOR_BUFFER_BIT);

			// connect 'VideoTexture' to video source texture (mTextureID).
			// mTextureID is bound to GL_TEXTURE_EXTERNAL_OES in updateTexImage
			GLES20.glUniform1i(mTextureUniform, 0);
			GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
			GLES20.glBindTexture(GL_TEXTURE_EXTERNAL_OES, mTextureID);

			// Draw the video texture mesh.
			GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

			GLES20.glFlush();

			// Read the FBO texture pixels into raw data.
			if (null != destData)
			{
				GLES20.glReadPixels(
					0, 0, mTextureWidth, mTextureHeight,
					GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE,
					destData);
			}

			glVerify("draw & read movie texture");

			// Restore state and cleanup.
			if (mUseOwnContext)
			{
				GLES20.glFramebufferTexture2D(
					GLES20.GL_FRAMEBUFFER,
					GLES20.GL_COLOR_ATTACHMENT0,
					GLES20.GL_TEXTURE_2D, 0, 0);

				if (null != destData && FBOTextureID > 0)
				{
					glInt[0] = FBOTextureID;
					GLES20.glDeleteTextures(1, glInt, 0);
				}

				restoreContext();

				// Restore previous texture filtering
				GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, previousMinFilter);
				GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, previousMagFilter);
			}
			else
			{
				GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, previousFBO);
				if (null != destData && FBOTextureID > 0)
				{
					glInt[0] = FBOTextureID;
					GLES20.glDeleteTextures(1, glInt, 0);
				}
				GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, previousVBO);

				GLES20.glViewport(previousViewport[0], previousViewport[1],	previousViewport[2], previousViewport[3]);
				if (previousBlend) GLES20.glEnable(GLES20.GL_BLEND);
				if (previousCullFace) GLES20.glEnable(GLES20.GL_CULL_FACE);
				if (previousScissorTest) GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
				if (previousStencilTest) GLES20.glEnable(GLES20.GL_STENCIL_TEST);
				if (previousDepthTest) GLES20.glEnable(GLES20.GL_DEPTH_TEST);
				if (previousDither) GLES20.glEnable(GLES20.GL_DITHER);

				GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, previousMinFilter);
				GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, previousMagFilter);

				// invalidate cached state in RHI
				GLES20.glDisableVertexAttribArray(mPositionAttrib);
				GLES20.glDisableVertexAttribArray(mTexCoordsAttrib);
				nativeClearCachedAttributeState(mPositionAttrib, mTexCoordsAttrib);
			}

			return true;
		}

		private void showGlError(String op, int error)
		{
			switch (error)
			{
				case GLES20.GL_INVALID_ENUM:						GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_INVALID_ENUM");  break;
				case GLES20.GL_INVALID_OPERATION:					GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_INVALID_OPERATION");  break;
				case GLES20.GL_INVALID_FRAMEBUFFER_OPERATION:		GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_INVALID_FRAMEBUFFER_OPERATION");  break;
				case GLES20.GL_INVALID_VALUE:						GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_INVALID_VALUE");  break;
				case GLES20.GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:	GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");  break;
				case GLES20.GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:	GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS");  break;
				case GLES20.GL_FRAMEBUFFER_UNSUPPORTED:				GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_FRAMEBUFFER_UNSUPPORTED");  break;
				case GLES20.GL_OUT_OF_MEMORY:						GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError GL_OUT_OF_MEMORY");  break;
				default:											GameActivity.Log.error("WebViewControl$BitmapRenderer: " + op + ": glGetError " + error);
			}
		}

		private void glVerify(String op)
		{
			int error;
			while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR)
			{
				showGlError(op, error);
				throw new RuntimeException(op + ": glGetError " + error);
			}
		}

		private void glWarn(String op)
		{
			int error;
			while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR)
			{
				showGlError(op, error);
			}
		}

		public void release()
		{
			if (null != mSurface)
			{
				mSurface.release();
				mSurface = null;
			}
			if (null != mSurfaceTexture)
			{
				mSurfaceTexture.release();
				mSurfaceTexture = null;
			}
			int[] glInt = new int[1];
			if (mBlitBuffer > 0)
			{
				glInt[0] = mBlitBuffer;
				GLES20.glDeleteBuffers(1,glInt,0);
				mBlitBuffer = -1;
			}
			if (mProgram > 0)
			{
				GLES20.glDeleteProgram(mProgram);
				mProgram = -1;
			}
			if (mBlitVertexShaderID > 0)
			{
				GLES20.glDeleteShader(mBlitVertexShaderID);
				mBlitVertexShaderID = -1;
			}
			if (mBlitFragmentShaderID > 0)
			{
				GLES20.glDeleteShader(mBlitFragmentShaderID);
				mBlitFragmentShaderID = -1;
			}
			if (mFBO > 0)
			{
				glInt[0] = mFBO;
				GLES20.glDeleteFramebuffers(1,glInt,0);
				mFBO = -1;
			}
			if (mTextureID > 0)
			{
				glInt[0] = mTextureID;
				GLES20.glDeleteTextures(1,glInt,0);
				mTextureID = -1;
			}
			if (mEglSurface != EGL14.EGL_NO_SURFACE)
			{
				EGL14.eglDestroySurface(mEglDisplay, mEglSurface);
				mEglSurface = EGL14.EGL_NO_SURFACE;
			}
			if (mEglContext != EGL14.EGL_NO_CONTEXT)
			{
				EGL14.eglDestroyContext(mEglDisplay, mEglContext);
				mEglContext = EGL14.EGL_NO_CONTEXT;
			}
			if (mCreatedEGLDisplay)
			{
				EGL14.eglTerminate(mEglDisplay);
				mEglDisplay = EGL14.EGL_NO_DISPLAY;
				mCreatedEGLDisplay = false;	
			}
		}
	};
		
	public native void nativeClearCachedAttributeState(int PositionAttrib, int TexCoordsAttrib);

	private boolean CreateOESTextureRenderer(int OESTextureId)
	{
		releaseOESTextureRenderer();

		mOESTextureRenderer = new OESTextureRenderer(OESTextureId);
		if (!mOESTextureRenderer.isValid())
		{
			mOESTextureRenderer = null;
			return false;
		}
		
		// set this here as the size may have been set before the GL resources were created.
		mOESTextureRenderer.setSize(initialWidth, initialHeight);
		GameActivity.Get().runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.setSurface(mOESTextureRenderer.getSurface());
			}
		});
		return true;
	}

	void releaseOESTextureRenderer()
	{
		if (null != mOESTextureRenderer)
		{
			mOESTextureRenderer.release();
			mOESTextureRenderer = null;
		}
	}
	
	public FrameUpdateInfo updateVideoFrame(int externalTextureId)
	{
		if (null == mOESTextureRenderer)
		{
			if (!CreateOESTextureRenderer(externalTextureId))
			{
				GameActivity.Log.warn("updateVideoFrame failed to alloc mOESTextureRenderer ");
				release();
				return null;
			}
		}

		WaitOnBitmapRender = true;
		FrameUpdateInfo result = mOESTextureRenderer.updateVideoFrame();
		WaitOnBitmapRender = false;
		return result;
	}
	
	/*
		This handles events for our OES texture
	*/
	class OESTextureRenderer
		implements android.graphics.SurfaceTexture.OnFrameAvailableListener
	{
		private android.graphics.SurfaceTexture mSurfaceTexture = null;
		private int mTextureWidth = -1;
		private int mTextureHeight = -1;
		private android.view.Surface mSurface = null;
		private boolean mFrameAvailable = false;
		private int mTextureID = -1;
		private float[] mTransformMatrix = new float[16];
		private boolean mTextureSizeChanged = true;
		private int GL_TEXTURE_EXTERNAL_OES = 0x8D65;

		private float mUScale = 1.0f;
		private float mVScale = -1.0f;
		private float mUOffset = 0.0f;
		private float mVOffset = 1.0f;

		public OESTextureRenderer(int OESTextureId)
		{
			mTextureID = OESTextureId;

			mSurfaceTexture = new android.graphics.SurfaceTexture(mTextureID);
			mSurfaceTexture.setDefaultBufferSize(initialWidth, initialHeight);

			mSurfaceTexture.setOnFrameAvailableListener(this);
			mSurface = new android.view.Surface(mSurfaceTexture);
		}

		public void release()
		{
			if (null != mSurface)
			{
				mSurface.release();
				mSurface = null;
			}
			if (null != mSurfaceTexture)
			{
				mSurfaceTexture.release();
				mSurfaceTexture = null;
			}
		}

		public boolean isValid()
		{
			return mSurfaceTexture != null;
		}

		public void onFrameAvailable(android.graphics.SurfaceTexture st)
		{
			synchronized(this)
			{
				mFrameAvailable = true;
			}
		}

		public android.graphics.SurfaceTexture getSurfaceTexture()
		{
			return mSurfaceTexture;
		}

		public android.view.Surface getSurface()
		{
			return mSurface;
		}

		public int getExternalTextureId()
		{
			return mTextureID;
		}

		// NOTE: Synchronized with updateFrameData to prevent frame
		// updates while the surface may need to get reallocated.
		public void setSize(int width, int height)
		{
			synchronized(this)
			{
				if (width != mTextureWidth ||
					height != mTextureHeight)
				{
					mTextureWidth = width;
					mTextureHeight = height;
					mTextureSizeChanged = true;
				}
			}
		}

		public boolean resolutionChanged()
		{
			boolean changed;
			synchronized(this)
			{
				changed = mTextureSizeChanged;
				mTextureSizeChanged = false;
			}
			return changed;
		}

		public FrameUpdateInfo updateVideoFrame()
		{
			synchronized(this)
			{
				return getFrameUpdateInfo();
			}
		}

		private FrameUpdateInfo getFrameUpdateInfo()
		{
			FrameUpdateInfo frameUpdateInfo = new FrameUpdateInfo();

			frameUpdateInfo.Buffer = null;
			frameUpdateInfo.FrameReady = false;
			frameUpdateInfo.RegionChanged = false;
			frameUpdateInfo.UScale = mUScale;
			frameUpdateInfo.UOffset = mUOffset;

			// note: the matrix has V flipped
			frameUpdateInfo.VScale = -mVScale;
			frameUpdateInfo.VOffset = 1.0f - mVOffset;

			if (!mFrameAvailable)
			{
				// We only return fresh data when we generate it. At other
				// time we return nothing to indicate that there was nothing
				// new to return. The media player deals with this by keeping
				// the last frame around and using that for rendering.
				return frameUpdateInfo;
			}

			mFrameAvailable = false;
			if (null == mSurfaceTexture)
			{
				// Can't update if there's no surface to update into.
				return frameUpdateInfo;
			}

			frameUpdateInfo.FrameReady = true;

			// Get the latest video texture frame.
			
			mSurfaceTexture.updateTexImage();

			// updateTexImage binds an external texture to active texture unit
			// make sure to unbind it to prevent state leak
			GLES20.glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
						
			return frameUpdateInfo;
		}
	};

	// ======================================================================================


	class GLWebView extends WebView 
	{
		private android.view.Surface mSurface = null;
		public boolean IsAndroid3DBrowser = false;
		public boolean bBitmapDraw = false;
		Canvas TargetCanvas = null;

		// default constructors
		public GLWebView(Context context) {
			super(context);
			init();
		}

		public GLWebView(Context context, AttributeSet attrs) {
			super(context, attrs);
			init();
		}

		public GLWebView(Context context, AttributeSet attrs, int defStyle) {
			super(context, attrs, defStyle);
			init();
		}

		public void init()
		{
		
			setOnTouchListener(new View.OnTouchListener() {
				@Override
				public boolean onTouch(View v, MotionEvent event) {
					return IsAndroid3DBrowser || bBitmapDraw;
				}
			});
		
		}

		public void SetBitmapDraw(Canvas InCanvas)
		{
			boolean InBitmapDraw = InCanvas != null;
			synchronized(this)
			{
				if(bBitmapDraw != InBitmapDraw)
				{
					TargetCanvas = InCanvas;
					bBitmapDraw = InBitmapDraw;
					webView.setFocusableInTouchMode(!(IsAndroid3DBrowser || bBitmapDraw));
				}
			}
		}

		public void SetAndroid3DBrowser(boolean InIsAndroid3DBrowser)
		{
			synchronized(this)
			{
				if(IsAndroid3DBrowser != InIsAndroid3DBrowser)
				{
					IsAndroid3DBrowser = InIsAndroid3DBrowser;
					webView.setFocusableInTouchMode(!(IsAndroid3DBrowser || bBitmapDraw));
				}
			}
		}

		@Override
		protected void onLayout(boolean changed, int left, int top, int right, int bottom)
		{
			super.onLayout(changed, left, top, right, bottom);
		}

		// draw magic
		@Override
		public void onDraw( Canvas canvas ) 
		{
			if (bBitmapDraw)
			{
				TargetCanvas.save();
				float xScale = TargetCanvas.getWidth() / (float) canvas.getWidth();
				TargetCanvas.translate(-getScrollX(), -getScrollY());
				super.onDraw(TargetCanvas);
				TargetCanvas.restore();
				//GameActivity.Log.debug("onDraw: res: " + TargetCanvas.getWidth() + " x " + TargetCanvas.getHeight() + ", scroll:" + getScrollX() + " x " + getScrollY());
				return;
			}

			//returns canvas attached to gl texture to draw on
			Canvas glAttachedCanvas = null;
			if(!IsAndroid3DBrowser)
			{
				super.onDraw(canvas);
			}
			else if (mSurface != null) 
			{
				try 
				{
					glAttachedCanvas = mSurface.lockHardwareCanvas();
					//draw the view to provided canvas

					//translate canvas to reflect view scrolling
					float xScale = glAttachedCanvas.getWidth() / (float) canvas.getWidth();
					glAttachedCanvas.scale(xScale, xScale);
					glAttachedCanvas.translate(-getScrollX(), -getScrollY());
					super.onDraw(glAttachedCanvas);
					mSurface.unlockCanvasAndPost(glAttachedCanvas);
				}
				catch (Exception e)
				{
					Log.e(TAG, "error while rendering view to gl: " + e);
				}
			}
		}

		public void setSurface(android.view.Surface surface)
		{
			mSurface = surface;
		}
	}

	private class ViewClient
		extends WebViewClient
	{
		@Override
		public WebResourceResponse shouldInterceptRequest(WebView View, String Url)
		{
			byte[] Result = shouldInterceptRequestImpl(Url);
			if (Result != null)
			{
				return new WebResourceResponse("text/html", "utf8", new ByteArrayInputStream(Result));
			}
			else
			{
				return null;
			}
		}

		@Override
		public native boolean shouldOverrideUrlLoading(WebView View, String Url);

		@Override
		public void onPageStarted(WebView View, String Url, Bitmap Favicon)
		{
			WebBackForwardList History = View.copyBackForwardList();
			onPageLoad(Url, true, History.getSize(), History.getCurrentIndex());
		}

		@Override
		public void onPageFinished(WebView View, String Url)
		{
			WebBackForwardList History = View.copyBackForwardList();
			onPageLoad(Url, false, History.getSize(), History.getCurrentIndex());
		}

		@Override
		public native void onReceivedError(WebView View, int ErrorCode, String Description, String Url);

		public native void onPageLoad(String Url, boolean bIsLoading, int HistorySize, int HistoryPosition);
		private native byte[] shouldInterceptRequestImpl(String Url);

		public long GetNativePtr()
		{
			return WebViewControl.this.nativePtr;
		}
	}

	private class ChromeClient
		extends WebChromeClient
	{
		public native boolean onJsAlert(WebView View, String Url, String Message, JsResult Result);
		public native boolean onJsBeforeUnload(WebView View, String Url, String Message, JsResult Result);
		public native boolean onJsConfirm(WebView View, String Url, String Message, JsResult Result);
		public native boolean onJsPrompt(WebView View, String Url, String Message, String DefaultValue, JsPromptResult Result);
		public native void onReceivedTitle(WebView View, String Title);

		public long GetNativePtr()
		{
			return WebViewControl.this.nativePtr;
		}

		@Override
		public boolean onCreateWindow(WebView View, boolean isDialog, boolean isUserGesture, Message resultMsg)
		{
			WebView newView = new WebView(GameActivity.Get());
			View.addView(newView);
            WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
            transport.setWebView(newView);
            resultMsg.sendToTarget();
            return true;
		}

		@Override
		public boolean onConsoleMessage(ConsoleMessage cm) {
			GameActivity.Log.warn(cm.message() + " -- From line "
				+ cm.lineNumber() + " of "
				+ cm.sourceId() );
			return true;
		}
	}
	public long GetNativePtr()
	{
		return nativePtr;
	}
		
	public GLWebView webView;
	private WebViewPositionLayout positionLayout;
	public int curX, curY, curW, curH;
	private boolean bShown;
	private boolean bClosed;
	private String NextURL;
	private String NextContent;
	
	// Address of the native SAndroidWebBrowserWidget object
	private long nativePtr;
}
