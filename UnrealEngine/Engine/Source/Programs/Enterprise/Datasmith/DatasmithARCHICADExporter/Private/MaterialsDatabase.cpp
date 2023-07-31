// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialsDatabase.h"
#include "Utils/AutoChangeDatabase.h"

#include "ModelMaterial.hpp"
#include "Texture.hpp"
#include "AttributeIndex.hpp"

#include "Synchronizer.h"
#include "TexturesCache.h"

#include "DatasmithSceneFactory.h"

BEGIN_NAMESPACE_UE_AC

class FMaterialInfo
{
  public:
	FMaterialInfo(FMaterialsDatabase::FMaterialSyncData* IOMaterialSyncData)
		: SyncData(*IOMaterialSyncData)
	{
	}

	void Init(const FSyncContext& SyncContext, const API_Component3D& CUmat, const FMaterialKey& MaterialKey);

	void UpdateMaterial(const TSharedPtr< IDatasmithUEPbrMaterialElement >& DSMaterial);

	bool	bIdIsSynthetized = false;
	FString DatasmithId;
	FString DatasmithLabel;

	double Rotation = 0.0; // Rotation angle
	double InvXSize = 1.0; // Used to compute uv
	double InvYSize = 1.0; // Used to compute uv
	bool   bMirrorX = false;
	bool   bMirrorY = false;

	bool			  bTwoSide = false;
	float			  Opacity = 0.0f;
	float			  SpecularReflection = 0.0f;
	ModelerAPI::Color SurfaceColor;
	ModelerAPI::Color EmissiveColor;

	const FTexturesCache::FTexturesCacheElem* Texture = nullptr;
	FMaterialsDatabase::FMaterialSyncData&	  SyncData;
};

void FMaterialInfo::Init(const FSyncContext& SyncContext, const API_Component3D& CUmat, const FMaterialKey& MaterialKey)
{
	// Get modeler material
	ModelerAPI::AttributeIndex IndexMaterial(ModelerAPI::AttributeIndex::MaterialIndex, MaterialKey.ACMaterialIndex);
	ModelerAPI::Material	   AcMaterial;
	SyncContext.GetModel().GetMaterial(IndexMaterial, &AcMaterial);
	ModelerAPI::AttributeIndex IndexTexture;
	AcMaterial.GetTextureIndex(IndexTexture);
	GS::Int32 textureIndex = IndexTexture.GetOriginalModelerIndex();
	if (MaterialKey.ACTextureIndex != kInvalidMaterialIndex)
	{
		textureIndex = MaterialKey.ACTextureIndex;
	}

	if (SyncData.MaterialId == APINULLGuid)
	{
		API_Guid MatGuid = CUmat.umat.mater.head.guid;
		if (MatGuid == APINULLGuid)
		{
			bIdIsSynthetized = true;
			// Simulate a Guid from material name and properties
			MD5::Generator g;
			std::string	   name(CUmat.umat.mater.head.uniStringNamePtr->ToUtf8());
			g.Update(name.c_str(), (unsigned int)name.size());
			const char* p1 = (const char*)&CUmat.umat.mater.mtype;
			g.Update(p1, (unsigned int)((const char*)&CUmat.umat.mater.texture - p1));
			MD5::FingerPrint fp;
			g.Finish(fp);
			MatGuid = Fingerprint2API_Guid(fp);
			if (textureIndex > 0)
			{
				// Add the texture finderprint
				MatGuid = CombineGuid(MatGuid,
									  SyncContext.GetTexturesCache().GetTexture(SyncContext, textureIndex).Fingerprint);
			}
			UE_AC_VerboseF("Simulate Guid for material %d, %s Guid=%s\n", MaterialKey.ACMaterialIndex,
						   DisplayName.ToUtf8(), APIGuidToString(MatGuid).ToUtf8());
		}
		SyncData.MaterialId = APIGuid2GSGuid(MatGuid);
		SyncData.MaterialIndex = CUmat.umat.mater.head.index;
	}

	DatasmithId = GSStringToUE(SyncData.MaterialId.ToUniString());
	DatasmithLabel = GSStringToUE((*CUmat.umat.mater.head.uniStringNamePtr));

	// If the material use a texture
	if (textureIndex > 0)
	{
		// Add the texture info to SyncDatabase
		Texture = &SyncContext.GetTexturesCache().GetTexture(SyncContext, textureIndex);

		Rotation = AcMaterial.GetTextureRotationAngle();
		InvXSize = Texture->InvXSize;
		InvYSize = Texture->InvYSize;
		bMirrorX = Texture->bMirrorX;
		bMirrorY = Texture->bMirrorY;

		if (MaterialKey.ACTextureIndex != kInvalidMaterialIndex)
		{
			GS::UniString fingerprint = APIGuidToString(Texture->Fingerprint);

			DatasmithId += TEXT("_");
			DatasmithId += GSStringToUE(fingerprint);

			DatasmithLabel += GSStringToUE(fingerprint);
			DatasmithLabel += "_";
			DatasmithLabel += GSStringToUE(Texture->TextureLabel);
		}
	}

	if (MaterialKey.Sided == kDoubleSide)
	{
		DatasmithId += TEXT("_DS");
		DatasmithLabel += TEXT("_DS");
	}

	Opacity = 1.0f - (float)AcMaterial.GetTransparency();
	SurfaceColor = AcMaterial.GetSurfaceColor();
	SpecularReflection = (float)AcMaterial.GetSpecularReflection();
	EmissiveColor = AcMaterial.GetEmissionColor();
	bTwoSide = MaterialKey.Sided == kDoubleSide;
}

void FMaterialInfo::UpdateMaterial(const TSharedPtr< IDatasmithUEPbrMaterialElement >& DSMaterial)
{
	bool bRemoveAllExpressions = true;
	DSMaterial->ResetExpressionGraph(bRemoveAllExpressions);
	DSMaterial->SetLabel(*DatasmithLabel);

	const bool bIsTransparent = Opacity != 1.0;
	bool	   bHasAphaMask = false;

	if (Texture != nullptr)
	{
		IDatasmithMaterialExpressionTexture* BaseTextureExpression =
			DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionTexture >();
		BaseTextureExpression->SetTexturePathName(GSStringToUE(APIGuidToString(Texture->Fingerprint)));
		BaseTextureExpression->SetName(TEXT("Diffuse_Map"));
		BaseTextureExpression->ConnectExpression(DSMaterial->GetBaseColor());

		if (PIVOT_0_5_0_5 != 0 || InvXSize != 1.0 || InvYSize != 1.0 || bMirrorX || bMirrorY || Rotation != 0.0)
		{
			IDatasmithMaterialExpressionFunctionCall* UVEditExpression =
				DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionFunctionCall >();
			UVEditExpression->SetFunctionPathName(TEXT("/DatasmithContent/Materials/UVEdit.UVEdit"));
			UVEditExpression->ConnectExpression(BaseTextureExpression->GetInputCoordinate());

			// Mirror
			IDatasmithMaterialExpressionBool* MirrorUFlag =
				DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionBool >();
			MirrorUFlag->SetName(TEXT("Mirror U"));
			MirrorUFlag->GetBool() = bMirrorX;

			MirrorUFlag->ConnectExpression(*UVEditExpression->GetInput(3));

			IDatasmithMaterialExpressionBool* MirrorVFlag =
				DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionBool >();
			MirrorVFlag->SetName(TEXT("Mirror V"));
			MirrorVFlag->GetBool() = bMirrorY;

			MirrorVFlag->ConnectExpression(*UVEditExpression->GetInput(4));

			// Tiling
			IDatasmithMaterialExpressionColor* TilingValue =
				DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
			TilingValue->SetName(TEXT("UV Tiling"));
			TilingValue->GetColor() = FLinearColor(float(InvXSize), float(InvYSize), 0.0f);

			TilingValue->ConnectExpression(*UVEditExpression->GetInput(2));

			IDatasmithMaterialExpressionColor* OffsetValue =
				DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
			OffsetValue->SetName(TEXT("UV Offset"));
#if PIVOT_0_5_0_5
			OffsetValue->GetColor() = FLinearColor(-0.5f, -0.5f, 0.0f);
#else
			OffsetValue->GetColor() = FLinearColor(0.0f, 0.0f, 0.0f);
#endif

			OffsetValue->ConnectExpression(*UVEditExpression->GetInput(7));

			IDatasmithMaterialExpressionColor* TilingPivot =
				DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
			TilingPivot->SetName(TEXT("Tiling Pivot"));
			TilingPivot->GetColor() = FLinearColor(0.0f, 0.0f, 0.f);

			TilingPivot->ConnectExpression(*UVEditExpression->GetInput(1));

			// Rotation
			if (!FMath::IsNearlyZero(Rotation))
			{
				IDatasmithMaterialExpressionScalar* RotationValue =
					DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionScalar >();
				RotationValue->SetName(TEXT("W Rotation"));
				double intPart;
				float  Rot = float(modf(Rotation * -(1.0 / PI), &intPart) * 0.5);
				RotationValue->GetScalar() = Rot;

				RotationValue->ConnectExpression(*UVEditExpression->GetInput(6));

				IDatasmithMaterialExpressionColor* RotationPivot =
					DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
				RotationPivot->SetName(TEXT("Rotation Pivot"));
#if PIVOT_0_5_0_5
				RotationPivot->GetColor() = FLinearColor(0.5f, 0.5f, 0.0f);
#else
				RotationPivot->GetColor() = FLinearColor(0.0f, 0.0f, 0.0f);
#endif
				RotationPivot->ConnectExpression(*UVEditExpression->GetInput(5));
			}

			IDatasmithMaterialExpressionTextureCoordinate* TextureCoordinateExpression =
				DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionTextureCoordinate >();
			TextureCoordinateExpression->SetCoordinateIndex(0);
			TextureCoordinateExpression->ConnectExpression(*UVEditExpression->GetInput(0));
		}
		if (Texture->bHasAlpha && Texture->bAlphaIsTransparence)
		{
			if (bIsTransparent)
			{
				IDatasmithMaterialExpressionGeneric* MultiplyExpression =
					DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionGeneric >();
				MultiplyExpression->SetExpressionName(TEXT("Multiply"));
				MultiplyExpression->SetName(TEXT("Multiply Expression"));

				IDatasmithMaterialExpressionScalar* OpacityExpression =
					DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionScalar >();
				OpacityExpression->GetScalar() = Opacity;
				OpacityExpression->SetName(TEXT("Opacity"));

				IDatasmithExpressionInput* MultiplyInputA = MultiplyExpression->GetInput(0);
				IDatasmithExpressionInput* MultiplyInputB = MultiplyExpression->GetInput(1);

				MultiplyExpression->ConnectExpression(DSMaterial->GetOpacity());

				BaseTextureExpression->ConnectExpression(*MultiplyInputA, 4);
				OpacityExpression->ConnectExpression(*MultiplyInputB);
			}
			else
			{
				BaseTextureExpression->ConnectExpression(DSMaterial->GetOpacity(), 4);
			}
			bHasAphaMask = true;
		}
	}
	else
	{
		// Diffuse color
		IDatasmithMaterialExpressionColor* DiffuseExpression =
			DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
		if (DiffuseExpression != nullptr)
		{
			DiffuseExpression->GetColor() = ACRGBColorToUELinearColor(SurfaceColor);
			DiffuseExpression->SetName(TEXT("Base Color"));
			DiffuseExpression->ConnectExpression(DSMaterial->GetBaseColor());
		}
	}

	// Specular color
	IDatasmithMaterialExpressionScalar* specularExpression =
		DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionScalar >();
	specularExpression->GetScalar() = SpecularReflection;
	specularExpression->SetName(TEXT("Specular"));
	specularExpression->ConnectExpression(DSMaterial->GetSpecular());

	// Emissive color
	IDatasmithMaterialExpressionColor* EmissiveExpression =
		DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
	EmissiveExpression->GetColor() = ACRGBColorToUELinearColor(EmissiveColor);
	EmissiveExpression->SetName(TEXT("Emissive Color"));
	EmissiveExpression->ConnectExpression(DSMaterial->GetEmissiveColor());

	// Opacity
	if (!bHasAphaMask && bIsTransparent)
	{
		IDatasmithMaterialExpressionScalar* OpacityExpression =
			DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionScalar >();
		OpacityExpression->GetScalar() = Opacity;
		OpacityExpression->SetName(TEXT("Opacity"));
		OpacityExpression->ConnectExpression(DSMaterial->GetOpacity());
	}

	if (bTwoSide)
	{
		DSMaterial->SetTwoSided(bTwoSide);
	}

#if 0
	// Metallic
	float Metallic = 0.0;
	if (DatasmithLabel.Contains(TEXT("Metal"))) // Experimental
	{
		Metallic = 1.0;
	}
	IDatasmithMaterialExpressionScalar* MetallicExpression =
		DSMaterial->AddMaterialExpression< IDatasmithMaterialExpressionScalar >();
	MetallicExpression->GetScalar() = Metallic;
	MetallicExpression->SetName(TEXT("Metallic"));
	MetallicExpression->ConnectExpression(DSMaterial->GetMetallic());
#endif
}

// Constructor
FMaterialsDatabase::FMaterialsDatabase() {}

// Destructor
FMaterialsDatabase::~FMaterialsDatabase() {}

// Reset
void FMaterialsDatabase::Clear()
{
	MapMaterials.Reset();
	MaterialsNamesSet.Reset();
}

// Return true if at least one material have been modified
bool FMaterialsDatabase::CheckModify()
{
	FAutoChangeDatabase changeDB(APIWind_3DModelID);

	for (TPair< FMaterialKey, TUniquePtr< FMaterialSyncData > >& Iter : MapMaterials)
	{
		UE_AC_Assert(Iter.Value.IsValid());
		if (Iter.Value->CheckModify(Iter.Key))
		{
			return true;
		}
	}
	return false;
}

// Scan all material and update modified ones
void FMaterialsDatabase::UpdateModified(const FSyncContext& SyncContext)
{
	for (TPair< FMaterialKey, TUniquePtr< FMaterialSyncData > >& Iter : MapMaterials)
	{
		UE_AC_Assert(Iter.Value.IsValid());
		Iter.Value->Update(SyncContext, Iter.Key);
	}
}

// Return true if the material have been modified
bool FMaterialsDatabase::FMaterialSyncData::CheckModify(const FMaterialKey& /* MaterialKey */)
{
	if (!bIsDuplicate && !bIdIsSynthetized && MaterialIndex != kInvalidMaterialIndex)
	{
		API_Attribute MaterialAttibute;
		Zap(&MaterialAttibute);
		MaterialAttibute.header.typeID = API_MaterialID;
		MaterialAttibute.header.index = MaterialIndex;
		GSErrCode GSErr = ACAPI_Attribute_Get(&MaterialAttibute);
		if (GSErr == NoError)
		{
			if (MaterialAttibute.header.modiTime == 0)
			{
				MaterialAttibute.header.modiTime = 1;
			}
			if (LastModificationStamp != MaterialAttibute.header.modiTime)
			{
				return true;
			}
		}
		else
		{
			UE_AC_DebugF("FMaterialsDatabase::FMaterialSyncData::CheckModify - ACAPI_Attribute_Search error=%s\n",
						 GetErrorName(GSErr));
		}
	}

	return false;
}

const FMaterialsDatabase::FMaterialSyncData& FMaterialsDatabase::GetMaterial(const FSyncContext& SyncContext,
																			 GS::Int32			 inACMaterialIndex,
																			 GS::Int32 inACTextureIndex, ESided InSided)
{
	// Test invariant
	if (inACMaterialIndex <= kInvalidMaterialIndex)
	{
		UE_AC_DebugF("FMaterialsDatabase::GetMaterial - Invalid material index (%d)\n", inACMaterialIndex);
		inACMaterialIndex = 1;
	}
	// Test invariant
	if (inACTextureIndex < kInvalidMaterialIndex)
	{
		UE_AC_DebugF("FMaterialsDatabase::GetMaterial - Invalid texture index (%d)\n", inACTextureIndex);
		inACTextureIndex = kInvalidMaterialIndex;
	}
	FMaterialKey MaterialKey(inACMaterialIndex, inACTextureIndex, InSided);

	TUniquePtr< FMaterialSyncData >& material = MapMaterials.FindOrAdd(MaterialKey);
	if (!material.IsValid())
	{
		FMaterialSyncData* MatSyncData = new FMaterialSyncData();
		material.Reset(MatSyncData);
		MatSyncData->Init(SyncContext, MaterialKey);
	}

	return *material;
}

void FMaterialsDatabase::FMaterialSyncData::Init(const FSyncContext& SyncContext, const FMaterialKey& MaterialKey)
{
	if (bIsInitialized)
	{
		return;
	}
	bIsInitialized = true;

	GS::UniString	DisplayName;
	API_Component3D CUmat;
	Zap(&CUmat);
	CUmat.header.typeID = API_UmatID;
	CUmat.header.index = MaterialKey.ACMaterialIndex;
	CUmat.umat.mater.head.uniStringNamePtr = &DisplayName;
	GSErrCode GSErr = ACAPI_3D_GetComponent(&CUmat);
	if (GSErr == NoError)
	{
		LastModificationStamp = CUmat.umat.mater.head.modiTime;

		FMaterialInfo MaterialInfo(this);

		MaterialInfo.Init(SyncContext, CUmat, MaterialKey);
		bIdIsSynthetized = MaterialInfo.bIdIsSynthetized;
		DatasmithId = MaterialInfo.DatasmithId;
		DatasmithLabel = MaterialInfo.DatasmithLabel;
		bHasTexture = MaterialInfo.Texture != nullptr;

		FMaterialsDatabase::SetMaterialsNames& MaterialsNamesSet = SyncContext.GetMaterialsDatabase().MaterialsNamesSet;
		if (MaterialsNamesSet.Find(DatasmithId) != nullptr)
		{
			bIsDuplicate = true;
			return; // An identical material already exist (Can happen for simulated Guid)
		}
		MaterialsNamesSet.Add(DatasmithId);

		Element = FDatasmithSceneFactory::CreateUEPbrMaterial(*MaterialInfo.DatasmithId);
		MaterialInfo.UpdateMaterial(Element);
	}
	else
	{
		// Set a gray diffuse color
		MaterialId.Generate();
		bIdIsSynthetized = true;
		DatasmithId = GSStringToUE(MaterialId.ToUniString());
		DatasmithLabel = FString(TEXT("Invalid material index"));
		Element = FDatasmithSceneFactory::CreateUEPbrMaterial(*DatasmithId);
		Element->SetLabel(*DatasmithLabel);
		{
			IDatasmithMaterialExpressionColor* DiffuseExpression =
				Element->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
			if (DiffuseExpression != nullptr)
			{
				DiffuseExpression->GetColor() = FLinearColor(0.5f, 0.5f, 0.5f);
				DiffuseExpression->SetName(TEXT("Base Color"));
				DiffuseExpression->ConnectExpression(Element->GetBaseColor());
			}
		}
	}
	SyncContext.GetScene().AddMaterial(Element);
}

// Return true if the material have been modified
void FMaterialsDatabase::FMaterialSyncData::Update(const FSyncContext& SyncContext, const FMaterialKey& MaterialKey)
{
	if (bIsDuplicate)
	{
		return;
	}

	// Get 3D DB material
	GS::UniString	DisplayName;
	API_Component3D CUmat;
	Zap(&CUmat);
	CUmat.header.typeID = API_UmatID;
	CUmat.header.index = MaterialKey.ACMaterialIndex;
	CUmat.umat.mater.head.uniStringNamePtr = &DisplayName;
	GSErrCode GSErr = ACAPI_3D_GetComponent(&CUmat);
	if (GSErr == NoError)
	{
		if (CUmat.umat.mater.head.modiTime == 0)
		{
			CUmat.umat.mater.head.modiTime = 1;
		}
		if (LastModificationStamp != CUmat.umat.mater.head.modiTime)
		{
			LastModificationStamp = CUmat.umat.mater.head.modiTime;

			FMaterialInfo MaterialInfo(this);
			MaterialInfo.Init(SyncContext, CUmat, MaterialKey);
			DatasmithLabel = MaterialInfo.DatasmithLabel;
			bHasTexture = MaterialInfo.Texture != nullptr;
			MaterialInfo.UpdateMaterial(Element);
		}
	}
	else
	{
		UE_AC_DebugF("FMaterialsDatabase::FMaterialSyncData::Update - ACAPI_3D_GetComponent error=%s\n",
					 GetErrorName(GSErr));
	}
}

END_NAMESPACE_UE_AC
