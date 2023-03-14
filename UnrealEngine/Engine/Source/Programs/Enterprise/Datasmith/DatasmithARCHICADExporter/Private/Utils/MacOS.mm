// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurrentOS.h"

//#include "XGetHTTPSource.h"
#import <Cocoa/Cocoa.h>
#include <pwd.h>
#include <codecvt>
#include <string>

BEGIN_NAMESPACE_UE_AC

static utf8_string NS2stdString(NSString* InString)
{
	const utf8_t* s = [InString UTF8String];
	if (s == nullptr)
	{
		s = "UTF8String returned NULL";
		UE_AC_DebugF("UE_AC::%s\n", s);
	}
	return s;
}

VecStrings GetPrefLanguages()
{
	VecStrings			  languages;
	NSArray< NSString* >* userPrefLanguages = [NSLocale preferredLanguages];
	NSUInteger			  NbLanguages = [userPrefLanguages count];
	for (NSUInteger i = 0; i < NbLanguages; i++)
	{
		NSString* language = [userPrefLanguages objectAtIndex:i];
		languages.push_back(NS2stdString(language));
	}
	return languages;
}

// Return the user app support directory
GS::UniString GetApplicationSupportDirectory()
{
	NSError* error = nil;
	NSURL*	 AppSupportDir = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
																	inDomain:NSUserDomainMask
														   appropriateForURL:nil
																	  create:YES
																	   error:&error];
	if (error)
	{
		UE_AC_DebugF("GetApplicationSupportDirectory - error \"%s\"\n", [[error description] UTF8String]);
		return GetHomeDirectory();
	}

	return GS::UniString([[AppSupportDir path] UTF8String], CC_UTF8);
}

// Return the user home directory
GS::UniString GetHomeDirectory()
{
	struct passwd* pw = getpwuid(getuid());
	return GS::UniString(pw->pw_dir, CC_UTF8);
}

END_NAMESPACE_UE_AC
