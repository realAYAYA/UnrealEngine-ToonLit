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

// Set the string from the pasteboard contents
utf8_string GetStringFromPasteboard()
{
	utf8_string TheString;

#ifdef __GNUC__
	PasteboardRef Pasteboard;
	OSStatus	  OSErr = ::PasteboardCreate(kPasteboardClipboard, &Pasteboard);
	if (OSErr != noErr)
	{
		fprintf(stderr, "PasteboardCreate error=%d\n", int(OSErr));
	}
	else
	{
		::PasteboardSyncFlags SyncFlags = ::PasteboardSynchronize(Pasteboard);
		(void)SyncFlags;

		ItemCount ItemCount;
		OSErr = ::PasteboardGetItemCount(Pasteboard, &ItemCount);
		if (OSErr != noErr)
		{
			fprintf(stderr, "PasteboardGetItemCount error=%d\n", int(OSErr));
		}

		for (CFIndex ItemIndex = 1; ItemIndex <= ItemCount && TheString.size() == 0; ItemIndex++)
		{
			PasteboardItemID ItemID;
			OSErr = ::PasteboardGetItemIdentifier(Pasteboard, ItemIndex, &ItemID);
			if (OSErr != noErr)
			{
				fprintf(stderr, "PasteboardGetItemIdentifier error=%d\n", int(OSErr));
			}
			else
			{
				CFArrayRef FlavorTypeArray;
				OSErr = ::PasteboardCopyItemFlavors(Pasteboard, ItemID, &FlavorTypeArray);
				if (OSErr != noErr)
				{
					fprintf(stderr, "PasteboardCopyItemFlavors error=%d\n", int(OSErr));
				}
				else
				{
					CFIndex FlavorCount = ::CFArrayGetCount(FlavorTypeArray);
					for (CFIndex FlavorIndex = 0; FlavorIndex < FlavorCount && TheString.size() == 0; FlavorIndex++)
					{
						CFStringRef FlavorType = (CFStringRef)::CFArrayGetValueAtIndex(FlavorTypeArray, FlavorIndex);
						if (::UTTypeConformsTo(FlavorType, CFSTR("public.utf8-plain-text")))
						{
							CFDataRef FlavorData;

							OSErr = ::PasteboardCopyItemFlavorData(Pasteboard, ItemID, FlavorType, &FlavorData);
							if (OSErr != noErr)
							{
								fprintf(stderr, "PasteboardCopyItemFlavorData error=%d\n", int(OSErr));
							}
							else
							{
								CFIndex FlavorDataSize = ::CFDataGetLength(FlavorData);
								if (FlavorDataSize > 0)
								{
									TheString.assign(reinterpret_cast< const char* >(::CFDataGetBytePtr(FlavorData)),
													 FlavorDataSize);
								}

								CFRelease(FlavorData);
							}
						}
					}
				}
				CFRelease(FlavorTypeArray);
			}
		}

		CFRelease(Pasteboard);
	}
#else
	if (OpenClipboard(NULL))
	{
		HANDLE HData = GetClipboardData(CF_TEXT);
		TheString = reinterpret_cast< char* >(GlobalLock(HData));
		GlobalUnlock(HData);
		CloseClipboard();
	}
#endif

	return TheString;
}

END_NAMESPACE_UE_AC
