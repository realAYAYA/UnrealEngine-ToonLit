// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpComponent.h"

#include "DatasmithSketchUpExportContext.h"
#include "DatasmithSketchUpMaterial.h"
#include "DatasmithSketchUpMetadata.h"
#include "DatasmithSketchUpString.h"
#include "DatasmithSketchUpSummary.h"
#include "DatasmithSketchUpUtils.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/model/drawing_element.h"
#include <SketchUpAPI/model/entity.h>
#include <SketchUpAPI/model/image.h>
#include <SketchUpAPI/model/image_rep.h>
#include "SketchUpAPI/model/layer.h"
#include "SketchUpAPI/geometry/transformation.h"
#include "SketchUpAPI/geometry/vector3d.h"

#include "DatasmithSketchUpSDKCeases.h"

#include "IDatasmithSceneElements.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"

#include "Algo/AnyOf.h"
#include "Misc/Paths.h"


using namespace DatasmithSketchUp;

FImage::FImage(SUImageRef InEntityRef)
	: Super(SUImageToEntity(InEntityRef))
{
	FString IdStr = FString::Printf(TEXT("%llx"), GetPersistentId());
	MeshElementName = FString::Printf(TEXT("M%s"), *IdStr);
}

void FImage::RemoveImage(FExportContext& Context)
{
	RemoveOccurrences(Context);
	RemoveImageFromDatasmithScene(Context);
}

int64 FImage::GetPersistentId()
{
	int64 PersistentId = 0;
	SUEntityGetPersistentID(EntityRef, &PersistentId);

	return PersistentId;
}

FString FImage::GetFileName()
{
	// Get file name to use in name(label) computation. Not used to export file image!

	// Note: 
	// Not using SUImageGetName - returns empty string:
	// https://github.com/SketchUp/api-issue-tracker/issues/303

	// Also, newer SU has access to Definition of an Image
	// 	SUComponentDefinitionRef Definition = SU_INVALID;
	// 	SUResult ImageGetDefinitionResult = SUImageGetDefinition(SUImageFromEntity(EntityRef), &Definition);
	// 	ensure(SU_ERROR_NONE == ImageGetDefinitionResult);

	// So for SU 2019+ file name is the only way to get Image's 'name' 

	FString FileName = SuGetString(SUImageGetFileName, SUImageFromEntity(EntityRef));
	FString Name = FPaths::GetCleanFilename(FileName);

	if (!Name.IsEmpty())
	{
		return Name;
	}

	return FString::Printf(TEXT("image_%llx.png"), GetPersistentId());
}

void FImage::InvalidateImage()
{
	InvalidateEntityProperties();
	InvalidateEntityGeometry();
}


FString FImage::GetName()
{
	return FDatasmithUtils::SanitizeObjectName(GetFileName());
}

void FImage::ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material)
{
	// Image entity is not affected by override materials on components
}

void FImage::UpdateOccurrence(FExportContext& Context, FNodeOccurence& Node)
{
	Node.EffectiveLayerRef = DatasmithSketchUpUtils::GetEffectiveLayer( SUImageToDrawingElement(SUImageFromEntity(EntityRef)), Node.ParentNode->EffectiveLayerRef);

	BuildNodeNames(Node);

	FString EffectiveLayerName = FDatasmithUtils::SanitizeObjectName(SuGetString(SULayerGetName, Node.EffectiveLayerRef));
	Node.DatasmithActorElement->SetLayer(*EffectiveLayerName);
	Node.DatasmithActorElement->SetLabel(*Node.GetActorLabel());

	// Compute the world transform of the SketchUp component instance.
	SUTransformation LocalTransform;

	// note: For Image, transformation handling is a bit than for Component(see further)
	SUImageGetTransform(SUImageFromEntity(EntityRef), &LocalTransform);

	// Remove scaling from local Image's transform
	// SU embeds image dimensions into its transform's scaling and we don't need it since we construct geometry using Image entity's width/height
	SUVector3D XAxis{1, 0, 0};
	SUTransformationGetXAxis(&LocalTransform, &XAxis);
	SUVector3D YAxis{0, 1, 0};
	SUTransformationGetYAxis(&LocalTransform, &YAxis);
	SUVector3D ZAxis{0, 0, 1};
	SUTransformationGetZAxis(&LocalTransform, &ZAxis);

	SUVector3DNormalize(&XAxis);
	SUVector3DNormalize(&YAxis);
	SUVector3DNormalize(&ZAxis);

	SUPoint3D Origin{0, 0, 0};
	SUTransformationGetOrigin(&LocalTransform, &Origin);

	// SUTransformationSetFromPointAndAxes point parameter is not 'Origin' of transformation(or in other words translation of transform from local to world)
	// but rather inverse of translation
	SUPoint3D Point = {-Origin.x, -Origin.y, -Origin.z}; 
	SUTransformationSetFromPointAndAxes(&LocalTransform, &Point, &XAxis, &YAxis,  &ZAxis);

	SUTransformation WorldTransform;
	SUTransformationMultiply(&Node.ParentNode->WorldTransform, &LocalTransform, &WorldTransform);
	Node.WorldTransform = WorldTransform; // Store world transform to be used by children to compute its

	// Set the Datasmith actor world transform.
	DatasmithSketchUpUtils::SetActorTransform(Node.DatasmithActorElement, Node.WorldTransform);

	Node.ResetMetadataElement(Context);

	// Update Datasmith Mesh Actors
	for (int32 MeshIndex = 0; MeshIndex < Node.MeshActors.Num(); ++MeshIndex)
	{
		const TSharedPtr<IDatasmithMeshActorElement>& MeshActor = Node.MeshActors[MeshIndex];

		// Set mesh actor transform after node transform
		MeshActor->SetScale(Node.DatasmithActorElement->GetScale());
		MeshActor->SetRotation(Node.DatasmithActorElement->GetRotation());
		MeshActor->SetTranslation(Node.DatasmithActorElement->GetTranslation());
	}

	FString MeshActorLabel = Node.GetActorLabel();

	// Update Datasmith Mesh Actors
	for (int32 MeshIndex = 0; MeshIndex < Node.MeshActors.Num(); ++MeshIndex)
	{
		const TSharedPtr<IDatasmithMeshActorElement>& MeshActor = Node.MeshActors[MeshIndex];
		MeshActor->SetLabel(*MeshActorLabel);
		MeshActor->SetLayer(*EffectiveLayerName);
	}
}

void FImage::RemoveImageFromDatasmithScene(FExportContext& Context)
{
	Context.Images.ReleaseMaterial(*this);

	if (DatasmithMeshElement)
	{
		Context.DatasmithScene->RemoveMesh(DatasmithMeshElement);
		DatasmithMeshElement.Reset();
	}
}

void FImage::EntityOccurrenceVisible(FNodeOccurence* Node, bool bUses)
{
	bool bNodesInvisibleBefore = VisibleNodes.IsEmpty();
	FEntity::EntityOccurrenceVisible(Node, bUses);
	if (bNodesInvisibleBefore != VisibleNodes.IsEmpty())
	{
		// Just invalidate geometry to create/remove geometry depending on existing occurrences
		InvalidateEntityGeometry(); 
	}

}

const TCHAR* FImage::GetMeshElementName()
{
	return *MeshElementName;
}

void FImage::Update(FExportContext& Context)
{
	if (bGeometryInvalidated)
	{
		RemoveImageFromDatasmithScene(Context);

		if (!VisibleNodes.IsEmpty())
		{
			UpdateEntityGeometry(Context);
			UpdateGeometry(Context);

			DatasmithMeshElement->SetMaterial(Context.Images.AcquireMaterial(*this), 0);
		}
	}
}

void FImage::BuildNodeNames(FNodeOccurence& Node)
{
	int64 SketchupPersistentID = Node.Entity.GetPersistentId();
	Node.DatasmithActorName = FString::Printf(TEXT("%ls_%lld"), *Node.ParentNode->GetActorName(), SketchupPersistentID);

	FString EntityName = Node.Entity.GetName();
	Node.DatasmithActorLabel = FDatasmithUtils::SanitizeObjectName(EntityName.IsEmpty() ? GetName() : EntityName);
}

void FImage::SetupActor(FExportContext& Context, FNodeOccurence& Node)
{
	// Add the Datasmith actor component depth tag.
	// We use component depth + 1 to factor in the added Datasmith scene root once imported in Unreal.
	FString ComponentDepthTag = FString::Printf(TEXT("SU.DEPTH.%d"), Node.Depth);
	Node.DatasmithActorElement->AddTag(*ComponentDepthTag);

	// Add the Datasmith actor component instance path tag.
	FString InstancePathTag = Node.GetActorName().Replace(TEXT("SU"), TEXT("SU.PATH.0")).Replace(TEXT("_"), TEXT("."));
	Node.DatasmithActorElement->AddTag(*InstancePathTag);

	if (Node.ParentNode->DatasmithActorElement)
	{
		Node.ParentNode->DatasmithActorElement->AddChild(Node.DatasmithActorElement);
	}
	else
	{
		Context.DatasmithScene->AddActor(Node.DatasmithActorElement);
	}
}


void FImage::UpdateOccurrenceVisibility(FExportContext& Context, FNodeOccurence& Node)
{
	// Parent node, component instance and layer - all should be visible to have node visible
	Node.SetVisibility(Node.ParentNode->bVisible && !bHidden && bLayerVisible);

	EntityOccurrenceVisible(&Node, Node.bVisible);

	if (Node.bVisible)
	{
		Node.InvalidateProperties();
		Node.InvalidateMeshActors();
	}
	else
	{
		Node.RemoveDatasmithActorHierarchy(Context);
	}

	for (FNodeOccurence* ChildNode : Node.Children)
	{
		ChildNode->bVisibilityInvalidated = true;
	}
}

void FImage::UpdateOccurrenceMeshActors(FExportContext& Context, FNodeOccurence& Node)
{
	BuildNodeNames(Node);

	FString ComponentActorName = Node.GetActorName();
	
	{
		FString MeshActorName = ComponentActorName;

		TSharedPtr<IDatasmithMeshActorElement> DMeshActorPtr = FDatasmithSceneFactory::CreateMeshActor(*MeshActorName);

		Node.MeshActors.Add(DMeshActorPtr);

		// Add the Datasmith actor component depth tag.
		// We use component depth + 1 to factor in the added Datasmith scene root once imported in Unreal.
		FString ComponentDepthTag = FString::Printf(TEXT("SU.DEPTH.%d"), Node.Depth + 1);
		DMeshActorPtr->AddTag(*ComponentDepthTag);

		// Add the Datasmith actor component instance path tag.
		FString InstancePathTag = ComponentActorName.Replace(TEXT("SU"), TEXT("SU.PATH.0")).Replace(TEXT("_"), TEXT("."));
		DMeshActorPtr->AddTag(*InstancePathTag);

		// ADD_TRACE_LINE(TEXT("Actor %ls: %ls %ls %ls"), *MeshActorLabel, *ComponentDepthTag, *DefinitionGUIDTag, *InstancePathTag);

		// Set the Datasmith mesh element used by the mesh actor.
		DMeshActorPtr->SetStaticMeshPathName(GetMeshElementName());
	}

	Node.DatasmithActorElement = Node.MeshActors[0];

	SetupActor(Context, Node);
}

void FImage::ResetOccurrenceActors(FExportContext& Context, FNodeOccurence& Node)
{
	Node.ResetNodeActors(Context);
}

void FImage::InvalidateOccurrencesGeometry(FExportContext& Context)
{
	for (FNodeOccurence* Node : Occurrences)
	{
		Node->InvalidateMeshActors();
		Node->InvalidateProperties();
	}
}

void FImage::InvalidateOccurrencesProperties(FExportContext& Context)
{
	// When ComponentInstance is modified we need to determine if its visibility might have changed foremost
	// because this determines whether corresponding node would exist in the Datasmith scene 
	// Two things affect this - Hidden instance flag and layer(tag):

	bool bNewHidden = false;
	SUDrawingElementRef DrawingElementRef = SUImageToDrawingElement(SUImageFromEntity(EntityRef));
	SUDrawingElementGetHidden(DrawingElementRef, &bNewHidden);

	SUDrawingElementGetLayer(DrawingElementRef, &LayerRef);
	bool bNewLayerVisible = Context.Layers.IsLayerVisible(LayerRef);

	if (bHidden != bNewHidden || bLayerVisible != bNewLayerVisible)
	{
		bHidden = bNewHidden;
		bLayerVisible = bNewLayerVisible;
		for (FNodeOccurence* Node : Occurrences)
		{
			Node->bVisibilityInvalidated = true;
		}
	}

	for (FNodeOccurence* Node : Occurrences)
	{
		Node->InvalidateProperties();
	}
}

void FImage::UpdateMetadata(FExportContext& Context)
{
	ParsedMetadata = MakeUnique<FMetadata>(EntityRef);;
}

void FImage::UpdateEntityProperties(FExportContext& Context)
{
	if (bPropertiesInvalidated)
	{
		InvalidateEntityGeometry();
	}

	FEntity::UpdateEntityProperties(Context);
}

namespace DatasmithSketchUp
{
	class FImageFile
	{
	public:
		struct FOptions
		{
			bool bColorized = false;
		};

		SUImageRepRef ImageRep;
		FOptions Options;

		FString FilePath;

		bool bHasAlpha = false;

		FImageFile(SUImageRepRef InImageRep)
			: ImageRep(InImageRep)
		{
			size_t Width, Height;
			SUImageRepGetPixelDimensions(ImageRep, &Width, &Height);

			size_t DataSize, Bpp;
			SUImageRepGetDataSize(ImageRep, &DataSize, &Bpp);

			TArray<SUByte> Data;
			Data.SetNumUninitialized(DataSize);
			SUImageRepGetData(ImageRep, DataSize, Data.GetData());

			FMD5 MD5;
			MD5.Update(reinterpret_cast<const uint8*>(&Width), sizeof(Width));
			MD5.Update(reinterpret_cast<const uint8*>(&Height), sizeof(Height));
			MD5.Update(reinterpret_cast<const uint8*>(&Bpp), sizeof(Bpp));
			MD5.Update(reinterpret_cast<const uint8*>(&Options.bColorized), sizeof(Options.bColorized)); // Colorized flag affects resulting texture image
			MD5.Update(Data.GetData(), Data.Num());

			ImageHash.Set(MD5);

			TArray<SUColor> Colors;
			Colors.SetNum(Width*Height);
			SUImageRepGetDataAsColors(ImageRep, Colors.GetData());

			bHasAlpha = Algo::AnyOf(Colors, [](const SUColor& Color) { return Color.alpha != 255; });
		}

		FMD5Hash ImageHash;

	};

}

TSharedPtr<FImageFile> FImageFileCollection::AddImage(SUImageRepRef ImageRep, FString FileName)
{
	TSharedPtr<FImageFile> ImageFile = MakeShared<FImageFile>(ImageRep);
	if (TSharedPtr<FImageFile>* Found = ImageFiles.Find(ImageFile->ImageHash))
	{
		return *Found;
	}

	ImageFile->FilePath = FPaths::Combine(Context.GetAssetsOutputPath(), TCHAR_TO_UTF8(*FileName));
	SUResult SaveToFileResult = SUImageRepSaveToFile(ImageRep, TCHAR_TO_UTF8(*ImageFile->FilePath));
	ensure(SaveToFileResult == SU_ERROR_NONE);

	return ImageFiles.Add(ImageFile->ImageHash, ImageFile);
}

const TCHAR* FImageFileCollection::GetImageFilePath(FImageFile& ImageFile)
{
	return *ImageFile.FilePath;
}

FMD5Hash FImageFileCollection::GetImageFileHash(FImageFile& ImageFile)
{
	return ImageFile.ImageHash;
}

bool FImageFileCollection::GetImageHasAlpha(FImageFile& ImageFile)
{
	return ImageFile.bHasAlpha;	
}
