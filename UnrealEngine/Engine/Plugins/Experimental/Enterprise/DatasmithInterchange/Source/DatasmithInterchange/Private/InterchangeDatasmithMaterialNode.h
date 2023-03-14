// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DatasmithDefinitions.h"

#include "Nodes/InterchangeBaseNode.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeShaderGraphNode.h"

#include "Engine/EngineTypes.h"

#include "InterchangeDatasmithMaterialNode.generated.h"

class IDatasmithBaseMaterialElement;
class UInterchangeBaseNodeContainer;

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithMaterialNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("DatasmithMaterialNode");
		return TypeName;
	}

	void SetMaterialType(EDatasmithReferenceMaterialType MaterialType)
	{
		AddInt32Attribute(MaterialTypeAttrName, (int32)MaterialType);
	}

	EDatasmithReferenceMaterialType GetMaterialType()
	{
		int32 AttributeValue;
		return GetInt32Attribute(MaterialTypeAttrName, AttributeValue) ? (EDatasmithReferenceMaterialType)AttributeValue : EDatasmithReferenceMaterialType::Opaque;
	}

	void SetMaterialQuality(EDatasmithReferenceMaterialQuality MaterialQuality)
	{
		AddInt32Attribute(MaterialQualityAttrName, (int32)MaterialQuality);
	}

	EDatasmithReferenceMaterialQuality GetMaterialQuality()
	{
		int32 AttributeValue;
		return GetInt32Attribute(MaterialQualityAttrName, AttributeValue) ? (EDatasmithReferenceMaterialQuality)AttributeValue : EDatasmithReferenceMaterialQuality::High;
	}

	void SetParentPath(const TCHAR* InSelectorName)
	{
		AddStringAttribute(MaterialParentAttrName, InSelectorName ? InSelectorName : FString());
	}

	FString GetParentPath()
	{
		FString AttributeValue;
		return GetStringAttribute(MaterialParentAttrName, AttributeValue) ? AttributeValue : FString();
	}

public:
	// TODO: See if it is not more appropriate to use IMPLEMENT_NODE_ATTRIBUTE_GETTER/SETTER defines
	static const FName MaterialTypeAttrName;
	static const FName MaterialQualityAttrName;
	static const FName MaterialParentAttrName;
};

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithPbrMaterialNode : public UInterchangeShaderGraphNode
{
	GENERATED_BODY()

public:

	UInterchangeDatasmithPbrMaterialNode()
	{
		MaterialFunctionsDependencies.Initialize(Attributes, MaterialFunctionsDependenciesKey);
	}

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("DatasmithPbrMaterialNode");
		return TypeName;
	}

	bool GetCustomShadingModel(EMaterialShadingModel& AttributeValue) const
	{
		int32 LocalAttributeValue;
		if (GetInt32Attribute(ShadingModelAttrName, LocalAttributeValue))
		{
			AttributeValue = static_cast<EMaterialShadingModel>(LocalAttributeValue);
			return true;
		}

		return false;
	}

	bool SetCustomShadingModel(EMaterialShadingModel AttributeValue)
	{
		return AddInt32Attribute(ShadingModelAttrName, static_cast<int32>(AttributeValue));
	}

	bool GetCustomBlendMode(EBlendMode& AttributeValue) const
	{
		int32 LocalAttributeValue;
		if (GetInt32Attribute(BlendModeAttrName, LocalAttributeValue))
		{
			AttributeValue = static_cast<EBlendMode>(LocalAttributeValue);
			return true;
		}

		return false;
	}

	bool SetCustomBlendMode(EBlendMode AttributeValue)
	{
		return AddInt32Attribute(BlendModeAttrName, static_cast<int32>(AttributeValue));
	}

	bool GetCustomOpacityMaskClipValue(float& AttributeValue) const
	{
		return GetFloatAttribute(OpacityMaskClipValueAttrName, AttributeValue);
	}

	bool SetCustomOpacityMaskClipValue(float AttributeValue)
	{
		return AddFloatAttribute(OpacityMaskClipValueAttrName, AttributeValue);
	}

	bool GetCustomTranslucencyLightingMode(ETranslucencyLightingMode& AttributeValue) const
	{
		int32 LocalAttributeValue;
		if (GetInt32Attribute(TranslucencyLightingModeAttrName, LocalAttributeValue))
		{
			AttributeValue = static_cast<ETranslucencyLightingMode>(LocalAttributeValue);
			return true;
		}

		return false;
	}

	bool SetCustomTranslucencyLightingMode(ETranslucencyLightingMode AttributeValue)
	{
		return AddInt32Attribute(TranslucencyLightingModeAttrName, static_cast<int32>(AttributeValue));
	}

	void GetMaterialFunctionsDependencies(TArray<FString>& MaterialFunctionUids)
	{
		MaterialFunctionsDependencies.GetItems(MaterialFunctionUids);
	}

	bool AddMaterialFunctionsDependency(const FString& MaterialFunctionUid)
	{
		return MaterialFunctionsDependencies.AddItem(MaterialFunctionUid);
	}

	bool RemoveAllMaterialFunctionsDependencies()
	{
		return MaterialFunctionsDependencies.RemoveAllItems();
	}

private:
	static const FName ShadingModelAttrName;
	static const FName BlendModeAttrName;
	static const FName OpacityMaskClipValueAttrName;
	static const FName TranslucencyLightingModeAttrName;
	static const FString MaterialFunctionsDependenciesKey;

	UE::Interchange::TArrayAttributeHelper<FString> MaterialFunctionsDependencies;
};

namespace UE::DatasmithInterchange::MaterialUtils
{
	extern const FName DefaultOutputIndexAttrName;
	extern const FName MaterialFunctionPathAttrName;

	void ProcessMaterialElements(TArray<TSharedPtr<IDatasmithBaseMaterialElement>>& InOutMaterialElements);
	UInterchangeBaseNode* AddMaterialNode(const TSharedPtr<IDatasmithBaseMaterialElement>& MaterialElement, UInterchangeBaseNodeContainer& NodeContainer);
}
