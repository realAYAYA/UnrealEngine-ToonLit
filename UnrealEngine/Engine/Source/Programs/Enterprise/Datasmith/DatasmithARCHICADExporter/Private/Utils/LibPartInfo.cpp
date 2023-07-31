// Copyright Epic Games, Inc. All Rights Reserved.

#include "LibPartInfo.h"

#include <stdexcept>

BEGIN_NAMESPACE_UE_AC

const char* FGSUnID::UnIDNullStr = "{00000000-0000-0000-0000-000000000000}-{00000000-0000-0000-0000-000000000000}";

GS::GSErrCode FGSUnID::InitWithString(const Buffer InStr)
{
	GS::Guid	main, rev;
	char		guidStr[60];
	const Int32 strLen = sizeof(guidStr) / sizeof(guidStr[0]);
	Int32		i;

	const char* uiStr = InStr;
	while (*uiStr == ' ' || *uiStr == '\t')
	{
		uiStr++;
	}

	if (*uiStr++ != '{')
	{
		return Error;
	}

	for (i = 0; i < strLen - 1 && *uiStr != 0 && *uiStr != '}'; i++, uiStr++)
	{
		guidStr[i] = *uiStr;
	}
	if (*uiStr != '}')
	{
		return Error;
	}
	guidStr[i] = 0;

	if (main.ConvertFromString(guidStr) != NoError)
	{
		return Error;
	}

	if (*(++uiStr)++ != '-' || *uiStr++ != '{')
	{
		return Error;
	}

	for (i = 0; i < strLen - 1 && *uiStr != 0 && *uiStr != '}'; i++, uiStr++)
	{
		guidStr[i] = *uiStr;
	}
	if (*uiStr != '}')
	{
		return Error;
	}
	guidStr[i] = 0;

	if (rev.ConvertFromString(guidStr) != NoError)
	{
		return Error;
	}

	Main = main;
	Rev = rev;

	return NoError;
}

// Intialize
void FLibPartInfo::Initialize(GS::Int32 InIndex)
{
	if (Index != 0)
	{
		return;
	}
	Index = InIndex;
	FAuto_API_LibPart LibPart;
	LibPart.index = InIndex;
	GSErrCode err = ACAPI_LibPart_Get(&LibPart);
	if (err == APIERR_MISSINGDEF)
	{
		UE_AC_TraceF("FLibPartInfo::FLibPartInfo - LibPart missing index=%d, name=\"%s\"\n", InIndex,
					 GS::UniString(LibPart.docu_UName).ToUtf8());
	}
	else if (err != GS::NoError)
	{
		throw std::runtime_error(
			Utf8StringFormat("FLibPartInfo::FLibPartInfo - LibPart index=%d, error=%d\n", InIndex, err));
	}
	Name = LibPart.docu_UName;
	Guid.InitWithString(LibPart.ownUnID);
	if (Guid.Main.IsNull())
	{
		// LibPart id can be null
		if (strncmp(LibPart.ownUnID, FGSUnID::UnIDNullStr, sizeof(LibPart.ownUnID)) != 0)
			UE_AC_DebugF("FLibPartInfo::FLibPartInfo - LibPart id is null index=%d, name=\"%s\", id=\"%s\"\n", InIndex,
						 GS::UniString(LibPart.docu_UName).ToUtf8(), LibPart.ownUnID);

		Guid.Main = APIGuid2GSGuid(String2API_Guid(Name)); // Simulate guid from name
	}
}

const API_AddParType* GetParameter(API_AddParType** InParameters, const char* InParameterName)
{
	UInt32 nParams = BMGetHandleSize((GSHandle)InParameters) / sizeof(API_AddParType);

	GS::UInt32 j = 0;
	for (j = 0; j < nParams; ++j)
	{
		const API_AddParType* Parameter = *InParameters + j;
		if (strncmp(Parameter->name, InParameterName, API_NameLen) == 0)
		{
			// Parameter found
			if (Parameter->flags & API_ParFlg_Disabled)
			{
				break; // But declared as disabled
			}
			return Parameter;
		}
	}
	return nullptr;
}

bool GetParameter(API_AddParType** InParameters, const char* InParameterName, GS::UniString* OutString)
{
	const API_AddParType* Parameter = GetParameter(InParameters, InParameterName);
	if (Parameter == nullptr || Parameter->typeMod != API_ParSimple ||
		(Parameter->typeID != APIParT_CString && Parameter->typeID == APIParT_Title))
	{
		return false;
	}

	*OutString = GS::UniString(Parameter->value.uStr);
	return true;
}

bool GetParameter(API_AddParType** InParameters, const char* InParameterName, double* OutValue)
{
	const API_AddParType* Parameter = GetParameter(InParameters, InParameterName);
	if (Parameter == nullptr || Parameter->typeMod != API_ParSimple)
	{
		return false;
	}
	switch (Parameter->typeID)
	{
		case APIParT_Integer:
		case APIParT_LightSw:
		case APIParT_Intens:
		case APIParT_LineTyp:
		case APIParT_Mater:
		case APIParT_FillPat:
		case APIParT_PenCol:
		case APIParT_Boolean:
		case APIParT_Length:
		case APIParT_Angle:
		case APIParT_RealNum:
		case APIParT_ColRGB:
			break;

		default:
			return false;
	}

	*OutValue = Parameter->value.real;
	return true;
}

bool GetParameter(API_AddParType** InParameters, const char* InParameterName, bool* OutFlag)
{
	double Value = 0.0;
	if (!GetParameter(InParameters, InParameterName, &Value))
	{
		return false;
	}
	*OutFlag = Value != 0;
	return true;
}

END_NAMESPACE_UE_AC
