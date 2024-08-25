// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/UnrealBakeHelpers.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuT/UnrealPixelFormatOverride.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialExpressionTextureBase.h"

#include "TextureResource.h"

UObject* FUnrealBakeHelpers::BakeHelper_DuplicateAsset(UObject* Object, const FString& ObjName, const FString& PkgName, bool ResetDuplicatedFlags, 
													   TMap<UObject*, UObject*>& ReplacementMap, bool OverwritePackage, 
													   const bool bGenerateConstantMaterialInstances)
{
	FString FinalObjName = ObjName;
	FString FinalPkgName = PkgName;

	if (!OverwritePackage)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(PkgName, "", FinalPkgName, FinalObjName);
	}

	UPackage* Package = CreatePackage(*FinalPkgName);
	Package->FullyLoad();

	FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(Object, Package, *FinalObjName, RF_AllFlags, nullptr, EDuplicateMode::Normal);

	UObject* DupObject = nullptr;
	UMaterialInterface* MatInterface = Cast<UMaterialInterface>(Object);
	// Only generate constant material instances if the original material is actually an instance, so check it here. 
	// Otherwise just duplicate
	UMaterialInstance* MatInstance = Cast<UMaterialInstance>(Object);
	
	if (bGenerateConstantMaterialInstances && MatInterface && MatInstance)
	{
		UMaterialInterface* ParentInterface = MatInterface;

		UMaterialInstanceDynamic* InstanceDynamic = Cast<UMaterialInstanceDynamic>(Object);

		if (InstanceDynamic)
		{
			ParentInterface = InstanceDynamic->Parent;
		}
		else
		{
			UMaterialInstanceConstant* InstanceConstant = Cast<UMaterialInstanceConstant>(Object);

			if (InstanceConstant)
			{
				ParentInterface = InstanceConstant->Parent;
			}
		}

		UMaterialInstanceConstantFactoryNew* MaterialFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
		MaterialFactory->InitialParent = ParentInterface;
		FString MaterialInstanceName = FinalObjName;
		DupObject = (UMaterialInstanceConstant*)MaterialFactory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(),
			Package, FName(MaterialInstanceName), RF_NoFlags, NULL, GWarn);
		ensure(DupObject);

		TMap<int, UTexture*> EmptyTextureReplacementMap;
		FUnrealBakeHelpers::CopyAllMaterialParameters(DupObject, MatInterface, EmptyTextureReplacementMap);
	}
	else
	{
		DupObject = StaticDuplicateObjectEx(Params);
	}

	if (DupObject)
	{
		DupObject->SetFlags(RF_Public | RF_Standalone);
		DupObject->ClearFlags(RF_Transient);

		// The garbage collector is called in the middle of the bake process, and this can destroy this temporary objects. 
		// We add them to the garbage root to prevent this. This will avoid them being unloaded while the editor is running, but this
		// action is not used often.
		DupObject->AddToRoot();
		DupObject->MarkPackageDirty();

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(DupObject);

		// Replace all references
		ReplacementMap.Add(Object, DupObject);

		constexpr EArchiveReplaceObjectFlags ReplaceFlags = (EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
		FArchiveReplaceObjectRef<UObject> ReplaceAr(DupObject, ReplacementMap, ReplaceFlags);
	}

	return DupObject;
}


namespace
{

	void CopyTextureProperties(UTexture2D* Texture, const UTexture2D* SourceTexture)
	{
		MUTABLE_CPUPROFILER_SCOPE(CopyTextureProperties)

			Texture->NeverStream = SourceTexture->NeverStream;

		Texture->SRGB = SourceTexture->SRGB;
		Texture->Filter = SourceTexture->Filter;
		Texture->LODBias = SourceTexture->LODBias;

		Texture->MipGenSettings = SourceTexture->MipGenSettings;
		Texture->CompressionNone = SourceTexture->CompressionNone;

		Texture->LODGroup = SourceTexture->LODGroup;
		Texture->AddressX = SourceTexture->AddressX;
		Texture->AddressY = SourceTexture->AddressY;
	}
}


UTexture2D* FUnrealBakeHelpers::BakeHelper_CreateAssetTexture(UTexture2D* SrcTex, const FString& TexObjName, const FString& TexPkgName, const UTexture* OrgTex, bool ResetDuplicatedFlags, TMap<UObject*, UObject*>& ReplacementMap, bool OverwritePackage)
{
	const bool bIsMutableTexture = !SrcTex->Source.IsValid();
	if (!bIsMutableTexture)
	{
		return Cast<UTexture2D>(BakeHelper_DuplicateAsset(SrcTex, TexObjName, TexPkgName, ResetDuplicatedFlags, ReplacementMap, OverwritePackage, false));
	}

	int32 sx = SrcTex->GetPlatformData()->SizeX;
	int32 sy = SrcTex->GetPlatformData()->SizeY;
	EPixelFormat SrcPixelFormat = SrcTex->GetPlatformData()->PixelFormat;
	ETextureSourceFormat PixelFormat = (SrcPixelFormat == PF_BC4 || SrcPixelFormat == PF_G8) ? TSF_G8 : TSF_BGRA8;

	// Begin duplicate texture
	FString FinalObjName = TexObjName;
	FString FinalPkgName = TexPkgName;

	if (!OverwritePackage)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(TexPkgName, "", FinalPkgName, FinalObjName);
	}

	UPackage* Package = CreatePackage(*FinalPkgName);
	Package->FullyLoad();

	UTexture2D* DupTex = NewObject<UTexture2D>(Package, *FinalObjName, RF_Public | RF_Standalone);

	ReplacementMap.Add(SrcTex, DupTex);

	// The garbage collector is called in the middle of the bake process, and this can destroy this temporary object. 
	// We add them to the garbage root to prevent this. This will avoid them being unloaded while the editor is running, but this
	// action is not used often.
	DupTex->AddToRoot();
	DupTex->MarkPackageDirty();

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(DupTex);

	// Replace all references
	constexpr EArchiveReplaceObjectFlags ReplaceFlags = (EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
	FArchiveReplaceObjectRef<UObject> ReplaceAr(DupTex, ReplacementMap, ReplaceFlags);

	// End duplicate texture

	CopyTextureProperties(DupTex, SrcTex);

	DupTex->RemoveUserDataOfClass(UMutableTextureMipDataProviderFactory::StaticClass());
	if (OrgTex)
	{
		DupTex->CompressionSettings = OrgTex->CompressionSettings;
	}

	// Mutable textures only have platform data. We need to build the source data for them to be assets.
	DupTex->Source.Init(sx, sy, 1, 1, PixelFormat);

	int32 MipCount = SrcTex->GetPlatformData()->Mips.Num();
	if (!MipCount)
	{
		UE_LOG(LogMutable, Warning, TEXT("Bake Instances: Empty texture found [%s]."), *SrcTex->GetName());
		return DupTex;
	}

	// Create a mutable image from the platform data.
	mu::EImageFormat MutableFormat = UnrealToMutablePixelFormat(SrcTex->GetPlatformData()->PixelFormat, SrcTex->HasAlphaChannel());
	mu::Ptr<mu::Image> PlatformImage = new mu::Image(sx, sy, 1, MutableFormat, mu::EInitializationType::NotInitialized);

	constexpr int32 MipIndex = 0;
	const uint8* SourceData = reinterpret_cast<const uint8*>(SrcTex->GetPlatformData()->Mips[MipIndex].BulkData.LockReadOnly());
	check(SourceData); // A mutable-generated texture should always contain platform data
	int32 PlatformDataSize = SrcTex->GetPlatformData()->Mips[MipIndex].BulkData.GetBulkDataSize();
	check(PlatformImage->GetDataSize()== PlatformDataSize);
	FMemory::Memcpy(PlatformImage->GetLODData(0), SourceData, PlatformDataSize);
	SourceData = nullptr;
	SrcTex->GetPlatformData()->Mips[MipIndex].BulkData.Unlock();

	// Reformat the mutable image
	mu::EImageFormat UncompressedMutableFormat = mu::EImageFormat::IF_RGBA_UBYTE;
	switch (SrcTex->GetPlatformData()->PixelFormat)
	{
	case PF_G8:
	case PF_L8:
	case PF_A8:
	case PF_BC4:
		UncompressedMutableFormat = mu::EImageFormat::IF_L_UBYTE;
		break;

	default:
		break;
	}

	mu::FImageOperator ImOp = mu::FImageOperator::GetDefault(mu::FImageOperator::FImagePixelFormatFunc());
	int32 Quality = 4; // Doesn't matter for decompression.
	mu::Ptr<mu::Image> UncompressedImage = ImOp.ImagePixelFormat(Quality, PlatformImage.get(), UncompressedMutableFormat);

	// Copy the decompressed data to the texture source data
	int32 SourceDataSize = DupTex->Source.CalcMipSize(MipIndex);

	TArrayView<uint8> UncompressedView = UncompressedImage->DataStorage.GetLOD(0);
	
	// If this doesn't match, more cases have to be added to the switch above.
	check(UncompressedView.Num() == SourceDataSize);

	uint8* Dest = DupTex->Source.LockMip(MipIndex);
	check(Dest);
	FMemory::Memcpy(Dest, UncompressedView.GetData(), SourceDataSize);

	// Probably can be integrated in the pixel format
	const bool bNeedsRBSwizzle = PixelFormat == TSF_BGRA8;
	if (bNeedsRBSwizzle)
	{
		for (int32 x = 0; x < sx * sy; ++x)
		{
			uint8 temp = Dest[0];
			Dest[0] = Dest[2];
			Dest[2] = temp;
			Dest += 4;
		}
	}

	Dest = nullptr;
	DupTex->Source.UnlockMip(MipIndex);

	bool bNeeds_TC_Grayscale = PixelFormat == TSF_G8 || PixelFormat == TSF_G16;
	bool bDoNotCompress = SrcPixelFormat == PF_R8G8B8A8;
	bool bIsNormalMap = SrcPixelFormat == PF_BC5;

	if (bNeeds_TC_Grayscale || bDoNotCompress || bIsNormalMap)
	{
		FTextureFormatSettings Settings;

		Settings.SRGB = SrcTex->SRGB;

		if (bNeeds_TC_Grayscale)
		{
			// If compression settings are not set to TC_Grayscale the texture will get a DXT format
			// instead of G8 or G16.
			Settings.CompressionSettings = TC_Grayscale;
			DupTex->CompressionSettings = TC_Grayscale;
		}

		if (bDoNotCompress)
		{
			// In this case keep the RGBA format instead of compressing to DXT
			Settings.CompressionNone = true;
			DupTex->CompressionNone = true;
		}

		if (bIsNormalMap)
		{
			Settings.CompressionSettings = TC_Normalmap;
			DupTex->CompressionSettings = TC_Normalmap;
		}
		
		DupTex->SetLayerFormatSettings(0, Settings);
	}

	DupTex->UpdateResource();

	return DupTex;
}


void FUnrealBakeHelpers::CopyAllMaterialParameters(UObject* DestMaterial, UMaterialInterface* OriginMaterial, const TMap<int, UTexture*>& TextureReplacementMap)
{
	UMaterial* Material = OriginMaterial ? OriginMaterial->GetMaterial() : nullptr;
	UMaterialInterface* DupMaterialInterface = Cast<UMaterialInterface>(DestMaterial);
	UMaterial* DupMaterial = DupMaterialInterface ? DupMaterialInterface->GetMaterial() : nullptr;
	UMaterialInstanceConstant* DupMaterialInstanceConstant = Cast<UMaterialInstanceConstant>(DestMaterial);

	if (Material && DupMaterial)
	{
		TArray<FMaterialParameterInfo> parametersInfo;
		TArray<FGuid> parametersGuids;

		// copy scalar parameters
		TArray<FMaterialParameterInfo> ScalarParameterInfoArray;
		TArray<FGuid> GuidArray;
		OriginMaterial->GetAllScalarParameterInfo(ScalarParameterInfoArray, GuidArray);
		for (const FMaterialParameterInfo& Param : ScalarParameterInfoArray)
		{
			float Value = 0.f;
			if (OriginMaterial->GetScalarParameterValue(Param, Value))
			{
				if (DupMaterialInstanceConstant)
				{
					DupMaterialInstanceConstant->SetScalarParameterValueEditorOnly(Param.Name, Value);
				}
				else
				{
					DupMaterial->SetScalarParameterValueEditorOnly(Param.Name, Value);
				}
			}
		}

		// copy vector parameters
		TArray<FMaterialParameterInfo> VectorParameterInfoArray;
		OriginMaterial->GetAllVectorParameterInfo(VectorParameterInfoArray, GuidArray);
		for (const FMaterialParameterInfo& Param : VectorParameterInfoArray)
		{
			FLinearColor Value;
			if (OriginMaterial->GetVectorParameterValue(Param, Value))
			{
				if (DupMaterialInstanceConstant)
				{
					DupMaterialInstanceConstant->SetVectorParameterValueEditorOnly(Param.Name, Value);
				}
				else
				{
					DupMaterial->SetVectorParameterValueEditorOnly(Param.Name, Value);
				}
			}
		}

		// copy switch parameters								
		TArray<FMaterialParameterInfo> StaticSwitchParameterInfoArray;
		OriginMaterial->GetAllStaticSwitchParameterInfo(StaticSwitchParameterInfoArray, GuidArray);
		for (int i = 0; i < StaticSwitchParameterInfoArray.Num(); ++i)
		{
			bool Value = false;
			if (OriginMaterial->GetStaticSwitchParameterValue(StaticSwitchParameterInfoArray[i].Name, Value, GuidArray[i]))
			{
				DupMaterial->SetStaticSwitchParameterValueEditorOnly(StaticSwitchParameterInfoArray[i].Name, Value, GuidArray[i]);
			}
		}

		// Replace Textures
		TArray<FName> ParameterNames = GetTextureParameterNames(Material);
		for (const TPair<int, UTexture*>& it : TextureReplacementMap)
		{
			if (ParameterNames.IsValidIndex(it.Key))
			{
				DupMaterial->SetTextureParameterValueEditorOnly(ParameterNames[it.Key], it.Value);
			}
		}

		// Fix potential errors compiling materials due to Sampler Types
		for (const TObjectPtr<UMaterialExpression>& Expression : DupMaterial->GetExpressions())
		{
			if (UMaterialExpressionTextureBase* MatExpressionTexBase = Cast<UMaterialExpressionTextureBase>(Expression))
			{
				MatExpressionTexBase->AutoSetSampleType();
			}
		}

		DestMaterial->PreEditChange(NULL);
		DestMaterial->PostEditChange();
	}
	else
	{
		ensure(false);
	}
}


TArray<FName> FUnrealBakeHelpers::GetTextureParameterNames(UMaterial* Material)
{
	TArray<FGuid> Guids;
	TArray<FName> ParameterNames;

	TArray<FMaterialParameterInfo> OutParameterInfo;
	Material->GetAllTextureParameterInfo(OutParameterInfo, Guids);

	const int32 MaxIndex = OutParameterInfo.Num();
	ParameterNames.SetNum(MaxIndex);

	for (int32 i = 0; i < MaxIndex; i++)
	{
		ParameterNames[i] = OutParameterInfo[i].Name;
	}

	return ParameterNames;
}
