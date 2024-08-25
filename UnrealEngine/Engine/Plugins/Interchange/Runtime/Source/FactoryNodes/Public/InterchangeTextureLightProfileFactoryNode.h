// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTexture2DFactoryNode.h"
#include "Nodes/InterchangeBaseNode.h"

#if WITH_ENGINE
#include "Engine/Texture.h"
#include "Engine/TextureLightProfile.h"
#endif


#include "InterchangeTextureLightProfileFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeTextureLightProfileFactoryNode : public UInterchangeTexture2DFactoryNode
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool GetCustomBrightness(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(Brightness, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool SetCustomBrightness(const float AttributeValue, bool bAddApplyDelegate = true)
	{
#if WITH_EDITORONLY_DATA
		IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeTextureLightProfileFactoryNode, Brightness, float, UTextureLightProfile)
#else
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(Brightness, float)
#endif
	}


	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool GetCustomTextureMultiplier(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(TextureMultiplier, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool SetCustomTextureMultiplier(const float AttributeValue, bool bAddApplyDelegate = true)
	{
#if WITH_EDITORONLY_DATA
		IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeTextureLightProfileFactoryNode, TextureMultiplier, float, UTextureLightProfile)
#else
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(TextureMultiplier, float)
#endif
	}

	virtual void CopyWithObject(const UInterchangeFactoryBaseNode* SourceNode, UObject* Object) override
	{
		Super::CopyWithObject(SourceNode, Object);

#if WITH_EDITORONLY_DATA
		if (const UInterchangeTextureLightProfileFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureLightProfileFactoryNode>(SourceNode))
		{
			COPY_NODE_DELEGATES(TextureFactoryNode, Brightness, float, UTextureLightProfile)
			COPY_NODE_DELEGATES(TextureFactoryNode, TextureMultiplier, float, UTextureLightProfile)
		}
#endif
	}

private:
	virtual UClass* GetObjectClass() const override
	{
		return UTextureLightProfile::StaticClass();
	}

	// Addressing
	IMPLEMENT_NODE_ATTRIBUTE_KEY(Brightness);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(TextureMultiplier);

};