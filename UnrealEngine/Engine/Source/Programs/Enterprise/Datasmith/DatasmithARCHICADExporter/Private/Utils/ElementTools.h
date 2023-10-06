// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FElementTools
{
  public:
	// Tool: return the info string (≈ name)
	static bool GetInfoString(const API_Guid& InGUID, GS::UniString* OutString);

	// Tool: Return the localize name for element type id
	static const utf8_t* TypeName(API_ElemTypeID InElementType);

	// Tool: Return the localize name for element's type
	static const utf8_t* TypeName(const API_Guid& InElementGuid);

	// Tool: Return the variation as string
	static utf8_string GetVariationAsString(API_ElemVariationID InVariation);

	// Tool: return libpart index (or 0 if no libpart)
	static GS::Int32 GetLibPartIndex(const API_Element& InElement);

	static GS::Guid GetLibPartId(const API_Elem_Head& InElement);

	// Tool: return element's owner guid
	static API_Guid GetOwner(const API_Element& InElement);

	// Tool: return element's owner guid
	static API_Guid GetOwner(const API_Guid& InElementGuid);

	// Tool: return owner offset for specified element type
	static size_t GetOwnerOffset(API_ElemTypeID InElementType);

	// Tool: return classifications of the element
	static GSErrCode GetElementClassifications(
		GS::Array< GS::Pair< API_ClassificationSystem, API_ClassificationItem > >& OutClassifications,
		const API_Guid&															   InElementGuid);

	static GSErrCode GetElementProperties(GS::Array< API_Property >& OutProperties, const API_Guid& InElementGuid);
};

END_NAMESPACE_UE_AC
