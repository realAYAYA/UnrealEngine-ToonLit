// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSCookieManager.h"

#if PLATFORM_IOS
#import <Foundation/Foundation.h>
#include "IOS/IOSAsyncTask.h"

FIOSCookieManager::FIOSCookieManager()
{
}

FIOSCookieManager::~FIOSCookieManager()
{
}

void FIOSCookieManager::SetCookie(const FString& URL, const FCookie& Cookie, TFunction<void(bool)> Completed)
{
	// not implemented
	if (Completed)
	{
		Completed(false);
	}
}

void FIOSCookieManager::DeleteCookies(const FString& URL, const FString& CookieName, TFunction<void(int)> Completed)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		// Delete matching cookies
		NSHTTPCookie* Cookie;
		NSHTTPCookieStorage* Storage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
		for (Cookie in[Storage cookies])
		{
			FString Domain([Cookie domain]);
			FString Path([Cookie path]);
			FString CookieUrl = Domain + Path;
			
			if (CookieUrl.Contains(URL) || URL.IsEmpty())
			{
				[Storage deleteCookie:Cookie];
			}
		}
		
		// Notify on the game thread
		[FIOSAsyncTask CreateTaskWithBlock:^bool(void)
		 {
			 if (Completed)
			 {
				 Completed(true);
			 }
			 return true;
		 }];
	});
}

#endif
