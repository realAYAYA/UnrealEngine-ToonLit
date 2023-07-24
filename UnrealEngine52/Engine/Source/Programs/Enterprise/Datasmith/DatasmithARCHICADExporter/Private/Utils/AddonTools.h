// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "APIEnvir.h"

#if PLATFORM_WINDOWS
	#pragma warning(push)
	#pragma warning(disable : 4800)
	#pragma warning(disable : 4505)
#endif

#include <string>
#include <cstdarg>

#if PLATFORM_WINDOWS
	#pragma warning(pop)
#endif

#include "LocalizeTools.h"
#include "DebugTools.h"

BEGIN_NAMESPACE_UE_AC

// Print in a string using the format and arguments list
utf8_string VStringFormat(const utf8_t* InFmt, std::va_list InArgumentsList) __printflike(1, 0);

// Print in a string using the format and arguments
utf8_string Utf8StringFormat(const utf8_t* InFmt, ...) __printflike(1, 2);

// Zero the content of structure *x
#define Zap(x) BNZeroMemory(x, sizeof(*x));

// Short way to get Utf8 char pointer of a GS::UniString
#define ToUtf8() ToCStr(0, GS::MaxUSize, CC_UTF8).Get()

// Short way to get TCHAR pointer of a GS::UniString
#define GSStringToUE(InGSString) UTF16_TO_TCHAR(InGSString.ToUStr().Get())

// Convert a UE String to GS::UniString
inline GS::UniString UEToGSString(const TCHAR* InUEString)
{
	return GS::UniString(reinterpret_cast< const GS::UniChar::Layout* >(InUEString));
}

// Convert an Archicad fingerprint to an API_Guid
inline const API_Guid& Fingerprint2API_Guid(const MD5::FingerPrint& inFP)
{
	return *(const API_Guid*)&inFP;
}

// Compute Guid of the string
API_Guid String2API_Guid(const GS::UniString& inString);

// Compute Guid from MD5 of the value
template < class T > inline API_Guid GuidFromMD5(const T& inV)
{
	MD5::Generator MD5Generator;
	MD5Generator.Update(&inV, sizeof(T));
	MD5::FingerPrint FingerPrint;
	MD5Generator.Finish(FingerPrint);
	return Fingerprint2API_Guid(FingerPrint);
}

// Combine 2 guid in one
API_Guid CombineGuid(const API_Guid& InGuid1, const API_Guid& InGuid2);

// Convert StandardRGB component to LinearRGB component
inline float StandardRGBToLinear(double InRgbComponent)
{
	return float(InRgbComponent > 0.04045 ? pow(InRgbComponent * (1.0 / 1.055) + 0.0521327, 2.4)
										  : InRgbComponent * (1.0 / 12.92));
}

// Convert Archicad RGB color to UE Linear color
inline FLinearColor ACRGBColorToUELinearColor(const ModelerAPI::Color& InColor)
{
	return FLinearColor(StandardRGBToLinear(InColor.red), StandardRGBToLinear(InColor.green),
						StandardRGBToLinear(InColor.blue));
}

// Stack class to auto dispose memo handles
class FAutoMemo
{
  public:
	FAutoMemo(const API_Guid& Guid, UInt64 Mask = APIMemoMask_All)
	{
		Zap(&Memo);
		GSErr = ACAPI_Element_GetMemo(Guid, &Memo, Mask);
	}

	~FAutoMemo()
	{
		if (GSErr != NoError)
		{
			ACAPI_DisposeElemMemoHdls(&Memo);
		}
	}

	API_ElementMemo Memo;
	GS::GSErrCode	GSErr = NoError;
};

// Stack class to auto dispose handles
class FAutoHandle
{
	GSHandle Handle;

  public:
	FAutoHandle(GSHandle InHandleToDispose)
		: Handle(InHandleToDispose)
	{
	}

	~FAutoHandle()
	{
		if (Handle)
		{
			BMKillHandle(&Handle);
		}
	}

	// Take ownership of this handle
	GSHandle Take()
	{
		GSHandle TmpHandle = Handle;
		Handle = nullptr;
		return TmpHandle;
	}
};

// kStrListENames multi strings
enum ENames
{
	kName_Invalid,
	kName_TextureExtension,
	kName_TextureMime,
	kName_ShowPalette,
	kName_HidePalette,
	kName_DatasmithFileTypeName,
	kName_ExportToDatasmithFile,
	kName_StartAutoSync,
	kName_PauseAutoSync,
	kName_Undefined,
	kName_CacheDirectory,
	kName_Source,
	kName_Destination,
	kName_FmtTooltip,
	kName_NBNames
};
const utf8_t*		 GetStdName(ENames InStrIndex);
const GS::UniString& GetGSName(ENames InStrIndex);

// Return the company directory
IO::Location GetCompanyDataDirectory();

// Return the data directory of the addon
const GS::UniString& GetAddonDataDirectory();

// Return the addon version string
GS::UniString GetAddonVersionsStr();

// Tool to get the name of layer
GS::UniString GetLayerName(API_AttributeIndex InLayer);

// Return current display unit name
const utf8_t* GetCurrentUnitDisplayName();

// Return current time as string
utf8_string GetCurrentLocalDateTime();

// Return true if 3d window is the current
bool Is3DCurrenWindow();

END_NAMESPACE_UE_AC
