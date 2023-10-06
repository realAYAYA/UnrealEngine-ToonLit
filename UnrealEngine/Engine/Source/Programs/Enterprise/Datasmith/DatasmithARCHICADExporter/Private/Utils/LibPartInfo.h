// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FGSUnID
{
  public:
	typedef char Buffer[128];
	GS::Guid	 Main;
	GS::Guid	 Rev;

	static const char* UnIDNullStr;

	GS::GSErrCode InitWithString(const Buffer InStr);

	// Equality operator needed for use as FMap key
	bool operator==(const FGSUnID& InOther) const { return Main == InOther.Main && Rev == InOther.Rev; }
};

inline uint32 GetTypeHash(const FGSUnID& A)
{
	return FCrc::TypeCrc32(A);
}

class FLibPartInfo
{
  public:
	// Constructor
	FLibPartInfo()
		: Index(0)
	{
	}

	// Intialize
	void Initialize(GS::Int32 InIndex);

	GS::Int32	  Index; // LibPart index
	FGSUnID		  Guid; // LibPart Guid
	GS::UniString Name; // LibPart name
};

// Class that will auto delete location field
class FAuto_API_LibPart : public API_LibPart
{
  public:
	FAuto_API_LibPart() { Zap((API_LibPart*)this); }
	~FAuto_API_LibPart()
	{
		delete location;
		location = nullptr;
	}
};

const API_AddParType* GetParameter(API_AddParType** InParameters, const char* InParameterName);
bool GetParameter(API_AddParType** InParameters, const char* InParameterName, GS::UniString* OutString);
bool GetParameter(API_AddParType** InParameters, const char* InParameterName, double* OutValue);
bool GetParameter(API_AddParType** InParameters, const char* InParameterName, bool* OutFlag);

END_NAMESPACE_UE_AC
