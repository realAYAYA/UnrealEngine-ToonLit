// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pasteboard.h"

BEGIN_NAMESPACE_UE_AC

// Copy the string to the pasteboard
void SetPasteboardWithString(const utf8_t* InUtf8String)
{
#ifdef __GNUC__
	CFDataRef Data =
		CFDataCreate(kCFAllocatorDefault, reinterpret_cast< const UInt8* >(InUtf8String), strlen(InUtf8String));
	if (Data == NULL)
	{
		return (void)fprintf(stderr, "CFDataCreate returned NULL\n");
	}

	PasteboardRef Pasteboard;
	OSStatus	  OSErr = ::PasteboardCreate(kPasteboardClipboard, &Pasteboard);
	if (OSErr != noErr)
	{
		fprintf(stderr, "PasteboardCreate error=%d\n", int(OSErr));
	}
	else
	{
		OSErr = ::PasteboardClear(Pasteboard);
		if (OSErr != noErr)
		{
			fprintf(stderr, "PasteboardClear error=%d\n", int(OSErr));
		}
		else
		{
			PasteboardSyncFlags SyncFlags = ::PasteboardSynchronize(Pasteboard);
			(void)SyncFlags;
			OSErr =
				::PasteboardPutItemFlavor(Pasteboard, (PasteboardItemID)1, CFSTR("public.utf8-plain-text"), Data, 0);
			if (OSErr != noErr)
			{
				fprintf(stderr, "PasteboardPutItemFlavor error=%d\n", int(OSErr));
			}
		}

		CFRelease(Pasteboard);
	}

	CFRelease(Data);
#else
	if (OpenClipboard(NULL))
	{
		EmptyClipboard();
		int LUtf16 = MultiByteToWideChar(CP_UTF8, 0, InUtf8String, -1, nullptr, 0);
		if (LUtf16 > 0)
		{
			HGLOBAL	 ClipBuffer = GlobalAlloc(GMEM_DDESHARE, LUtf16 * sizeof(wchar_t));
			wchar_t* Buffer = reinterpret_cast< wchar_t* >(GlobalLock(ClipBuffer));
			MultiByteToWideChar(CP_UTF8, 0, InUtf8String, -1, Buffer, LUtf16);
			GlobalUnlock(ClipBuffer);
			SetClipboardData(CF_UNICODETEXT, ClipBuffer);
		}
		CloseClipboard();
	}
#endif
}

END_NAMESPACE_UE_AC
