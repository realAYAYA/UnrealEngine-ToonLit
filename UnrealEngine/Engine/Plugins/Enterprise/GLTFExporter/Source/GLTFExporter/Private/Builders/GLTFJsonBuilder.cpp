// Copyright Epic Games, Inc. All Rights Reserved.

#include "Builders/GLTFJsonBuilder.h"
#include "GLTFExporterModule.h"
#include "Interfaces/IPluginManager.h"
#include "Runtime/Launch/Resources/Version.h"

FGLTFJsonBuilder::FGLTFJsonBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions)
	: FGLTFFileBuilder(FileName, ExportOptions)
	, DefaultScene(JsonRoot.DefaultScene)
{
	JsonRoot.Asset.Generator = GetGeneratorString();
}

void FGLTFJsonBuilder::WriteJsonArchive(FArchive& Archive)
{
	JsonRoot.WriteJson(Archive, !bIsGLB, ExportOptions->bSkipNearDefaultValues ? KINDA_SMALL_NUMBER : 0);
}

TSet<EGLTFJsonExtension> FGLTFJsonBuilder::GetCustomExtensionsUsed() const
{
	TSet<EGLTFJsonExtension> CustomExtensions;

	for (EGLTFJsonExtension Extension : JsonRoot.Extensions.Used)
	{
		if (IsCustomExtension(Extension))
		{
			CustomExtensions.Add(Extension);
		}
	}

	return CustomExtensions;
}

void FGLTFJsonBuilder::AddExtension(EGLTFJsonExtension Extension, bool bIsRequired)
{
	JsonRoot.Extensions.Used.Add(Extension);
	if (bIsRequired)
	{
		JsonRoot.Extensions.Required.Add(Extension);
	}
}

FGLTFJsonAccessor* FGLTFJsonBuilder::AddAccessor()
{
	return JsonRoot.Accessors.Add();
}

FGLTFJsonAnimation* FGLTFJsonBuilder::AddAnimation()
{
	return JsonRoot.Animations.Add();
}

FGLTFJsonBuffer* FGLTFJsonBuilder::AddBuffer()
{
	return JsonRoot.Buffers.Add();
}

FGLTFJsonBufferView* FGLTFJsonBuilder::AddBufferView()
{
	return JsonRoot.BufferViews.Add();
}

FGLTFJsonCamera* FGLTFJsonBuilder::AddCamera()
{
	return JsonRoot.Cameras.Add();
}

FGLTFJsonImage* FGLTFJsonBuilder::AddImage()
{
	return JsonRoot.Images.Add();
}

FGLTFJsonMaterial* FGLTFJsonBuilder::AddMaterial()
{
	return JsonRoot.Materials.Add();
}

FGLTFJsonMesh* FGLTFJsonBuilder::AddMesh()
{
	return JsonRoot.Meshes.Add();
}

FGLTFJsonNode* FGLTFJsonBuilder::AddNode()
{
	return JsonRoot.Nodes.Add();
}

FGLTFJsonSampler* FGLTFJsonBuilder::AddSampler()
{
	return JsonRoot.Samplers.Add();
}

FGLTFJsonScene* FGLTFJsonBuilder::AddScene()
{
	return JsonRoot.Scenes.Add();
}

FGLTFJsonSkin* FGLTFJsonBuilder::AddSkin()
{
	return JsonRoot.Skins.Add();
}

FGLTFJsonTexture* FGLTFJsonBuilder::AddTexture()
{
	return JsonRoot.Textures.Add();
}

FGLTFJsonBackdrop* FGLTFJsonBuilder::AddBackdrop()
{
	return JsonRoot.Backdrops.Add();
}

FGLTFJsonLight* FGLTFJsonBuilder::AddLight()
{
	return JsonRoot.Lights.Add();
}

FGLTFJsonLightMap* FGLTFJsonBuilder::AddLightMap()
{
	return JsonRoot.LightMaps.Add();
}

FGLTFJsonSkySphere* FGLTFJsonBuilder::AddSkySphere()
{
	return JsonRoot.SkySpheres.Add();
}

FGLTFJsonEpicLevelVariantSets* FGLTFJsonBuilder::AddEpicLevelVariantSets()
{
	return JsonRoot.EpicLevelVariantSets.Add();
}

FGLTFJsonKhrMaterialVariant* FGLTFJsonBuilder::AddKhrMaterialVariant()
{
	return JsonRoot.KhrMaterialVariants.Add();
}

const FGLTFJsonRoot& FGLTFJsonBuilder::GetRoot() const
{
	return JsonRoot;
}

FString FGLTFJsonBuilder::GetGeneratorString() const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(GLTFEXPORTER_MODULE_NAME);
	const FPluginDescriptor& PluginDescriptor = Plugin->GetDescriptor();

	return ExportOptions->bIncludeGeneratorVersion
		? TEXT(EPIC_PRODUCT_NAME) TEXT(" ") ENGINE_VERSION_STRING TEXT(" ") + PluginDescriptor.FriendlyName + TEXT(" ") + PluginDescriptor.VersionName
		: TEXT(EPIC_PRODUCT_NAME) TEXT(" ") + PluginDescriptor.FriendlyName;
}

bool FGLTFJsonBuilder::IsCustomExtension(EGLTFJsonExtension Extension)
{
	const TCHAR CustomPrefix[] = TEXT("EPIC_");

	const TCHAR* ExtensionString = FGLTFJsonUtilities::GetValue(Extension);
	return FCString::Strncmp(ExtensionString, CustomPrefix, GetNum(CustomPrefix) - 1) == 0;
}
