// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpMaterial.h"

#include "DatasmithSketchUpCommon.h"
#include "DatasmithSketchUpComponent.h"
#include "DatasmithSketchUpExportContext.h"
#include "DatasmithSketchUpMetadata.h"
#include "DatasmithSketchUpMesh.h"
#include "DatasmithSketchUpString.h"
#include "DatasmithSketchUpSummary.h"
#include "DatasmithSketchUpTexture.h"
#include "DatasmithSketchUpUtils.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/model/component_instance.h"
#include <SketchUpAPI/model/image.h>
#include <SketchUpAPI/model/image_rep.h>
#include <SketchUpAPI/model/layer.h>
#include "SketchUpAPI/model/model.h"
#include "SketchUpAPI/model/texture.h"
#include "SketchUpAPI/model/typed_value.h"
#include "SketchUpAPI/model/rendering_options.h"
#include "DatasmithSketchUpSDKCeases.h"

// Datasmith SDK.

#include "Containers/Array.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "DatasmithMaterialsUtils.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"


using namespace DatasmithSketchUp;


// Structure that holds extracted data from SketchUp material during export process
class FExtractedMaterial
{
public:

	FExtractedMaterial(
		FExportContext& Context,
		SUMaterialRef InSMaterialDefinitionRef // source SketchUp material definition
	);

	// Source SketchUp material referecene
	SUMaterialRef SourceMaterialRef;

	// Extracted from SketchUp
	// Source SketchUp material ID.
	FEntityIDType SketchupSourceID;

	// Source SketchUp material name.
	FString SketchupSourceName;

	// Source SketchUp material type: SUMaterialType_Colored, SUMaterialType_Textured or SUMaterialType_ColorizedTexture.
	SUMaterialType SourceType;

	// Source SketchUp material color.
	SUColor SourceColor;

	// Whether or not the source SketchUp color alpha values are used.
	bool bSourceColorAlphaUsed;

	SUTextureRef TextureRef = SU_INVALID;

	FString LocalizedMaterialName;
	FString InheritedMaterialName;


private:

	// Make the material names sanitized for Datasmith.
	void InitMaterialNames();
};

FExtractedMaterial::FExtractedMaterial(
	FExportContext& Context,
	SUMaterialRef InSMaterialDefinitionRef)
	: SourceMaterialRef(InSMaterialDefinitionRef)
	, SourceColor({ 128, 128, 128, 255 }) // default RGBA: sRGB opaque middle gray
{
	SUResult SResult = SU_ERROR_NONE;

	// Get the material ID of the SketckUp material.
	SketchupSourceID = DatasmithSketchUpUtils::GetMaterialID(SourceMaterialRef);

	// Retrieve the SketchUp material name.
	SketchupSourceName = SuGetString(SUMaterialGetName, SourceMaterialRef);
	// Remove any name encasing "[]".
	SketchupSourceName.RemoveFromStart(TEXT("["));
	SketchupSourceName.RemoveFromEnd(TEXT("]"));

	// Get the SketchUp material type.
	SUMaterialGetType(SourceMaterialRef, &SourceType); // we can ignore the returned SU_RESULT


	// Get the SketchUp material color.
	SUColor SMaterialColor;
	SResult = SUMaterialGetColor(SourceMaterialRef, &SMaterialColor);
	// Keep the default opaque middle gray when the material does not have a color value (SU_ERROR_NO_DATA).
	if (SResult == SU_ERROR_NONE)
	{
		SourceColor = SMaterialColor;
	}

	// Get the flag indicating whether or not the SketchUp color alpha values are used.
	SUMaterialGetUseOpacity(SourceMaterialRef, &bSourceColorAlphaUsed); // we can ignore the returned SU_RESULT

	// Retrieve the SketchUp material texture.
	SResult = SUMaterialGetTexture(SourceMaterialRef, &TextureRef);

	// Make the material names sanitized for Datasmith.
	InitMaterialNames();
}

void FExtractedMaterial::InitMaterialNames()
{
	FString SanitizedName = FDatasmithUtils::SanitizeObjectName(SketchupSourceName);
	FString HashedName    = FMD5::HashAnsiString(*SketchupSourceName);

	LocalizedMaterialName = FString::Printf(TEXT("%ls-L%ls"), *SanitizedName, *HashedName);
	InheritedMaterialName = FString::Printf(TEXT("%ls-I%ls"), *SanitizedName, *HashedName);
}

FMaterialIDType const FMaterial::DEFAULT_MATERIAL_ID;
FMaterialIDType const FMaterial::INHERITED_MATERIAL_ID;


namespace DatasmithSketchUp
{
	TSharedPtr<IDatasmithBaseMaterialElement> CreateMaterialElement(
		FExportContext& Context,
		FExtractedMaterial& InMaterial,
		TCHAR const* InMaterialName,
		FTexture* Texture,
		bool bInScaleTexture
	)
	{
		// Create a Datasmith material element for the material definition.
		TSharedRef<IDatasmithUEPbrMaterialElement> DatasmithMaterialElementPtr = FDatasmithSceneFactory::CreateUEPbrMaterial(InMaterialName);

		// Set the material element label used in the Unreal UI.
		FString MaterialLabel = FDatasmithUtils::SanitizeObjectName(InMaterial.SketchupSourceName);
		DatasmithMaterialElementPtr->SetLabel(*MaterialLabel);

		
		FLinearColor LinearColor = FMaterial::ConvertColor(InMaterial.SourceColor, InMaterial.bSourceColorAlphaUsed);

		DatasmithMaterialElementPtr->SetTwoSided(false);// todo: consider this

		IDatasmithMaterialExpression* ExpressionOpacityFromTexture = nullptr;
		IDatasmithMaterialExpressionScalar* ExpressionOpacityScalar = nullptr;

		if (Texture)
		{
			IDatasmithMaterialExpressionTexture* ExpressionTexture = DatasmithMaterialElementPtr->AddMaterialExpression< IDatasmithMaterialExpressionTexture >();
			ExpressionTexture->SetName(TEXT("Texture"));
			ExpressionTexture->SetTexturePathName(Texture->GetDatasmithElementName());

			// Apply texture scaling
			if (bInScaleTexture && !Texture->TextureScale.Equals(FVector2D::UnitVector))
			{
				IDatasmithMaterialExpressionFunctionCall* UVEditExpression = DatasmithMaterialElementPtr->AddMaterialExpression< IDatasmithMaterialExpressionFunctionCall >();
				UVEditExpression->SetFunctionPathName(TEXT("/DatasmithContent/Materials/UVEdit.UVEdit"));

				UVEditExpression->ConnectExpression(ExpressionTexture->GetInputCoordinate());

				// Tiling
				IDatasmithMaterialExpressionColor* TilingValue = DatasmithMaterialElementPtr->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
				TilingValue->SetName(TEXT("UV Tiling"));
				TilingValue->GetColor() = FLinearColor(Texture->TextureScale.X, Texture->TextureScale.Y, 0.f);

				TilingValue->ConnectExpression(*UVEditExpression->GetInput(2));

				//IDatasmithMaterialExpressionColor* OffsetValue = MaterialElement->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
				//OffsetValue->SetName(TEXT("UV Offset"));
				//OffsetValue->GetColor() = FLinearColor(0.f, 0.f, 0.f);
				//OffsetValue->ConnectExpression(*UVEditExpression->GetInput(7));

				IDatasmithMaterialExpressionTextureCoordinate* TextureCoordinateExpression = DatasmithMaterialElementPtr->AddMaterialExpression< IDatasmithMaterialExpressionTextureCoordinate >();
				TextureCoordinateExpression->SetCoordinateIndex(0);
				TextureCoordinateExpression->ConnectExpression(*UVEditExpression->GetInput(0));
			}

			ExpressionTexture->ConnectExpression(DatasmithMaterialElementPtr->GetBaseColor());

			// Set the Datasmith material element opacity.
			if (Texture->GetTextureUseAlphaChannel())
			{
				ExpressionOpacityFromTexture = ExpressionTexture;
			}
		}
		else
		{
			IDatasmithMaterialExpressionColor* ExpressionColor = DatasmithMaterialElementPtr->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
			ExpressionColor->SetName(TEXT("Base Color"));
			ExpressionColor->GetColor() = LinearColor;
			ExpressionColor->ConnectExpression(DatasmithMaterialElementPtr->GetBaseColor());
		}

		// Create scalar parameter if opacity is set in material or texture has opacity channel recognized by SU
		if (InMaterial.bSourceColorAlphaUsed || ExpressionOpacityFromTexture)
		{
			ExpressionOpacityScalar = DatasmithMaterialElementPtr->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
			ExpressionOpacityScalar->SetName(TEXT("Opacity"));
			ExpressionOpacityScalar->GetScalar() = InMaterial.bSourceColorAlphaUsed ? (float(InMaterial.SourceColor.alpha) / float(255)) : 1.f;
			ExpressionOpacityScalar->ConnectExpression(DatasmithMaterialElementPtr->GetOpacity());
		}

		IDatasmithMaterialExpression* ExpressionOpacity = nullptr;

		if (ExpressionOpacityFromTexture)
		{
			IDatasmithMaterialExpressionGeneric* Multiply = DatasmithMaterialElementPtr->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			Multiply->SetExpressionName(TEXT("Multiply"));

			ExpressionOpacityFromTexture->ConnectExpression(*Multiply->GetInput(0), 4);  // Use Alpha output from Texture sampler
			ExpressionOpacityScalar->ConnectExpression(*Multiply->GetInput(1));

			ExpressionOpacity = Multiply;
		}
		else
		{
			ExpressionOpacity = ExpressionOpacityScalar;
		}

		if (ExpressionOpacity)
		{
			ExpressionOpacity->ConnectExpression(DatasmithMaterialElementPtr->GetOpacity());
			DatasmithMaterialElementPtr->SetBlendMode(/*EBlendMode::BLEND_Translucent*/2);
		}

		Context.DatasmithScene->AddMaterial(DatasmithMaterialElementPtr);

		// Return the Datasmith material element.
		return DatasmithMaterialElementPtr;
	}

	void FModel::ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material)
	{
		GetDefinition()->ApplyOverrideMaterialToNode(Node, Material);
	}

	void FComponentInstance::ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material)
	{
		GetDefinition()->ApplyOverrideMaterialToNode(Node, Material);
	}

	void FDefinition::ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material)
	{
		FEntitiesGeometry& EntitiesGeometry = *GetEntities().EntitiesGeometry;

		for (int32 MeshIndex = 0; MeshIndex < Node.MeshActors.Num(); ++MeshIndex)
		{
			// Update Override(Inherited)  Material
			// todo: set inherited material only on mesh actors that have faces with default material, right now setting on every mesh, hot harmful but excessive
			if (EntitiesGeometry.IsMeshUsingInheritedMaterial(MeshIndex))
			{
				const TSharedPtr<IDatasmithMeshActorElement>& MeshActor = Node.MeshActors[MeshIndex];

				// SketchUp has 'material override' only for single('Default') material. 
				// So we reset overrides on the actor to remove this single override(if it was set) and re-add new override
				MeshActor->ResetMaterialOverrides(); // Clear previous override if was set
				MeshActor->AddMaterialOverride(Material.GetName(), EntitiesGeometry.GetInheritedMaterialOverrideSlotId());
			}
		}
	}
}

FMaterial::FMaterial(SUMaterialRef InMaterialRef)
	: MaterialRef(InMaterialRef)
	, bInvalidated(true)
{}

TSharedPtr<FMaterial> FMaterial::Create(FExportContext& Context, SUMaterialRef InMaterialRef)
{
	TSharedPtr<FMaterial> Material = MakeShared<FMaterial>(InMaterialRef);
	return Material;
}

void FMaterial::Invalidate()
{
	bInvalidated = true;
}

FLinearColor FMaterial::ConvertColor(const SUColor& C, bool bAlphaUsed)
{
	FColor SRGBColor(C.red, C.green, C.blue, bAlphaUsed ? C.alpha : 255);
	return FLinearColor(SRGBColor);
}

TSharedRef<IDatasmithBaseMaterialElement> FMaterial::CreateDefaultMaterialElement(FExportContext& Context)
{
	TSharedRef<IDatasmithUEPbrMaterialElement> DatasmithMaterialElementPtr = FDatasmithSceneFactory::CreateUEPbrMaterial(TEXT("Default"));

	FLinearColor LinearColor(0.5, 0.5, 0.5, 1);

	// Retrieve Front Face color current Style - Styles api doesn't have a way to access it, using RenderingOptions instead
	SURenderingOptionsRef RenderingOptionsRef = SU_INVALID;
	if (SUModelGetRenderingOptions(Context.ModelRef, &RenderingOptionsRef) == SU_ERROR_NONE)
	{
		SUTypedValueRef ColorTypedValue = SU_INVALID;
		SUTypedValueCreate(&ColorTypedValue);
		if (SURenderingOptionsGetValue(RenderingOptionsRef, "FaceFrontColor", &ColorTypedValue) == SU_ERROR_NONE)
		{
			SUColor Color;
			if (SUTypedValueGetColor(ColorTypedValue, &Color) == SU_ERROR_NONE)
			{
				LinearColor = ConvertColor(Color);
			}
		}
		SUTypedValueRelease(&ColorTypedValue);
	}

	DatasmithMaterialElementPtr->SetTwoSided(false);// todo: consider this

	IDatasmithMaterialExpressionColor* ExpressionColor = DatasmithMaterialElementPtr->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
	ExpressionColor->SetName(TEXT("Base Color"));
	ExpressionColor->GetColor() = LinearColor;
	ExpressionColor->ConnectExpression(DatasmithMaterialElementPtr->GetBaseColor());

	Context.DatasmithScene->AddMaterial(DatasmithMaterialElementPtr);

	return DatasmithMaterialElementPtr;
}

void FMaterial::Remove(FExportContext& Context)
{
	// Expecting that SketchUp removes material from geometry/instances prior to removing it from the scene, but in case it doesn't...
	if (!ensure(MaterialDirectlyAppliedToMeshes.MeshesMaterialDirectlyAppliedTo.IsEmpty()))
	{
		for (FEntitiesGeometry* Geom: MaterialDirectlyAppliedToMeshes.MeshesMaterialDirectlyAppliedTo.Array())
		{
			UnregisterGeometry(Context, Geom);
		}
	}

	// Expecting that SketchUp removes material from geometry/instances prior to removing it from the scene, but in case it doesn't...
	if (!ensure(MaterialInheritedByNodes.NodesMaterialInheritedBy.IsEmpty()))
	{
		for (FNodeOccurence* Node: MaterialDirectlyAppliedToMeshes.NodesMaterialInheritedBy.Array())
		{
			UnregisterInstance(Context, Node);
		}
	}


	Context.Textures.UnregisterMaterial(this);
}

FMD5Hash FMaterial::ComputeHash(FExportContext& Context)
{
	FMD5 MD5;
	FExtractedMaterial ExtractedMaterial(Context, MaterialRef);

	MD5.Update(reinterpret_cast<uint8*>(&ExtractedMaterial.SourceType), sizeof(ExtractedMaterial.SourceType));
	MD5.Update(reinterpret_cast<uint8*>(&ExtractedMaterial.SourceColor), sizeof(ExtractedMaterial.SourceColor));
	MD5.Update(reinterpret_cast<uint8*>(&ExtractedMaterial.bSourceColorAlphaUsed), sizeof(ExtractedMaterial.bSourceColorAlphaUsed));
	// MD5.Update(reinterpret_cast<uint8*>(&ExtractedMaterial.SketchupSourceName), sizeof(ExtractedMaterial.SketchupSourceName));

	size_t TextureWidth = 0;
	size_t TextureHeight = 0;
	double TextureSScale = 1.0;
	double TextureTScale = 1.0;
	SUTextureGetDimensions(ExtractedMaterial.TextureRef, &TextureWidth, &TextureHeight, &TextureSScale, &TextureTScale);

	bool bUseAlphaChannel = false;
	SUTextureGetUseAlphaChannel(ExtractedMaterial.TextureRef, &bUseAlphaChannel);

	MD5.Update(reinterpret_cast<uint8*>(&TextureWidth), sizeof(TextureWidth));
	MD5.Update(reinterpret_cast<uint8*>(&TextureHeight), sizeof(TextureHeight));
	MD5.Update(reinterpret_cast<uint8*>(&TextureSScale), sizeof(TextureSScale));
	MD5.Update(reinterpret_cast<uint8*>(&TextureTScale), sizeof(TextureTScale));
	MD5.Update(reinterpret_cast<uint8*>(&bUseAlphaChannel), sizeof(bUseAlphaChannel));

	FMD5Hash Hash;
	Hash.Set(MD5);
	return Hash;
}

void FMaterial::UpdateTexturesUsage(FExportContext& Context)
{
	if (!bInvalidated)
	{
		return;
	}

	if (Texture)
	{
		Context.Textures.UnregisterMaterial(this);
		Texture = nullptr;
	}

	// todo: extract once(another usage in Update)
	FExtractedMaterial ExtractedMaterial(Context, MaterialRef);

	if (SUIsValid(ExtractedMaterial.TextureRef))
	{
		Texture = (ExtractedMaterial.SourceType == SUMaterialType::SUMaterialType_ColorizedTexture)
			? Context.Textures.AddColorizedTexture(ExtractedMaterial.TextureRef, ExtractedMaterial.SketchupSourceName)
			: Context.Textures.AddTexture(ExtractedMaterial.TextureRef, ExtractedMaterial.SketchupSourceName);
		Context.Textures.RegisterMaterial(this);
	}
}

void FMaterial::Update(FExportContext& Context)
{
	FExtractedMaterial ExtractedMaterial(Context, MaterialRef);
	EntityId = ExtractedMaterial.SketchupSourceID.EntityID;

	MaterialDirectlyAppliedToMeshes.RemoveDatasmithElement(Context);
	if (MaterialDirectlyAppliedToMeshes.HasUsers())
	{
		MaterialDirectlyAppliedToMeshes.DatasmithElement = CreateMaterialElement(Context, ExtractedMaterial, *ExtractedMaterial.LocalizedMaterialName, Texture, !bGeometryHasScalingBakedIntoUvs);
	}

	MaterialInheritedByNodes.RemoveDatasmithElement(Context);
	if (MaterialInheritedByNodes.HasUsers())
	{
		MaterialInheritedByNodes.DatasmithElement = CreateMaterialElement(Context, ExtractedMaterial, *ExtractedMaterial.InheritedMaterialName, Texture, true);
	}

	bInvalidated = false;
}

FMaterialOccurrence& FMaterial::RegisterGeometry(FEntitiesGeometry* Geom)
{
	MaterialDirectlyAppliedToMeshes.RegisterGeometry(Geom);

	if (MaterialDirectlyAppliedToMeshes.IsInvalidated())
	{
		Invalidate(); // Invalidate material if occurrence is not built
	}

	return MaterialDirectlyAppliedToMeshes;
}

void FMaterial::UnregisterGeometry(FExportContext& Context, FEntitiesGeometry* Geom)
{
	MaterialDirectlyAppliedToMeshes.UnregisterGeometry(Context, Geom);
}

FMaterialOccurrence& FMaterial::RegisterInstance(FNodeOccurence* NodeOccurrence)
{
	MaterialInheritedByNodes.RegisterInstance(NodeOccurrence);

	if (MaterialInheritedByNodes.IsInvalidated())
	{
		Invalidate(); // Invalidate material if occurrence is not built
	}

	NodeOccurrence->MaterialOverride = this;
	return MaterialInheritedByNodes;
}

void FMaterial::UnregisterInstance(FExportContext& Context, FNodeOccurence* NodeOccurrence)
{
	MaterialInheritedByNodes.UnregisterInstance(Context, NodeOccurrence);

	NodeOccurrence->MaterialOverride = nullptr;
}

const TCHAR* FMaterialOccurrence::GetName()
{
	if (!DatasmithElement)
	{
		return nullptr;
	}

	return DatasmithElement->GetName();
}

// todo: make virtual FMaterialOccurrence - for layer/regular materials different?
// they differ by the way they are applied to geometry/nodes -
// VERY tied to meshes composition - they either map slots from SU Material or from SU Layer
void FMaterialOccurrence::ApplyRegularMaterial(FMaterialIDType MaterialId)
{
	// Apply material to meshes
	for (FEntitiesGeometry* Geometry : MeshesMaterialDirectlyAppliedTo)
	{
		//Geometry->SetMaterialElementName(DatasmithSketchUpUtils::GetMaterialID(MaterialRef), MaterialDirectlyAppliedToMeshes->GetName());
		for (const TSharedPtr<FDatasmithInstantiatedMesh>& Mesh : Geometry->Meshes)
		{
			if (int32* SlotIdPtr = Mesh->SlotIdForMaterialId.Find(MaterialId))
			{
				Mesh->DatasmithMesh->SetMaterial(GetName(), *SlotIdPtr);
			}
		}
	}

	// Apply material to mesh actors
	for (FNodeOccurence* NodePtr : NodesMaterialInheritedBy)
	{
		NodePtr->Entity.ApplyOverrideMaterialToNode(*NodePtr, *this);
	}
}

void FMaterialOccurrence::ApplyLayerMaterial(FLayerIDType LayerId)
{
	// Apply material to meshes
	for (FEntitiesGeometry* Geometry : MeshesMaterialDirectlyAppliedTo)
	{
		//Geometry->SetMaterialElementName(DatasmithSketchUpUtils::GetMaterialID(MaterialRef), MaterialDirectlyAppliedToMeshes->GetName());
		for (const TSharedPtr<FDatasmithInstantiatedMesh>& Mesh : Geometry->Meshes)
		{
			// todo: maybe can refactor this and unify FMaterialOccurrence for regular/layer materials?
			// By moving this into the Mesh? It has this occurrence anyway so it can keep the association of material<->slot there?
			if (int32* SlotIdPtr = Mesh->SlotIdForLayerId.Find(LayerId))
			{
				Mesh->DatasmithMesh->SetMaterial(GetName(), *SlotIdPtr);
			}
		}
	}

	// Apply material to mesh actors
	for (FNodeOccurence* NodePtr : NodesMaterialInheritedBy)
	{
		NodePtr->Entity.ApplyOverrideMaterialToNode(*NodePtr, *this);
	}
}

void FMaterialOccurrence::RemoveDatasmithElement(FExportContext& Context)
{
	if (!DatasmithElement)
	{
		return;
	}
	Context.DatasmithScene->RemoveMaterial(DatasmithElement);
	DatasmithElement.Reset();
}

bool FMaterialOccurrence::RemoveUser(FExportContext& Context)
{
	if(!ensure(UserCount != 0))
	{
		return true;
	}

	UserCount--;

	if (UserCount != 0)
	{
		return false;
	}

	RemoveDatasmithElement(Context);
	return true;
}


void FMaterialOccurrence::RegisterGeometry(FEntitiesGeometry* Geom)
{
	if (!MeshesMaterialDirectlyAppliedTo.Contains(Geom))
	{
		MeshesMaterialDirectlyAppliedTo.Add(Geom);
		AddUser();
	}
}

void FMaterialOccurrence::UnregisterGeometry(FExportContext& Context, FEntitiesGeometry* Geom)
{
	if (!MeshesMaterialDirectlyAppliedTo.Contains(Geom))
	{
		return;
	}

	MeshesMaterialDirectlyAppliedTo.Remove(Geom);
	RemoveUser(Context);
}

void FMaterialOccurrence::RegisterInstance(FNodeOccurence* NodeOccurrence)
{
	NodesMaterialInheritedBy.Add(NodeOccurrence);
	AddUser();
}

void FMaterialOccurrence::UnregisterInstance(FExportContext& Context, FNodeOccurence* NodeOccurrence)
{
	NodesMaterialInheritedBy.Remove(NodeOccurrence);
	RemoveUser(Context);
}

void FRegularMaterials::RemoveUnused()
{
	TArray<FMaterialIDType> UnusedMaterials;
	for (TPair<FMaterialIDType, TSharedPtr<FMaterial>> IdAndMaterial : MaterialForMaterialId)
	{
		FMaterial& Material = *IdAndMaterial.Value;
		if (!Material.IsUsed())
		{
			UnusedMaterials.Add(IdAndMaterial.Key);
		}
	}

	// Remove unused materials(SketchUp material removal notification is unusable but we need to remove unused materials)
	for (FMaterialIDType MaterialId: UnusedMaterials)
	{
		RemoveMaterial(MaterialId);
	}

	if (!DefaultMaterial.HasUsers())
	{
		DefaultMaterial.Invalidate(Context);
	}
}

void FRegularMaterials::UpdateDefaultMaterial()
{
	if (DefaultMaterial.IsInvalidated() && DefaultMaterial.HasUsers())
	{
		DefaultMaterial.DatasmithElement = FMaterial::CreateDefaultMaterialElement(Context);
		DefaultMaterial.ApplyRegularMaterial(FMaterial::INHERITED_MATERIAL_ID);
	}
}

void FRegularMaterials::Apply(FMaterial* Material)
{
	FMaterialIDType* MaterialIdFound = MaterialIdForMaterial.Find(Material);
	if (!MaterialIdFound)
	{
		return;
	}
	FMaterialIDType MaterialId = *MaterialIdFound;
	
	Material->MaterialDirectlyAppliedToMeshes.ApplyRegularMaterial(MaterialId);
	Material->MaterialInheritedByNodes.ApplyRegularMaterial(MaterialId);
}

void FLayerMaterials::RemoveUnused()
{
	TArray<FLayerIDType> UnusedMaterials;
	for (TPair<FLayerIDType, TSharedPtr<FMaterial>> IdAndMaterial : MaterialForLayerId)
	{
		FMaterial& Material = *IdAndMaterial.Value;
		if (!Material.IsUsed())
		{
			UnusedMaterials.Add(IdAndMaterial.Key);
		}
	}

	for (FLayerIDType LayerId: UnusedMaterials)
	{
		TSharedPtr<FMaterial> Material;
		if (MaterialForLayerId.RemoveAndCopyValue(LayerId, Material))
		{
			MaterialHashForLayerId.Remove(LayerId);
			LayerIdForMaterial.Remove(Material.Get());
			Context.Materials.RemoveMaterial(Material.Get());
		}
	}
}

void FLayerMaterials::Apply(FMaterial* Material)
{
	FLayerIDType* Found = LayerIdForMaterial.Find(Material);
	if (!Found)
	{
		return;
	}
	FLayerIDType LayerId = *Found;

	MaterialHashForLayerId.Add(LayerId, Material->ComputeHash(Context));
	
	Material->MaterialDirectlyAppliedToMeshes.ApplyLayerMaterial(LayerId);
	Material->MaterialInheritedByNodes.ApplyLayerMaterial(LayerId);
}

void FLayerMaterials::UpdateLayer(SULayerRef LayerRef)
{
	if (TSharedPtr<DatasmithSketchUp::FMaterial>* Found = MaterialForLayerId.Find(DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef))))
	{
		(*Found)->Invalidate();
	}
}

// todo: VERY similar to regular material's RegisterInstance
FMaterialOccurrence* FLayerMaterials::RegisterInstance(FLayerIDType LayerID, FNodeOccurence* NodeOccurrence)
{
	if (NodeOccurrence->MaterialOverride)
	{
		NodeOccurrence->MaterialOverride->UnregisterInstance(Context, NodeOccurrence);
	}

	if (const TSharedPtr<DatasmithSketchUp::FMaterial> Material = FindOrCreateMaterialForLayer(LayerID))
	{
		return &Material->RegisterInstance(NodeOccurrence);
	}

	return {}; // Don't use a material if material id is unknown(of default)
}

// Test if a layer material has changed since last update 
bool FLayerMaterials::CheckForModifications()
{
	size_t LayerCount = 0;
	SUModelGetNumLayers(Context.ModelRef, &LayerCount);

	TArray<SULayerRef> Layers;
	Layers.Init(SU_INVALID, LayerCount);
	SUResult SResult = SUModelGetLayers(Context.ModelRef, LayerCount, Layers.GetData(), &LayerCount);
	Layers.SetNum(LayerCount);

	bool bHasModifications = false;

	for (SULayerRef LayerRef : Layers)
	{
		FLayerIDType LayerId = DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef));
		if (TSharedPtr<DatasmithSketchUp::FMaterial>* Found = MaterialForLayerId.Find(LayerId))
		{
			TSharedPtr<DatasmithSketchUp::FMaterial> Material = *Found;
			FMD5Hash Hash = Material->ComputeHash(Context);
			FMD5Hash* HashFound = MaterialHashForLayerId.Find(LayerId);
			if (!HashFound || (*HashFound != Hash))
			{
				Material->Invalidate();
				bHasModifications = true;
			}
		}
	}

	return bHasModifications;
}

void FMaterialCollection::Update()
{
	LayerMaterials.RemoveUnused();
	RegularMaterials.RemoveUnused();

	// Update usage of materials and textures by materials before updating textures(to only update used textures)
	for (FMaterial* Material : Materials)
	{
		Material->UpdateTexturesUsage(Context);
	}

	Context.Textures.Update();

	// Update materials after textures are updated - some materials might end up using shared texture(in case it has same contents, which is determined in Textures Update)
	for (FMaterial* Material: Materials)
	{
		if (Material->IsInvalidated())
		{
			Material->Update(Context);
			LayerMaterials.Apply(Material);
			RegularMaterials.Apply(Material);
		}
	}

	RegularMaterials.UpdateDefaultMaterial();
}

namespace DatasmithSketchUp
{

class FImageMaterial
{
public:

	struct FOptions
	{
		bool bScaleTexture = false;
		FVector2D TextureScale;
		bool bUseAlphaChannel = false;
		bool bTranslucent = false;
	};

	FImageMaterial(FExportContext& Context, FImage& Image, FImageFile& ImageFile)
	{
		FString NameBase = Image.GetName();

		FString MaterialName = FString::Printf(TEXT("image_material_%s"), *NameBase);  
		FString MaterialLabel = NameBase;

		FString TextureName = FString::Printf(TEXT("image_texture_%s"), *NameBase);  

		TextureElement = FDatasmithSceneFactory::CreateTexture(*TextureName);
		TextureElement->SetSRGB(EDatasmithColorSpace::sRGB);
		TextureElement->SetFile(Context.ImageFiles.GetImageFilePath(ImageFile));
		Context.DatasmithScene->AddTexture(TextureElement);

		FImageMaterial::FOptions MaterialOptions;
		MaterialOptions.bUseAlphaChannel = Context.ImageFiles.GetImageHasAlpha(ImageFile);
		CreateMaterialElement(Context, *MaterialName, *MaterialLabel, TextureElement->GetName(), MaterialOptions);

		Hash = Context.ImageFiles.GetImageFileHash(ImageFile);
	}

	void Release(FExportContext& Context)
	{
		if (DatasmithMaterialElement)
		{
			Context.DatasmithScene->RemoveMaterial(DatasmithMaterialElement);
			DatasmithMaterialElement.Reset();
		}

		if (TextureElement)
		{
			Context.DatasmithScene->RemoveTexture(TextureElement);
			TextureElement.Reset();
		}
	}

	const TCHAR* GetMaterialElementName()
	{
		return DatasmithMaterialElement->GetName();
	}

	TSharedPtr<IDatasmithBaseMaterialElement> CreateMaterialElement(
		FExportContext& Context,
		const TCHAR* InMaterialName,
		const TCHAR* InMaterialLabel,
		const TCHAR* InDatasmithTextureElementName,
		const FOptions& Options
	)
	{
		// Create a Datasmith material element for the material definition.
		DatasmithMaterialElement = FDatasmithSceneFactory::CreateUEPbrMaterial(InMaterialName);

		// Set the material element label used in the Unreal UI.
		FString MaterialLabel = FDatasmithUtils::SanitizeObjectName(InMaterialLabel);
		DatasmithMaterialElement->SetLabel(*MaterialLabel);

		DatasmithMaterialElement->SetTwoSided(true); // imported Image in SketchUp is displayed as two-sided with no way to controls it, so 

		bool bTranslucent = false; 

		{
			IDatasmithMaterialExpressionTexture* ExpressionTexture = DatasmithMaterialElement->AddMaterialExpression< IDatasmithMaterialExpressionTexture >();
			ExpressionTexture->SetName(TEXT("Texture"));
			ExpressionTexture->SetTexturePathName(InDatasmithTextureElementName);

			// Apply texture scaling
			if (Options.bScaleTexture && !Options.TextureScale.Equals(FVector2D::UnitVector))
			{
				IDatasmithMaterialExpressionFunctionCall* UVEditExpression = DatasmithMaterialElement->AddMaterialExpression< IDatasmithMaterialExpressionFunctionCall >();
				UVEditExpression->SetFunctionPathName(TEXT("/DatasmithContent/Materials/UVEdit.UVEdit"));

				UVEditExpression->ConnectExpression(ExpressionTexture->GetInputCoordinate());

				// Tiling
				IDatasmithMaterialExpressionColor* TilingValue = DatasmithMaterialElement->AddMaterialExpression< IDatasmithMaterialExpressionColor >();
				TilingValue->SetName(TEXT("UV Tiling"));
				TilingValue->GetColor() = FLinearColor(Options.TextureScale.X, Options.TextureScale.Y, 0.f);

				TilingValue->ConnectExpression(*UVEditExpression->GetInput(2));

				IDatasmithMaterialExpressionTextureCoordinate* TextureCoordinateExpression = DatasmithMaterialElement->AddMaterialExpression< IDatasmithMaterialExpressionTextureCoordinate >();
				TextureCoordinateExpression->SetCoordinateIndex(0);
				TextureCoordinateExpression->ConnectExpression(*UVEditExpression->GetInput(0));
			}

			ExpressionTexture->ConnectExpression(DatasmithMaterialElement->GetBaseColor());

			bTranslucent = Options.bTranslucent || Options.bUseAlphaChannel;

			// Set the Datasmith material element opacity.
			if (Options.bUseAlphaChannel)
			{
				ExpressionTexture->ConnectExpression(DatasmithMaterialElement->GetOpacity(), 4);  // Use Alpha output from Texture sampler
			}
		}

		if (bTranslucent)
		{
			DatasmithMaterialElement->SetBlendMode(/*EBlendMode::BLEND_Translucent*/2);
		}

		// World Position Offset
		// Shift image along local Z
		{
			IDatasmithMaterialExpressionGeneric* ExpressionObjectOrientation = DatasmithMaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			ExpressionObjectOrientation->SetExpressionName(TEXT("ObjectOrientation"));
			IDatasmithMaterialExpressionScalar* ExpressionPositionOffset = DatasmithMaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
			ExpressionPositionOffset->SetName(TEXT("PositionOffset"));
			ExpressionPositionOffset->GetScalar() = 1.f;

			IDatasmithMaterialExpressionGeneric* MultiplyOrientationWithOffset = DatasmithMaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			MultiplyOrientationWithOffset->SetExpressionName(TEXT("Multiply"));

			ExpressionObjectOrientation->ConnectExpression(*MultiplyOrientationWithOffset->GetInput(0));
			ExpressionPositionOffset->ConnectExpression(*MultiplyOrientationWithOffset->GetInput(1));

			MultiplyOrientationWithOffset->ConnectExpression(DatasmithMaterialElement->GetWorldPositionOffset());
		}

		Context.DatasmithScene->AddMaterial(DatasmithMaterialElement);

		// Return the Datasmith material element.
		return DatasmithMaterialElement;
	}

	TSet<FImage*> ImagesForMaterial;
	FMD5Hash Hash;

private:
	TSharedPtr<IDatasmithUEPbrMaterialElement> DatasmithMaterialElement;
	TSharedPtr<IDatasmithTextureElement> TextureElement;
};

}

void FImageCollection::ReleaseMaterial(FImage& Image)
{
	if (Image.ImageMaterial)
	{
		Image.ImageMaterial->ImagesForMaterial.Remove(&Image);

		if (Image.ImageMaterial->ImagesForMaterial.IsEmpty())
		{
			Image.ImageMaterial->Release(Context); //  Clean up material from the datasmith scene before deallocating
			ImageMaterials.Remove(Image.ImageMaterial->Hash);
		}

		Image.ImageMaterial = nullptr;
	}
}

const TCHAR* FImageCollection::AcquireMaterial(FImage& Image)
{
	if (!ensure(!Image.ImageMaterial))
	{
		ReleaseMaterial(Image);
	}

	SUImageRepRef ImageRep = SU_INVALID;
	ensure(SU_ERROR_NONE == SUImageRepCreate(&ImageRep));
	SUResult ImageGetImageRepResult = SUImageGetImageRep(SUImageFromEntity(Image.EntityRef), &ImageRep);
	ensure(ImageGetImageRepResult == SU_ERROR_NONE);

	TSharedPtr<FImageFile> ImageFile = Context.ImageFiles.AddImage(ImageRep, Image.GetFileName());
	SUResult ImageRepReleaseResult = SUImageRepRelease(&ImageRep);
	ensure(SU_ERROR_NONE == ImageRepReleaseResult);

	TSharedPtr<FImageMaterial> Material;

	if (TSharedPtr<FImageMaterial>* Found = ImageMaterials.Find(Context.ImageFiles.GetImageFileHash(*ImageFile)))
	{
		Material = (*Found);
	}
	else
	{
		Material = MakeShared<FImageMaterial>(Context, Image, *ImageFile);
		ImageMaterials.Add(Material->Hash, Material);
	}

	Material->ImagesForMaterial.Add(&Image);
	Image.ImageMaterial = Material.Get();

	return Material->GetMaterialElementName();
}
