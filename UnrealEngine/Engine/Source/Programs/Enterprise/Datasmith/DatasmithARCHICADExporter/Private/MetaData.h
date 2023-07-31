// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

BEGIN_NAMESPACE_UE_AC

class FMetaData
{
  public:
	//	FMetaData(const GS::Guid& InElementID);

	FMetaData(const TSharedPtr< IDatasmithElement >& InElement);

	void SetAssociatedElement(const GS::Guid& /* InElementID */, const TSharedPtr< IDatasmithElement >& InElement)
	{
		MetaData->SetAssociatedElement(InElement);
	}

	bool SetOrUpdate(TSharedPtr< IDatasmithMetaDataElement >* IOPtr, IDatasmithScene* IOScene);

	void ExportMetaData(const GS::Guid& InElementID);

	const TSharedRef< IDatasmithMetaDataElement >& GetMetaData() const { return MetaData; }

	void AddProperty(const TCHAR* InPropKey, EDatasmithKeyValuePropertyType InPropertyValueType, const TCHAR* InValue);

	void AddProperty(const TCHAR* InPropKey, EDatasmithKeyValuePropertyType InPropertyValueType,
					 const GS::UniString& InValue)
	{
		AddProperty(InPropKey, InPropertyValueType, GSStringToUE(InValue));
	}

	void AddStringProperty(const TCHAR* InPropKey, const TCHAR* InValue)
	{
		AddProperty(InPropKey, EDatasmithKeyValuePropertyType::String, InValue);
	}

	void AddStringProperty(const TCHAR* InPropKey, const GS::UniString& InValue)
	{
		AddProperty(InPropKey, EDatasmithKeyValuePropertyType::String, InValue);
	}

  private:
	void AddMetaDataProperty(API_VariantType variantType, const GS::UniString& PropertyKey,
							 const GS::UniString& PropertyValue);

	void ExportElementIDProperty(const API_Guid& InlementId);

	void ExportClassifications(const API_Guid& InlementId);

	void ExportCategories(const API_Guid& InlementId);

	void ExportIFCType(const API_Guid& InlementId);

	void ExportIFCProperties(const API_Guid& InlementId);

	void ExportIFCAttributes(const API_Guid& InlementId);

	void ExportProperties(const API_Guid& InlementId);

	GS::UniString GetPropertyValueString(const API_IFCPropertyValue& InIFCPropertyValue);

	GS::UniString GetPropertyValueString(const API_Variant& InVariant);

	TSharedRef< IDatasmithMetaDataElement > MetaData;
};

END_NAMESPACE_UE_AC
