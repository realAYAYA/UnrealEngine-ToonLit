// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTextureNode.h"

#include "InterchangeTexture2DNode.generated.h"

class UInterchangeBaseNodeContainer;

//Interchange namespace
namespace UE::Interchange
{
	struct FTexture2DNodeStaticData : public FBaseNodeStaticData
	{
		static const FAttributeKey& GetBaseSourceBlocksKey()
		{
			static FAttributeKey StringKey(TEXT("SourceBlocks"));
			return StringKey;
		}
	};
}//ns UE::Interchange

UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeTexture2DNode : public UInterchangeTextureNode
{
	GENERATED_BODY()

public:

	/**
	 * Build and return a UID name for a texture 2D node.
	 */
	static FString MakeNodeUid(const FStringView NodeName);

	/**
	 * Creates a new UInterchangeTexture2DNode and adds it to NodeContainer as a translated node.
	 */
	static UInterchangeTexture2DNode* Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView TextureNodeName);

	virtual void PostInitProperties()
	{
		Super::PostInitProperties();
		SourceBlocks.Initialize(Attributes.ToSharedRef(), UE::Interchange::FTexture2DNodeStaticData::GetBaseSourceBlocksKey().ToString());
	}

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("Texture2DNode");
		return TypeName;
	}

	/**
	 * Override serialize to restore SourceBlocks on load.
	 */
	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		if (Ar.IsLoading() && bIsInitialized)
		{
			SourceBlocks.RebuildCache();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// UDIMs begin here
	// UDIM base texture use a different model for the source data

	/**
	 * Get the source blocks for the texture
	 * If the map is empty then the texture will be simply be imported as normal texture using the payload key
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture | UDIMs")
	TMap<int32, FString> GetSourceBlocks() const
	{
		return SourceBlocks.ToMap();
	}

	/**
	 * Set the source blocks
	 * Using this will suggest the pipeline to consider this texture as UDIM and it can chose to pass or not these block to the texture factory node.
	 * @param InSourceBlocks The blocks and their source image that compose the whole texture.
	 * The textures must be of the same format and use the same pixel format
	 * The first block in the map is used to determine the accepted texture format and pixel format
	 */
	void SetSourceBlocks(TMap<int32, FString>&& InSourceBlocks)
	{
		SourceBlocks = InSourceBlocks;
	}

	
	/**
	 * Set the source blocks
	 * Using this will suggest the pipeline to consider this texture as UDIM and it can chose to pass or not these block to the texture factory node.
	 * @param InSourceBlocks The blocks and their source image that compose the whole texture.
	 * The textures must be of the same format and use the same pixel format
	 * The first block in the map is used to determine the accepted texture format and pixel format
	 */
	void SetSourceBlocks(const TMap<int32, FString>& InSourceBlocks)
	{
		SourceBlocks = InSourceBlocks;
	}

	// UDIMs ends here
	//////////////////////////////////////////////////////////////////////////

protected:

	UE::Interchange::TMapAttributeHelper<int32, FString> SourceBlocks;

// Custom attributes
public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool SetCustomWrapU(const EInterchangeTextureWrapMode& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(WrapU, EInterchangeTextureWrapMode);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool GetCustomWrapU(EInterchangeTextureWrapMode& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(WrapU, EInterchangeTextureWrapMode);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool SetCustomWrapV(const EInterchangeTextureWrapMode& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(WrapV, EInterchangeTextureWrapMode);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Texture")
	bool GetCustomWrapV(EInterchangeTextureWrapMode& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(WrapV, EInterchangeTextureWrapMode);
	}

private:
	IMPLEMENT_NODE_ATTRIBUTE_KEY(WrapU);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(WrapV);
};
