// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFAsset.h"

#include "GLTFAnimation.h"
#include "Misc/Paths.h"
#include "GLTFMaterial.h"
#include "GLTFMesh.h"
#include "GLTFNode.h"
#include "GLTFTexture.h"

namespace GLTF
{
	namespace
	{
		template <typename T>
		void GenerateNames(const FString& Prefix, TArray<T>& Objects)
		{
			for (int32 Index = 0; Index < Objects.Num(); ++Index)
			{
				T& Obj = Objects[Index];

				Obj.UniqueId = Prefix + FString::FromInt(Index);

				if (Obj.Name.IsEmpty())
				{
					Obj.Name = Obj.UniqueId;
				}
			}
		}

		//Make the Name FName complient.
		//For Scenario where Asset.Name is '_x' (where x is a number) making FName to be none:
		//UE-174253
		template <typename T>
		void ValidateFNameCriteria(const FString& Prefix, TArray<T>& Objects)
		{
			for (int32 Index = 0; Index < Objects.Num(); ++Index)
			{
				T& Obj = Objects[Index];

				if (!Obj.Name.IsEmpty())
				{
					FName SanitizationTest(*Obj.Name);
					if (SanitizationTest.IsNone())
					{
						Obj.Name = Prefix + Obj.Name;
					}
				}
			}
		}
	}

	const FMetadata::FExtraData* FMetadata::GetExtraData(const FString& Name) const
	{
		return Extras.FindByPredicate([&Name](const FMetadata::FExtraData& Data) { return Data.Name == Name; });
	}

	const TSet<GLTF::EExtension> FAsset::SupportedExtensions = { GLTF::EExtension::KHR_MaterialsPbrSpecularGlossiness,
																 GLTF::EExtension::KHR_MaterialsUnlit,
																 GLTF::EExtension::KHR_LightsPunctual,
																 GLTF::EExtension::KHR_MaterialsClearCoat,
																 GLTF::EExtension::KHR_MaterialsTransmission,
																 GLTF::EExtension::KHR_MaterialsSheen,
																 GLTF::EExtension::KHR_MaterialsVariants,
																 GLTF::EExtension::KHR_MaterialsSpecular,
																 GLTF::EExtension::KHR_MaterialsIOR };

	void FAsset::Clear(uint32 BinBufferKBytes, uint32 ExtraBinBufferKBytes)
	{
		Buffers.Empty();
		BufferViews.Empty();
		Accessors.Empty();
		Meshes.Empty();

		Scenes.Empty();
		Nodes.Empty();
		Cameras.Empty();
		Lights.Empty();
		Skins.Empty();

		Images.Empty();
		Samplers.Empty();
		Textures.Empty();
		Materials.Empty();

		ExtensionsUsed.Empty((int)EExtension::Count);
		RequiredExtensions.Empty();
		Metadata.GeneratorName.Empty();
		Metadata.Extras.Empty();

		if (BinData.Num() > 0)
			BinData.Empty(BinBufferKBytes * 1024);
		if (ExtraBinData.Num() > 0)
			ExtraBinData.Empty(ExtraBinBufferKBytes * 1024);
	}

	void FAsset::GenerateNames(const FString& Prefix)
	{
		check(!Prefix.IsEmpty());

		{
			const FString ScenePrefix = Prefix + TEXT("_scene_");

			for (size_t SceneIndex = 0; SceneIndex < Scenes.Num(); SceneIndex++)
			{
				FScene& Scene = Scenes[SceneIndex];

				Scene.UniqueId = ScenePrefix + FString::FromInt(SceneIndex);

				if (Scene.Name.IsEmpty())
				{
					Scene.Name = Scene.UniqueId;
				}
			}
		}

		{
			const FString NodePrefix = Prefix + TEXT("_node_");
			const FString JointPrefix = Prefix + TEXT("_joint_");
			for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
			{
				FNode& Node = Nodes[NodeIndex];

				bool bIsJoint = Node.Type == FNode::EType::Joint;
				Node.UniqueId = (bIsJoint ? JointPrefix : NodePrefix);

				Node.UniqueId = Node.UniqueId + FString::FromInt(NodeIndex);

				if (Node.Name.IsEmpty())
				{
					Node.Name = Node.UniqueId;
				}
			}
		}

		{
			const FString TexPrefix = Prefix + TEXT("_texture_");
			for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
			{
				FTexture& Tex = Textures[TextureIndex];

				Tex.UniqueId = TexPrefix + FString::FromInt(TextureIndex);

				if (!Tex.Name.IsEmpty())
				{
					// keep base part of the name as it was in gltf
				}
				else if (!Tex.Source.Name.IsEmpty())
				{
					Tex.Name = Tex.Source.Name;
				}
				else if (!Tex.Source.URI.IsEmpty())
				{
					//in case texture is embedded:
					if (Tex.Source.URI.StartsWith(TEXT("data:")))
					{
						Tex.Name = Tex.UniqueId;
					}
					else
					{
						Tex.Name = FPaths::GetBaseFilename(Tex.Source.URI);
					}
				}
				else
				{
					Tex.Name = Tex.UniqueId;
				}

			}
		}

		{
			const FString CameraPrefix = Prefix + TEXT("_camera_");
			for (int32 CameraIndex = 0; CameraIndex < Cameras.Num(); ++CameraIndex)
			{
				FCamera& Camera = Cameras[CameraIndex];

				Camera.UniqueId = CameraPrefix + FString::FromInt(CameraIndex);

				if (Camera.Name.IsEmpty())
				{
					if (!Camera.Node.Name.IsEmpty())
					{
						Camera.Name = Camera.Node.Name; // cant be empty
					}
					else
					{
						Camera.Name = Camera.UniqueId;
					}
				}
			}
		}

		{
			const FString LightPrefix = Prefix + TEXT("_light_");
			for (int32 LightIndex = 0; LightIndex < Lights.Num(); ++LightIndex)
			{
				FLight& Light = Lights[LightIndex];

				Light.UniqueId = LightPrefix + FString::FromInt(LightIndex);

				if (Light.Name.IsEmpty())
				{
					if (Light.Node && !Light.Node->Name.IsEmpty())
					{
						Light.Name = Light.Node->Name;
					}
					else
					{
						Light.Name = Light.UniqueId;
					}
				}
			}
		}

		// General pattern here is that we'll have a Prefix that will be a filename (like MyFile). 
		// The name (display label) and the UniqueId of an object has been separated.
		// UniqueId will consist of: [Prefil]+[_ElementType_]+[ElementIndex]
		// Name will be the provided GLTF object.name, if present,
		// if Name is not present the UniqueId will be used as the Name.
		GLTF::GenerateNames(Prefix + TEXT("_material_"), Materials);
		GLTF::GenerateNames(Prefix + TEXT("_skin_"), Skins);
		GLTF::GenerateNames(Prefix + TEXT("_animation_"), Animations);
		GLTF::GenerateNames(Prefix + TEXT("_image_"), Images);
		GLTF::GenerateNames(Prefix + TEXT("_mesh_"), Meshes);

		// Validate FName criteria for Materials Textures Meshes Animations (assets)
		GLTF::ValidateFNameCriteria(TEXT("Material_"), Materials);
		GLTF::ValidateFNameCriteria(TEXT("Texture_"), Textures);
		GLTF::ValidateFNameCriteria(TEXT("Mesh_"), Meshes);
		GLTF::ValidateFNameCriteria(TEXT("Animation_"), Animations);
	}

	void FAsset::GetRootNodes(TArray<int32>& NodeIndices)
	{
		TMap<int32, uint32> VisitCount;
		VisitCount.Reserve(Nodes.Num());
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			for (int32 ChildIndex : Nodes[NodeIndex].Children)
			{
				uint32* Found = VisitCount.Find(ChildIndex);
				if (Found)
				{
					(*Found)++;
				}
				else
				{
					VisitCount.Add(ChildIndex) = 1;
				}
			}
		}

		NodeIndices.Empty();
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			if (!VisitCount.Contains(NodeIndex))
				NodeIndices.Add(NodeIndex);
			else
				check(VisitCount[NodeIndex] == 1);
		}
	}

	FAsset::EValidationCheck FAsset::ValidationCheck() const
	{
		int32 Res = EValidationCheck::Valid;
		if (Meshes.FindByPredicate([](const FMesh& Mesh) { return !Mesh.IsValid(); }))
			Res |= InvalidMeshPresent;

		if (Nodes.FindByPredicate([](const FNode& Node) { return !Node.Transform.IsValid(); }))
			Res |= InvalidNodeTransform;

		// TODO: lots more validation

		// TODO: verify if image format is PNG or JPEG

		return static_cast<EValidationCheck>(Res);
	}

	const TCHAR* ToString(EExtension Extension)
	{
		switch (Extension)
		{
			case GLTF::EExtension::KHR_MaterialsPbrSpecularGlossiness:
				return TEXT("KHR_Materials_PbrSpecularGlossiness");
			case GLTF::EExtension::KHR_MaterialsUnlit:
				return TEXT("KHR_Materials_Unlit");
			case GLTF::EExtension::KHR_MaterialsClearCoat:
				return TEXT("KHR_MaterialsClearCoat");
			case GLTF::EExtension::KHR_MaterialsSheen:
				return TEXT("KHR_MaterialsSheen");
			case GLTF::EExtension::KHR_MaterialsTransmission:
				return TEXT("KHR_MaterialsTransmission");
			case GLTF::EExtension::KHR_MaterialsSpecular:
				return TEXT("KHR_MaterialsSpecular");
			case GLTF::EExtension::KHR_MaterialsIOR:
				return TEXT("KHR_MaterialsIOR");
			case GLTF::EExtension::KHR_TextureTransform:
				return TEXT("KHR_Texture_Transform");
			case GLTF::EExtension::KHR_DracoMeshCompression:
				return TEXT("KHR_DracoMeshCompression");
			case GLTF::EExtension::KHR_LightsPunctual:
				return TEXT("KHR_LightsPunctual");
			case GLTF::EExtension::KHR_Blend:
				return TEXT("KHR_Blend");
			case GLTF::EExtension::MSFT_TextureDDS:
				return TEXT("MSFT_Texture_DDS");
			case GLTF::EExtension::MSFT_PackingNormalRoughnessMetallic:
				return TEXT("MSFT_Packing_NormalRoughnessMetallic");
			case GLTF::EExtension::MSFT_PackingOcclusionRoughnessMetallic:
				return TEXT("MSFT_Packing_OcclusionRoughnessMetallic");
			case GLTF::EExtension::Count:
				check(false);
			default:
				return TEXT("UnknwonExtension");
		}
	}

}  // namespace GLTF
