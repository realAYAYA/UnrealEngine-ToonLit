// Copyright Epic Games, Inc. All Rights Reserved.

#include "AddonTools.h"
#include "ResourcesUtils.h"
#include "CurrentOS.h"

#include "Folder.hpp"

#include <stdexcept>
#include <cstdarg>
#include <time.h>

#if AC_VERSION > 26
class API_PenType;
GSErrCode ACAPI_ProjectOperation_Quit() { return ACAPI_ProjectOperation_Quit(0); }
GSErrCode ACAPI_GraphicalOverride_GetOverrideRule (API_OverrideRule& rule) { return GS::NoError; }
GSErrCode ACAPI_GraphicalOverride_CreateOverrideRule (API_OverrideRule& rule) { const API_Guid _Guid{};  return ACAPI_GraphicalOverride_CreateOverrideRule(rule, _Guid); }
GSErrCode ACAPI_Preferences_SetOldVersion (Int32 version, GSSize nByte, const void* data, unsigned short platformSign, API_FTypeID oldPlanFileID) { return GS::NoError; }

// Definition of most of the methods from previous versions of the ArchiCAD SDK
// Include once to avoid linkage errors for multiple implementations.
#include "ACAPI_MigrationHeader.hpp"
#endif

#if PLATFORM_WINDOWS && (__cplusplus < 201103L)
	#define va_copy(destination, source) ((destination) = (source))
#endif

BEGIN_NAMESPACE_UE_AC

// Print in a string using the format and arguments list
utf8_string VStringFormat(const utf8_t* InFmt, va_list InArgumentsList)
{
	// We need to copy in case of formatted string is bigger than default buffer
	va_list ArgumentsListCopy;
	va_copy(ArgumentsListCopy, InArgumentsList);
	// Try to print in a buffer
	utf8_t Buffer[10241];
	int	   FormattedStringSize = vsnprintf(Buffer, sizeof(Buffer), InFmt, InArgumentsList);
	if (FormattedStringSize < 0)
	{
		va_end(ArgumentsListCopy);
		throw std::runtime_error("vStringFormat - vsnprintf return an error");
	}

	// Is buffer was big enough ?
	if ((size_t)FormattedStringSize < sizeof(Buffer))
	{
		va_end(ArgumentsListCopy);
		return Buffer;
	}
	else
	{
		// Buffer is too small for the resulting string, so we use the size needed to create a right sized string
		utf8_string FormattedString;
		FormattedString.resize(FormattedStringSize);
		FormattedStringSize =
			vsnprintf((utf8_t*)FormattedString.c_str(), FormattedString.size() + 1, InFmt, ArgumentsListCopy);
		va_end(ArgumentsListCopy);
		if (FormattedStringSize < 0)
		{
			throw std::runtime_error("vStringFormat - vsnprintf return an error");
		}
		if ((size_t)FormattedStringSize != FormattedString.size())
		{
			throw std::runtime_error("vStringFormat - vsnprintf return an inconsistant size");
		}

		return FormattedString;
	}
}

// Print in a string using the format and arguments
utf8_string Utf8StringFormat(const utf8_t* InFmt, ...)
{
	utf8_string FormattedString;

	// Try to print in a buffer
	va_list ArgumentsList;
	va_start(ArgumentsList, InFmt);
	try
	{
		FormattedString = VStringFormat(InFmt, ArgumentsList);
	}
	catch (...)
	{
		// Clean up
		va_end(ArgumentsList);
		throw;
	}

	va_end(ArgumentsList);

	return FormattedString;
}

// Compute Guid of the string
API_Guid String2API_Guid(const GS::UniString& inString)
{
	utf8_string	   Utf8String(inString.ToUtf8());
	MD5::Generator g;
	g.Update(Utf8String.c_str(), (unsigned int)Utf8String.size());
	MD5::FingerPrint fp;
	g.Finish(fp);
	return Fingerprint2API_Guid(fp);
}

// Combine 2 guid in one
API_Guid CombineGuid(const API_Guid& InGuid1, const API_Guid& InGuid2)
{
	API_Guid Guid;
	((size_t*)&Guid)[0] = ((const size_t*)&InGuid1)[0] + ((const size_t*)&InGuid2)[0];
	((size_t*)&Guid)[1] = ((const size_t*)&InGuid1)[1] + ((const size_t*)&InGuid2)[1];
	return Guid;
}

static FMultiString Names(kStrListENames);

const utf8_t* GetStdName(ENames InStrIndex)
{
	return Names.GetStdString(InStrIndex);
}

const GS::UniString& GetGSName(ENames InStrIndex)
{
	return Names.GetGSString(InStrIndex);
}

// Return the company directory
IO::Location GetCompanyDataDirectory()
{
	// Get user application support directory
	IO::Location location(GetApplicationSupportDirectory());
	// Get or create company directory
	location.AppendToLocal(IO::Name("Epic"));
	IO::Folder CompanyFolder(location, IO::Folder::Create);
	GSErrCode  GSErr = CompanyFolder.GetStatus();
	if (GSErr != NoError)
	{
		UE_AC_ErrorMsgF("GetAddonDataDirectory - Accessing company folder error(%s)\n", GSErr);
		UE_AC_TestGSError(GSErr);
	}

	return location;
}

// Return the data directory of the addon
const GS::UniString& GetAddonDataDirectory()
{
	static GS::UniString AddonDataDirectoryString;
	if (AddonDataDirectoryString.GetLength() == 0)
	{
		// Get or create addon directory
#if !defined(EXPORT_ONLY)
		GSResID IdDescription = kStrListSyncPlugInDescription;
#else
		GSResID IdDescription = kStrListExportPlugInDescription;
#endif
		IO::Location location(GetCompanyDataDirectory());
		location.AppendToLocal(IO::Name(GetUniString(IdDescription, 1)));
		IO::Folder AddonFolder(location, IO::Folder::Create);
		GSErrCode  GSErr = AddonFolder.GetStatus();
		if (GSErr == NoError)
		{
			GSErr = location.ToPath(&AddonDataDirectoryString);
			if (GSErr == NoError)
			{
				UE_AC_TraceF("AddonDataDirectory=\"%s\"\n", AddonDataDirectoryString.ToUtf8());
			}
			else
			{
				UE_AC_ErrorMsgF("GetAddonDataDirectory - Accessing AddonSupport path error(%s)\n", GSErr);
			}
		}
		else
		{
			UE_AC_ErrorMsgF("GetAddonDataDirectory - Accessing AddonSupport folder error(%s)\n", GSErr);
		}
	}
	return AddonDataDirectoryString;
}

// Return the addon version string
GS::UniString GetAddonVersionsStr()
{
	return GS::UniString::Printf("v%s, SDK AC=%d.%d, DS=%d, DL=%d", UE_AC_STRINGIZE(ADDON_VERSION), AC_VERSION, AC_SDK,
								 1, 1);
}

// Tool to get the name of layer
GS::UniString GetLayerName(API_AttributeIndex InLayer)
{
	// Get the layer's name
	GS::UniString LayerName;

	API_Attribute attribute;
	Zap(&attribute);
	attribute.header.typeID = API_LayerID;
	attribute.header.index = static_cast<decltype(attribute.header.index)>(InLayer);
	attribute.header.uniStringNamePtr = &LayerName;
	GSErrCode error = ACAPI_Attribute_Get(&attribute);
	if (error != NoError)
	{
		// This case happened for the special ArchiCAD layer
		if (error == APIERR_DELETED)
		{
			static const GS::UniString LayerDeleted("Layer deleted");
			LayerName = LayerDeleted;
		}
		else
		{
			utf8_string LayerError(Utf8StringFormat("Layer error=%s", GetErrorName(error)));
			LayerName = GS::UniString(LayerError.c_str(), CC_UTF8);
		}
	}
	return LayerName;
}

// Return current display unit name
const utf8_t* GetCurrentUnitDisplayName()
{
	const utf8_t*		 unitName = "";
	API_WorkingUnitPrefs unitPrefs;
	GSErrCode			 GSErr = ACAPI_Environment(APIEnv_GetPreferencesID, &unitPrefs, (void*)APIPrefs_WorkingUnitsID);
	if (GSErr == GS::NoError)
	{
		switch (unitPrefs.lengthUnit)
		{
			case API_LengthTypeID::Meter:
				unitName = "m";
				break;
			case API_LengthTypeID::Decimeter:
				unitName = "dm";
				break;
			case API_LengthTypeID::Centimeter:
				unitName = "cm";
				break;
			case API_LengthTypeID::Millimeter:
				unitName = "mm";
				break;
			case API_LengthTypeID::FootFracInch:
				unitName = "'";
				break;
			case API_LengthTypeID::FootDecInch:
				unitName = "'";
				break;
			case API_LengthTypeID::DecFoot:
				unitName = "'";
				break;
			case API_LengthTypeID::FracInch:
				unitName = "\"";
				break;
			case API_LengthTypeID::DecInch:
				unitName = "\"";
				break;
			default:
				UE_AC_DebugF("GetCurrentUnitDisplayName - Unknown length unit\n");
		}
	}

	return unitName;
}

// Return current time as string
utf8_string GetCurrentLocalDateTime()
{
	time_t	  CurrentTime;
	struct tm timeinfo;
	utf8_t	  Buffer[256];
	time(&CurrentTime);
#if PLATFORM_WINDOWS
	UE_AC_Assert(localtime_s(&timeinfo, &CurrentTime) == 0);
	UE_AC_Assert(asctime_s(Buffer, &timeinfo) == 0);
	return Buffer;
#else
	return asctime_r(localtime_r(&CurrentTime, &timeinfo), Buffer);
#endif
}

// Return true if 3d window is the current
bool Is3DCurrenWindow()
{
	API_WindowInfo WindowInfo;
	Zap(&WindowInfo);
	GSErrCode GSErr = ACAPI_Database(APIDb_GetCurrentWindowID, &WindowInfo);
	if (GSErr != NoError)
	{
		if (GSErr == APIERR_BADWINDOW)
		{
			return false;
		}
		UE_AC_DebugF("Is3DCurrenWindow - APIDb_GetCurrentWindowID error=%s\n", GetErrorName(GSErr));

		// Something wrong happens. Might not be a 3D window. Not taking any chance
		return false;
	}

	return WindowInfo.typeID == APIWind_3DModelID;
}

END_NAMESPACE_UE_AC
