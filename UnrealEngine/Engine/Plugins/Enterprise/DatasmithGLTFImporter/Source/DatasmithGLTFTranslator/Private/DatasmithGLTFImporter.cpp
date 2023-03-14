// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithGLTFImporter.h"

#include "DatasmithGLTFAnimationImporter.h"
#include "DatasmithGLTFImportOptions.h"
#include "DatasmithGLTFMaterialElement.h"
#include "DatasmithGLTFTextureFactory.h"

#include "GLTFAsset.h"
#include "GLTFMaterialFactory.h"
#include "GLTFReader.h"
#include "GLTFMesh.h"
#include "GLTFMeshFactory.h"

#include "DatasmithAssetImportData.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "IDatasmithSceneElements.h"
#include "ObjectTemplates/DatasmithStaticMeshTemplate.h"
#include "Utility/DatasmithMeshHelper.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY(LogDatasmithGLTFImport);

#define LOCTEXT_NAMESPACE "DatasmithGLTFImporter"

class FGLTFMaterialElementFactory : public GLTF::IMaterialElementFactory
{
public:
	IDatasmithScene* CurrentScene;

public:
	FGLTFMaterialElementFactory()
	    : CurrentScene(nullptr)
	{
	}

	virtual GLTF::FMaterialElement* CreateMaterial(const TCHAR* Name, UObject* ParentPackage, EObjectFlags Flags) override
	{
		check(CurrentScene);

		TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement = FDatasmithSceneFactory::CreateUEPbrMaterial(Name);
		CurrentScene->AddMaterial(MaterialElement);
		return new FDatasmithGLTFMaterialElement(MaterialElement);
	}
};

FDatasmithGLTFImporter::FDatasmithGLTFImporter(TSharedRef<IDatasmithScene>& OutScene, UDatasmithGLTFImportOptions* InOptions)
    : DatasmithScene(OutScene)
	, GLTFReader(new GLTF::FFileReader())
    , GLTFAsset(new GLTF::FAsset())
    , MaterialFactory(new GLTF::FMaterialFactory(new FGLTFMaterialElementFactory(), new FDatasmithGLTFTextureFactory()))
    , AnimationImporter(new FDatasmithGLTFAnimationImporter(LogMessages))
{
	if (InOptions)
	{
		bGenerateLightmapUVs = InOptions->bGenerateLightmapUVs;
		ImportScale = InOptions->ImportScale;
		bAnimationFrameRateFromFile = InOptions->bAnimationFrameRateFromFile;
	}
}

FDatasmithGLTFImporter::~FDatasmithGLTFImporter() {}

void FDatasmithGLTFImporter::SetImportOptions(UDatasmithGLTFImportOptions* InOptions)
{
	if (InOptions)
	{
		bGenerateLightmapUVs = InOptions->bGenerateLightmapUVs;
		ImportScale = InOptions->ImportScale;
		bAnimationFrameRateFromFile = InOptions->bAnimationFrameRateFromFile;
	}
}

const TArray<GLTF::FLogMessage>& FDatasmithGLTFImporter::GetLogMessages() const
{
	LogMessages.Append(GLTFReader->GetLogMessages());
	return LogMessages;
}

bool FDatasmithGLTFImporter::OpenFile(const FString& InFileName)
{
	LogMessages.Empty();

	GLTFReader->ReadFile(InFileName, false, true, *GLTFAsset);
	const GLTF::FLogMessage* Found = GLTFReader->GetLogMessages().FindByPredicate(
	    [](const GLTF::FLogMessage& Message) { return Message.Get<0>() == GLTF::EMessageSeverity::Error; });
	if (Found)
	{
		return false;
	}
	check(GLTFAsset->ValidationCheck() == GLTF::FAsset::Valid);

	GLTFAsset->GenerateNames(FPaths::GetBaseFilename(InFileName));

	// check extensions supported
	for (GLTF::EExtension Extension : GLTFAsset->ExtensionsUsed)
	{
		if (!GLTF::FAsset::SupportedExtensions.Contains(Extension))
		{
			LogMessages.Emplace(GLTF::EMessageSeverity::Warning, FString::Printf(TEXT("Extension is not supported: %s"), GLTF::ToString(Extension)));
		}
	}

	return true;
}

TSharedPtr<IDatasmithActorElement> FDatasmithGLTFImporter::CreateCameraActor(int32 CameraIndex) const
{
	const GLTF::FCamera& Camera = GLTFAsset->Cameras[CameraIndex];

	TSharedRef<IDatasmithCameraActorElement> CameraElement = FDatasmithSceneFactory::CreateCameraActor(*Camera.Name);

	float       AspectRatio;
	float       FocalLength;
	const float SensorWidth = 36.f;  // mm
	CameraElement->SetSensorWidth(SensorWidth);
	if (Camera.bIsPerspective)
	{
		AspectRatio = Camera.Perspective.AspectRatio;
		FocalLength = (SensorWidth / Camera.Perspective.AspectRatio) / (2.0 * tan(Camera.Perspective.Fov / 2.0));
	}
	else
	{
		AspectRatio = Camera.Orthographic.XMagnification / Camera.Orthographic.YMagnification;
		FocalLength = (SensorWidth / AspectRatio) / (AspectRatio * tan(AspectRatio / 4.0));  // can only approximate Fov
	}

	CameraElement->SetSensorAspectRatio(AspectRatio);
	CameraElement->SetFocalLength(FocalLength);
	CameraElement->SetEnableDepthOfField(false);
	// ignore znear and zfar

	return CameraElement;
}

TSharedPtr<IDatasmithActorElement> FDatasmithGLTFImporter::CreateLightActor(int32 LightIndex) const
{
	const GLTF::FLight& Light = GLTFAsset->Lights[LightIndex];

	TSharedPtr<IDatasmithLightActorElement> LightElement;

	switch (Light.Type)
	{
		case GLTF::FLight::EType::Point:
		{
			TSharedRef<IDatasmithPointLightElement> Point = FDatasmithSceneFactory::CreatePointLight(*Light.Name);
			//  NOTE. spot light should also have these
			Point->SetIntensityUnits(EDatasmithLightUnits::Candelas);
			check(Light.Range > 0.f);
			if (Light.Range)
			{
				Point->SetAttenuationRadius(Light.Range * ImportScale);
			}
			LightElement = Point;
		}
		break;
		case GLTF::FLight::EType::Spot:
		{
			TSharedRef<IDatasmithSpotLightElement> Spot = FDatasmithSceneFactory::CreateSpotLight(*Light.Name);
			Spot->SetIntensityUnits(EDatasmithLightUnits::Candelas);
			Spot->SetInnerConeAngle(FMath::RadiansToDegrees(Light.Spot.InnerConeAngle));
			Spot->SetOuterConeAngle(FMath::RadiansToDegrees(Light.Spot.OuterConeAngle));
			LightElement = Spot;
		}
		break;
		case GLTF::FLight::EType::Directional:
		{
			TSharedRef<IDatasmithDirectionalLightElement> Directional = FDatasmithSceneFactory::CreateDirectionalLight(*Light.Name);
			LightElement                                              = Directional;
		}
		break;
		default:
			check(false);
			break;
	}

	// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md
	// "Brightness of light in.The units that this is defined in depend on the type of light.point and spot lights use luminous intensity in candela(lm / sr) while directional lights use illuminance in lux(lm / m2)"
	LightElement->SetIntensity(Light.Intensity);
	LightElement->SetColor(FLinearColor(Light.Color));

	return LightElement;
}

TSharedPtr<IDatasmithMeshActorElement> FDatasmithGLTFImporter::CreateStaticMeshActor(int32 MeshIndex)
{
	TSharedPtr<IDatasmithMeshActorElement> MeshActorElement;

	if (!ImportedMeshes.Contains(MeshIndex))
	{
		ImportedMeshes.Add(MeshIndex);

		const GLTF::FMesh& Mesh = GLTFAsset->Meshes[MeshIndex];

		TSharedRef<IDatasmithMeshElement> MeshElement = FDatasmithSceneFactory::CreateMesh(*Mesh.Name);

		FMD5 MD5;

		FMD5Hash MeshHash = Mesh.GetHash();
		MD5.Update(MeshHash.GetBytes(), MeshHash.GetSize());

		const TArray<GLTF::FMaterialElement*>& Materials = MaterialFactory->GetMaterials();
		for (int32 MaterialID = 0; MaterialID < Materials.Num(); ++MaterialID)
		{
			GLTF::FMaterialElement* Material = Materials[MaterialID];
			MeshElement->SetMaterial(*Material->GetName(), MaterialID);

			FMD5Hash MaterialHash = Material->GetGLTFMaterialHash();
			MD5.Update(MaterialHash.GetBytes(), MaterialHash.GetSize());
		}

		if (bGenerateLightmapUVs)
		{
			MeshElement->SetLightmapSourceUV(0);
			MeshElement->SetLightmapCoordinateIndex(-1);
		}
		else
		{
			MeshElement->SetLightmapCoordinateIndex(0);
		}

		uint8 GenerateLightmapUVs = static_cast<uint8>(bGenerateLightmapUVs);
		MD5.Update(&GenerateLightmapUVs, 1);

		FMD5Hash Result;
		Result.Set(MD5);
		MeshElement->SetFileHash(Result);

		MeshElementToGLTFMeshIndex.Add(&MeshElement.Get(), MeshIndex);
		GLTFMeshIndexToMeshElement.Add(MeshIndex, MeshElement);
		DatasmithScene->AddMesh(MeshElement);
	}

	MeshActorElement = FDatasmithSceneFactory::CreateMeshActor(TEXT("TempName"));
	MeshActorElement->SetStaticMeshPathName(*FDatasmithUtils::SanitizeObjectName(GLTFAsset->Meshes[MeshIndex].Name));

	return MeshActorElement;
}

void FDatasmithGLTFImporter::CreateMaterialVariants(TSharedPtr<IDatasmithMeshActorElement> MeshActorElement, int32 MeshIndex)
{
	for (const GLTF::FPrimitive& Primitive : GLTFAsset->Meshes[MeshIndex].Primitives)
	{
		for (const GLTF::FVariantMapping& VariantMapping : Primitive.VariantMappings)
		{
			for (int32 VariantIndex : VariantMapping.VariantIndices)
			{
				if (!VariantSets.IsValid())
				{
					VariantSets = FDatasmithSceneFactory::CreateLevelVariantSets(TEXT("LevelVariantSets"));
					VariantSet = FDatasmithSceneFactory::CreateVariantSet(TEXT("VariantSet"));
					VariantSets->AddVariantSet(VariantSet.ToSharedRef());
				}

				ensure(GLTFAsset->Variants.IsValidIndex(VariantIndex));
				const FString& VariantName = GLTFAsset->Variants[VariantIndex];

				TSharedPtr<IDatasmithVariantElement> Variant;

				if (!VariantNameToVariantElement.Contains(VariantName))
				{
					Variant = FDatasmithSceneFactory::CreateVariant(*VariantName);
					VariantNameToVariantElement.Add(VariantName, Variant.ToSharedRef());
					VariantSet->AddVariant(Variant.ToSharedRef());
				}
				else
				{
					Variant = VariantNameToVariantElement[VariantName];
				}

				const TArray<GLTF::FMaterialElement*>& Materials = MaterialFactory->GetMaterials();

				FDatasmithGLTFMaterialElement* Material = static_cast<FDatasmithGLTFMaterialElement*>(Materials[VariantMapping.MaterialIndex]);
				check(Material);

				TSharedRef<IDatasmithObjectPropertyCaptureElement> PropCapture = FDatasmithSceneFactory::CreateObjectPropertyCapture();
				PropCapture->SetCategory(EDatasmithPropertyCategory::Material);
				PropCapture->SetRecordedObject(Material->GetMaterial());

				// Assign mesh material to variant
				TSharedRef<IDatasmithActorBindingElement> Binding = FDatasmithSceneFactory::CreateActorBinding();
				Binding->SetActor(MeshActorElement);
				Binding->AddPropertyCapture(PropCapture);
			
				Variant->AddActorBinding(Binding);
			}
		}
	}
}

TSharedPtr<IDatasmithActorElement> FDatasmithGLTFImporter::ConvertNode(int32 NodeIndex)
{
	TSharedPtr<IDatasmithActorElement> ActorElement;

	GLTF::FNode& Node = GLTFAsset->Nodes[NodeIndex];
	check(!Node.Name.IsEmpty());

	FTransform Transform = Node.Transform;

	switch (Node.Type)
	{
		case GLTF::FNode::EType::Mesh:
		case GLTF::FNode::EType::MeshSkinned:
			if (GLTFAsset->Meshes.IsValidIndex(Node.MeshIndex))
			{
				TSharedPtr<IDatasmithMeshActorElement> MeshActorElement = CreateStaticMeshActor(Node.MeshIndex);

				ActorElement = MeshActorElement;
				ActorElement->SetName(*Node.Name);

				CreateMaterialVariants(MeshActorElement, Node.MeshIndex);
			}
			break;
		case GLTF::FNode::EType::Camera:
			ActorElement = CreateCameraActor(Node.CameraIndex);
			// fix GLTF camera orientation
			// https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#cameras
			Transform.ConcatenateRotation(FRotator(0, -90, 0).Quaternion());
			ActorElement->SetName(*Node.Name);
			break;
		case GLTF::FNode::EType::Light:
			ActorElement = CreateLightActor(Node.LightIndex);
			Transform.ConcatenateRotation(FRotator(0, -90, 0).Quaternion());
			ActorElement->SetName(*Node.Name);
			break;
		case GLTF::FNode::EType::Transform:
		case GLTF::FNode::EType::Joint:
		default:
			// Create regular actor
			ActorElement = FDatasmithSceneFactory::CreateActor(*Node.Name);
			break;
	}

	if (!ActorElement)
	{
		return ActorElement;
	}

	FString NodeOriginalName = Node.Name.RightChop(Node.Name.Find(TEXT("_")) + 1);
	ActorElement->AddTag(*NodeOriginalName);
	ActorElement->SetLabel(*NodeOriginalName);

	SetActorElementTransform(ActorElement, Transform);

	for (int32 Index = 0; Index < Node.Children.Num(); ++Index)
	{
		AddActorElementChild(ActorElement, ConvertNode(Node.Children[Index]));
	}

	return ActorElement;
}

bool FDatasmithGLTFImporter::SendSceneToDatasmith()
{
	if (GLTFAsset->ValidationCheck() != GLTF::FAsset::Valid)
	{
		return false;
	}

	// Setup importer
	AnimationImporter->SetUniformScale(ImportScale);

	FDatasmithGLTFTextureFactory& TextureFactory     = static_cast<FDatasmithGLTFTextureFactory&>(MaterialFactory->GetTextureFactory());
	FGLTFMaterialElementFactory&  ElementFactory     = static_cast<FGLTFMaterialElementFactory&>(MaterialFactory->GetMaterialElementFactory());
	TextureFactory.CurrentScene                      = &DatasmithScene.Get();
	ElementFactory.CurrentScene                      = &DatasmithScene.Get();
	const TArray<GLTF::FMaterialElement*>& Materials = MaterialFactory->CreateMaterials(*GLTFAsset, nullptr, EObjectFlags::RF_NoFlags);
	check(Materials.Num() == GLTFAsset->Materials.Num());

	// Perform conversion
	ImportedMeshes.Empty();

	TArray<int32> RootNodes;
	GLTFAsset->GetRootNodes(RootNodes);
	for (int32 RootIndex : RootNodes)
	{
		const GLTF::FNode& Node = GLTFAsset->Nodes[RootIndex];

		const TSharedPtr<IDatasmithActorElement> NodeActor = ConvertNode(RootIndex);
		if (NodeActor.IsValid())
		{
			DatasmithScene->AddActor(NodeActor);
		}
	}

	AnimationImporter->CurrentScene = &DatasmithScene.Get();
	AnimationImporter->CreateAnimations(*GLTFAsset, bAnimationFrameRateFromFile);

	if (VariantSets.IsValid())
	{
		DatasmithScene->AddLevelVariantSets(VariantSets);
	}

	return true;
}

void FDatasmithGLTFImporter::GetGeometriesForMeshElementAndRelease(const TSharedRef<IDatasmithMeshElement> MeshElement, TArray<FMeshDescription>& OutMeshDescriptions)
{
	if (int32* MeshIndexPtr = MeshElementToGLTFMeshIndex.Find(&MeshElement.Get()))
	{
		int32 MeshIndex = *MeshIndexPtr;

		FMeshDescription MeshDescription;
		DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

		GLTF::FMeshFactory LocalMeshFactory;
		LocalMeshFactory.SetUniformScale(ImportScale);

		LocalMeshFactory.FillMeshDescription(GLTFAsset->Meshes[MeshIndex], &MeshDescription);

		LogMessages.Append(LocalMeshFactory.GetLogMessages());
		LocalMeshFactory.CleanUp();

		OutMeshDescriptions.Add(MoveTemp(MeshDescription));
	}
}

const TArray<TSharedRef<IDatasmithLevelSequenceElement>>& FDatasmithGLTFImporter::GetImportedSequences()
{
	return AnimationImporter->GetImportedSequences();
}

void FDatasmithGLTFImporter::UnloadScene()
{
	MaterialFactory->CleanUp();
	GLTFAsset->Clear(8 * 1024, 512);

	GLTFReader.Reset(new GLTF::FFileReader());
	GLTFAsset.Reset(new GLTF::FAsset());
	MaterialFactory.Reset(new GLTF::FMaterialFactory(new FGLTFMaterialElementFactory(), new FDatasmithGLTFTextureFactory()));
}

void FDatasmithGLTFImporter::SetActorElementTransform(TSharedPtr<IDatasmithActorElement> ActorElement, const FTransform &Transform)
{
	if (Transform.GetRotation().IsNormalized())
	{
		ActorElement->SetRotation(Transform.GetRotation());
	}
	else
	{
		LogMessages.Emplace(GLTF::EMessageSeverity::Warning, FString::Printf(TEXT("Actor %s rotation is not normalized"), ActorElement->GetLabel()));
	}

	if (Transform.GetScale3D().IsNearlyZero())
	{
		FVector Scale = Transform.GetScale3D();
		LogMessages.Emplace(GLTF::EMessageSeverity::Warning, FString::Printf(TEXT("Actor %s scale(%f, %f, %f) is nearly zero"), ActorElement->GetLabel(), Scale.X, Scale.Y, Scale.Z));
	}
	ActorElement->SetScale(Transform.GetScale3D());

	ActorElement->SetTranslation(Transform.GetTranslation() * ImportScale);
}

void FDatasmithGLTFImporter::AddActorElementChild(TSharedPtr<IDatasmithActorElement> ActorElement, const TSharedPtr<IDatasmithActorElement>& ChildNodeActor)
{
	if (ChildNodeActor.IsValid())
	{
		ActorElement->AddChild(ChildNodeActor, bTransformIsLocal ? EDatasmithActorAttachmentRule::KeepRelativeTransform : EDatasmithActorAttachmentRule::KeepWorldTransform);
	}
}

#undef LOCTEXT_NAMESPACE
