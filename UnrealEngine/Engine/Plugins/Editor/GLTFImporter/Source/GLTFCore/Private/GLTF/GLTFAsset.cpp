// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFAsset.h"

#include "Misc/Paths.h"

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

				if (Obj.Name.IsEmpty())
				{
					Obj.Name = Prefix + FString::FromInt(Index);
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
			const FString NodePrefix = Prefix + TEXT("_node_");
			const FString JointPrefix = Prefix + TEXT("_joint_");
			for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
			{
				FNode& Node = Nodes[NodeIndex];

				if (Node.Name.IsEmpty())
				{
					bool bIsJoint = Node.Type == FNode::EType::Joint;
					Node.Name     = (bIsJoint ? JointPrefix : NodePrefix);

					// Make sure node names are unique, in case Name is not empty it is presumed to be unique / handled by the processing pipelines.
					Node.Name = Node.Name + FString::FromInt(NodeIndex);
				}
			}
		}

		{
			const FString TexPrefix = Prefix + TEXT("_texture_");
			for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
			{
				FTexture& Tex = Textures[TextureIndex];

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
					Tex.Name = FPaths::GetBaseFilename(Tex.Source.URI);
				}
				else
				{
					// GLTF texture name has decorative purpose, not guaranteed to be unique
					// only its index is unique. Same with glTF image or its source file's basename
					// So always include texture index into texture's name to increase probability that names are unique
					Tex.Name = TexPrefix + FString::FromInt(TextureIndex);
				}

			}
		}

		{
			const FString CameraPrefix = Prefix + TEXT("_camera_");
			for (int32 CameraIndex = 0; CameraIndex < Cameras.Num(); ++CameraIndex)
			{
				FCamera& Camera = Cameras[CameraIndex];

				if (Camera.Name.IsEmpty())
				{
					if (!Camera.Node.Name.IsEmpty())
					{
						Camera.Name = Camera.Node.Name; // cant be empty
					}
					else
					{
						Camera.Name = CameraPrefix + FString::FromInt(CameraIndex);
					}
				}
			}
		}

		{
			const FString LightPrefix = Prefix + TEXT("_light_");
			for (int32 LightIndex = 0; LightIndex < Lights.Num(); ++LightIndex)
			{
				FLight& Light = Lights[LightIndex];

				if (Light.Name.IsEmpty())
				{
					if (Light.Node && !Light.Node->Name.IsEmpty())
					{
						Light.Name = Light.Node->Name;
					}
					else
					{
						Light.Name = LightPrefix + FString::FromInt(LightIndex);
					}
				}
			}
		}

		// General pattern here is that we'll have a Prefix that will be a filename (like MyFile). If the
		// GLTF object has an actual name, it will end up like <index>_<objectname>, like "0_shoe" or "3_spotlight".
		// If it doesn't have a name, it will end up with a name like <index>_<filename>_<objecttype>, e.g.
		// "0_MyFile_material" or "3_myfile_mesh".
		// We want to do images after textures as the textures may want to reuse source image names, but we don't
		// want to use those if we generated them ourselves.
		GLTF::GenerateNames(Prefix + TEXT("_material_"), Materials);
		GLTF::GenerateNames(Prefix + TEXT("_skin_"), Skins);
		GLTF::GenerateNames(Prefix + TEXT("_animation_"), Animations);
		GLTF::GenerateNames(Prefix + TEXT("_image_"), Images);
		GLTF::GenerateNames(Prefix + TEXT("_mesh_"), Meshes);
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
