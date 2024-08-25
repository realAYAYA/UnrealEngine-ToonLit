// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FbxInclude.h"
#include "Misc/TVariant.h"

/** Forward declarations */
class UInterchangeBaseNode;
class UInterchangeBaseNodeContainer;
class UInterchangeShaderGraphNode;
class UInterchangeShaderNode;
class UInterchangeTexture2DNode;
class UInterchangeSceneNode;

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			class FFbxParser;

			class FFbxMaterial
			{
			public:
				explicit FFbxMaterial(FFbxParser& InParser)
					: Parser(InParser)
				{}

				/**
				 * Create a UInterchangeFextureNode and add it to the NodeContainer for each texture of type FbxFileTexture that the FBX file contains.
				 *
				 * @note - Any node that already exists in the NodeContainer will not be created or modified.
				 */
				void AddAllTextures(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer);
				
				/**
				 * Create a UInterchangeMaterialNode and add it to the NodeContainer for each material of type FbxSurfaceMaterial that the FBX file contains.
				 * 
				 * @note - Any node that already exists in the NodeContainer will not be created or modified.
				 */
				void AddAllMaterials(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer);

				/**
				 * Create a UInterchangeMaterialNode and add it to the NodeContainer for each material of type FbxSurfaceMaterial that the FBX ParentFbxNode contains.
				 * Also set the dependencies of the node materials on the Interchange ParentNode.
				 * 
				 * @note - Any material node that already exists in the NodeContainer will be added as a dependency.
				 */
				void AddAllNodeMaterials(UInterchangeSceneNode* SceneNode, FbxNode* ParentFbxNode, UInterchangeBaseNodeContainer& NodeContainer);

			protected:
				UInterchangeShaderGraphNode* CreateShaderGraphNode(UInterchangeBaseNodeContainer& NodeContainer, const FString& NodeUID, const FString& NodeName);
				const UInterchangeTexture2DNode* CreateTexture2DNode(UInterchangeBaseNodeContainer& NodeContainer, const FString& TextureFilePath);
				bool ConvertPropertyToShaderNode(UInterchangeBaseNodeContainer& NodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode,
												 FbxProperty& Property, float Factor, FName PropertyName, const TVariant<FLinearColor, float>& DefaultValue, bool bInverse = false);

			private:
				const UInterchangeShaderGraphNode* AddShaderGraphNode(FbxSurfaceMaterial* SurfaceMaterial, UInterchangeBaseNodeContainer& NodeContainer);
				void ConvertShininessToShaderNode(FbxSurfaceMaterial& SurfaceMaterial, UInterchangeBaseNodeContainer& NodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode);
				const UInterchangeShaderNode* CreateTextureSampler(FbxFileTexture* FbxTexture, UInterchangeBaseNodeContainer& NodeContainer, const FString& ShaderUniqueID, const FString& InputName);

				FFbxParser& Parser;
			};
		}//ns Private
	}//ns Interchange
}//ns UE
