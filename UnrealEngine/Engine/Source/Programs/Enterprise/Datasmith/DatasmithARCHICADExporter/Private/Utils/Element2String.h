// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

// Class that will collect element info in a string
class FElement2String
{
  public:
	// Tool:Return some element information as a string
	static utf8_string GetElementAsString(const API_Element& InElement);

	// Tool:Return short element information as a string
	static utf8_string GetElementAsShortString(const API_Element& InElement);

	// Tool:Return some element information as a string
	static utf8_string GetElementAsShortString(const API_Guid& InId);

	// Tool:return all element's informations as a string
	static utf8_string GetAllElementAsString(const API_Element& InElement);

	// Tool:return all element's informations as a string
	static utf8_string GetAllElementAsString(const API_Guid& InElementGuid);

	// Tool: Trace element informations
	static void DumpInfo(const API_Guid& InElementGuid);

	// Tool:Return parameters as a string
	static utf8_string GetParametersAsString(const API_Guid& InElementGuid);

	// Tool:Return IFC attributes as a string
	static utf8_string GetIFCAttributesAsString(const API_Guid& InElementGuid);

	// Tool:Return IFC properties as a string
	static utf8_string GetIFCPropertiesAsString(const API_Guid& InElementGuid);

	// Tool:Return components as a string
	static utf8_string GetComponentsAsString(const API_Elem_Head& InElementHeader);

	// Tool:Return descriptors as a string
	static utf8_string GetDescriptorsAsString(const API_Elem_Head& InElementHeader);

	// Tool:Return properties as a string
	static utf8_string GetPropertiesAsString(const API_Guid& InElementGuid);

	// Tool:Return property objects as a string
	static utf8_string GetPropertyObjectsAsString(const API_Elem_Head& InElementHeader);

  private:
	// Tool:Return parameters as a string
	static utf8_string GetParametersAsString(const API_AddParType* const* InParamsHandle);

	static bool IsPropertyGroupExportable(const API_Guid& InGroupGuid);

	static GS::UniString GetPropertyGroupName(const API_Guid& InGroupGuid);

	static utf8_string GetVariantValue(const API_Variant& InVariant);

	static utf8_string PropertyDefinition2String(const API_PropertyDefinition& InDefinition,
												 const API_PropertyValue& InValue, bool bInDefault);

	static utf8_string GetIFCPropertyValue(const API_IFCPropertyValue& InValue);

	static utf8_string GetIFCPropertyAnyValue(const API_IFCPropertyAnyValue& InValue);

	static void RemoveLeadingAndTrailing(utf8_string* IOString);
};

class FDump2String
{
	static utf8_string ListLibraries();
};

END_NAMESPACE_UE_AC
