// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasmithDefinitions.h"
#include "Nodes/InterchangeBaseNode.h"

class IDatasmithTextureElement;
class UInterchangeTextureNode;

namespace UE::DatasmithInterchange
{
	/**
	 * Helper struct to get/set Datasmith texture attributes on a node.
	 */
	struct FInterchangeDatasmithTextureDataConst
	{
		static bool HasData(const UInterchangeBaseNode* Node);

		FInterchangeDatasmithTextureDataConst(const UInterchangeBaseNode* InNode);

		bool GetCustomFile(FString& AttributeValue) const;

		bool GetCustomTextureMode(EDatasmithTextureMode& AttributeValue) const;

		bool GetCustomTextureFilter(EDatasmithTextureFilter& AttributeValue) const;

		bool GetCustomTextureAddressX(EDatasmithTextureAddress& AttributeValue) const;

		bool GetCustomTextureAddressY(EDatasmithTextureAddress& AttributeValue) const;

		bool GetCustomAllowResize(bool& AttributeValue) const;

		bool GetCustomRGBCurve(float& AttributeValue) const;

		bool GetCustomSRGB(EDatasmithColorSpace& AttributeValue) const;

	protected:
		const UInterchangeBaseNode* ConstBaseNode;

		static const FName DatasmithTextureDataKey;
		static const FName FileKey;
		static const FName TextureModeKey;
		static const FName TextureFilterKey;
		static const FName TextureAddressXKey;
		static const FName TextureAddressYKey;
		static const FName AllowResizeKey;
		static const FName RGBCurveKey;
		static const FName SRGBKey;
	};

	struct FInterchangeDatasmithTextureData : public FInterchangeDatasmithTextureDataConst
	{
		FInterchangeDatasmithTextureData(UInterchangeBaseNode* InNode);

		bool SetCustomFile(const FString& AttributeValue);

		bool SetCustomTextureMode(const EDatasmithTextureMode& AttributeValue);

		bool SetCustomTextureFilter(const EDatasmithTextureFilter& AttributeValue);

		bool SetCustomTextureAddressX(const EDatasmithTextureAddress& AttributeValue);

		bool SetCustomTextureAddressY(const EDatasmithTextureAddress& AttributeValue);

		bool SetCustomAllowResize(const bool& AttributeValue);

		bool SetCustomRGBCurve(const float& AttributeValue);

		bool SetCustomSRGB(const EDatasmithColorSpace& AttributeValue);

	private:
		UInterchangeBaseNode* BaseNode;
	};

	namespace TextureUtils
	{
		/**
		 * Register the properties of the TextureElement into the BaseNode's attributes.
		 */
		void ApplyTextureElementToNode(const TSharedRef<IDatasmithTextureElement>& TextureElement, UInterchangeBaseNode* BaseNode);
	}
}