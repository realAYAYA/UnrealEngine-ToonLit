// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalizeTools.h"
#include "AddonTools.h"
#include "ResourcesUtils.h"
#include "CurrentOS.h"

#include <stdexcept>

BEGIN_NAMESPACE_UE_AC

// Struct to associate language string to language id
typedef struct
{
	const utf8_t* Txt;
	int			  Id;
} LanguageTxt2Id;

// Table associate ARCHICAD languages to this add-ons languages
static LanguageTxt2Id TabLanguageTxt2Id[] = {{"AUS", kLangEnglish},	   {"INT", kLangEnglish},	 {"NZE", kLangEnglish},
											 {"ENG", kLangEnglish},	   {"USA", kLangEnglish},	 {"IND", kLangEnglish},
											 {"FRA", kLangFrench},	   {"AUT", kLangGerman},	 {"CHE", kLangGerman},
											 {"GER", kLangGerman},	   {"SPA", kLangSpanish},	 {"ITA", kLangItalian},
											 {"JPN", kLangJapanese},   {"HUN", kLangHungarian},	 {"RUS", kLangRussian},
											 {"GRE", kLangGreec},	   {"CHI", kLangChinese},	 {"TAI", kLangChinese},
											 {"POR", kLangPortuguese}, {"BRA", kLangPortuguese}, {"KOR", kLangKorean},

											 {nullptr, kLangUndefined}};

// Table associate OS languages to this add-ons languages
// clang-format off
static LanguageTxt2Id TabISO6312Id[] = {
	{"en", kLangEnglish},	  {"fr", kLangFrench},	   {"de", kLangGerman},
	{"es", kLangSpanish},	  {"it", kLangItalian},	   {"ja", kLangJapanese},
	{"hu", kLangHungarian},	  {"ru", kLangRussian},	   {"el", kLangGreec},
	{"zh", kLangChinese},	  {"pt", kLangPortuguese}, {"ko", kLangKorean},

	{nullptr, kLangUndefined}};
// clang-format on

// Return the current language
GSResID GetCurrentLanguage()
{
	static GSResID CurrentLanguage = 0;

	if (CurrentLanguage == 0)
	{
		const LanguageTxt2Id* l;
		GS::UniString		  LanguageName;

#if 1 // 1- Use AC current language, 0- User user prefered language
	  // Try ARCHICAD current language
		API_ServerApplicationInfo AppInfo;
		GSErrCode				  GSErr = ACAPI_Environment(APIEnv_ApplicationID, &AppInfo);
		if (GSErr == GS::NoError)
		{
			utf8_string AppLanguage = AppInfo.language.ToUtf8();
			l = TabLanguageTxt2Id;
			while (l->Txt && AppLanguage != l->Txt)
			{
				l++;
			}
			if (l->Txt)
			{
				if (RSGetIndString(&LanguageName, l->Id * 1000 + kStrLanguageName, 1, ACAPI_GetOwnResModule()))
				{
					UE_AC_TraceF("UE_AC::GetCurrentLanguage - Set language \"%s\" as current language\n",
								 LanguageName.ToUtf8());
					CurrentLanguage = l->Id;
				}
				else
				{
					UE_AC_TraceF("UE_AC::GetCurrentLanguage - Language \"%s\" not present\n", AppLanguage.c_str());
				}
			}
			else
			{
				UE_AC_TraceF("UE_AC::GetCurrentLanguage - Unknown ArchiCAD language \"%s\"\n", AppLanguage.c_str());
			}
		}
		else
		{
			UE_AC_DebugF("UE_AC::GetCurrentLanguage - Can't get application info (error=%s)\n", GetErrorName(GSErr));
		}
#endif

		// Try user prefered languages
		if (CurrentLanguage == 0)
		{
			VecStrings PrefLanguages = GetPrefLanguages();
			for (size_t i = 0; i < PrefLanguages.size() && CurrentLanguage == 0; i++)
			{
				utf8_string PrefLanguage(PrefLanguages[i].substr(0, 2));
				l = TabISO6312Id;
				while (l->Txt && PrefLanguage != l->Txt)
				{
					l++;
				}
				if (l->Txt &&
					RSGetIndString(&LanguageName, l->Id * 1000 + kStrLanguageName, 1, ACAPI_GetOwnResModule()))
				{
					UE_AC_TraceF("UE_AC::GetCurrentLanguage - Set prefered language \"%s\" as current language\n",
								 LanguageName.ToUtf8());
					CurrentLanguage = l->Id;
				}
			}
		}

		// Default to english
		if (CurrentLanguage == 0)
		{
			CurrentLanguage = kLangEnglish;
			UE_AC_TraceF("UE_AC::GetCurrentLanguage - English set as default language\n");
		}
	}

	return CurrentLanguage;
}

// Return a localized string
GS::UniString GetUniString(GSResID InResID, Int32 InStrIndex)
{
	GS::UniString s;
	bool		  strExist = RSGetIndString(&s, LocalizeResId(InResID), InStrIndex, ACAPI_GetOwnResModule());
	if (!strExist)
	{
		throw std::runtime_error("String ressource not found");
	}
	return s;
}

// Constructor
FMultiString::FMultiString(GSResID InResID, Int32 InStartIndex)
	: StringsListResID(InResID)
	, StartIndex(InStartIndex)
	, bInitialized(false)
{
}

// Destructor
FMultiString::~FMultiString() {}

// Initialize
bool FMultiString::IsValidIndex(Int32 InStrIndex)
{
	if (!bInitialized)
	{
		GS::UniString s;
		GSResID		  resID = LocalizeResId(StringsListResID);
		for (Int32 i = 1; RSGetIndString(&s, resID, i, ACAPI_GetOwnResModule()); i++)
		{
			mGSStrings.push_back(s);
			StdStrings.push_back(s.ToUtf8());
		}
		bInitialized = true;
	}
	if (size_t(InStrIndex - StartIndex) >= StdStrings.size())
	{
		DBPrintf("Invalid string index (ResID=%d, Idx=%d)", StringsListResID, InStrIndex);
		return false;
	}
	return true;
}

// Get string
const utf8_t* FMultiString::GetStdString(Int32 InStrIndex)
{
	if (!IsValidIndex(InStrIndex))
	{
		return "(Invalid string index)";
	}
	return StdStrings[size_t(InStrIndex - StartIndex)].c_str();
}

// Get string
const GS::UniString& FMultiString::GetGSString(Int32 InStrIndex)
{
	if (!IsValidIndex(InStrIndex))
	{
		static const GS::UniString invalid("(Invalid string index)");
		return invalid;
	}
	return mGSStrings[size_t(InStrIndex - StartIndex)];
}

// Print 2 debugger all strings of the indexed ressource string
void DumpIndStrings(GSResID InResId)
{
	GSResID		  LocalizedId = LocalizeResId(InResId);
	GS::UniString s;
	for (GS::Int32 i = 1; RSGetIndString(&s, LocalizedId, i, ACAPI_GetOwnResModule()); i++)
	{
		UE_AC_TraceF("---[%d] \"%s\"\n", i, s.ToUtf8());
	}
	UE_AC_TraceF("\n");
}

END_NAMESPACE_UE_AC
