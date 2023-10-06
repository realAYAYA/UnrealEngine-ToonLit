// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "MeshAttributeArray.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "UObject/NameTypes.h"


namespace MeshAttribute
{
	namespace Vertex
	{
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName CornerSharpness;	// deprecated
	}

	namespace VertexInstance
	{
		extern STATICMESHDESCRIPTION_API const FName TextureCoordinate;
		extern STATICMESHDESCRIPTION_API const FName Normal;
		extern STATICMESHDESCRIPTION_API const FName Tangent;
		extern STATICMESHDESCRIPTION_API const FName BinormalSign;
		extern STATICMESHDESCRIPTION_API const FName Color;
	}

	namespace Edge
	{
		extern STATICMESHDESCRIPTION_API const FName IsHard;
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName IsUVSeam;			// deprecated
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName CreaseSharpness;	// deprecated
	}

	namespace Triangle
	{
		extern STATICMESHDESCRIPTION_API const FName Normal;
		extern STATICMESHDESCRIPTION_API const FName Tangent;
		extern STATICMESHDESCRIPTION_API const FName Binormal;
	}

	namespace Polygon
	{
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName Normal;
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName Tangent;
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName Binormal;
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName Center;
	}

	namespace PolygonGroup
	{
		extern STATICMESHDESCRIPTION_API const FName ImportedMaterialSlotName;
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName EnableCollision;	// deprecated
		UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
		extern STATICMESHDESCRIPTION_API const FName CastShadow;		// deprecated
	}
}


PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FStaticMeshAttributes : public FMeshAttributes
{
public:

	explicit FStaticMeshAttributes(FMeshDescription& InMeshDescription)
		: FMeshAttributes(InMeshDescription)
	{}

	STATICMESHDESCRIPTION_API virtual void Register(bool bKeepExistingAttribute = false) override;
	
	static bool IsReservedAttributeName(const FName InAttributeName)
	{
		return FMeshAttributes::IsReservedAttributeName(InAttributeName) ||
               InAttributeName == MeshAttribute::VertexInstance::TextureCoordinate ||
               InAttributeName == MeshAttribute::VertexInstance::Normal ||
               InAttributeName == MeshAttribute::VertexInstance::Tangent ||
               InAttributeName == MeshAttribute::VertexInstance::BinormalSign ||
               InAttributeName == MeshAttribute::VertexInstance::Color ||
               InAttributeName == MeshAttribute::Edge::IsHard ||
               InAttributeName == MeshAttribute::Triangle::Normal ||
               InAttributeName == MeshAttribute::Triangle::Tangent ||
               InAttributeName == MeshAttribute::Triangle::Binormal ||
               InAttributeName == MeshAttribute::PolygonGroup::ImportedMaterialSlotName
		;
	}	

	UE_DEPRECATED(4.26, "Please use RegisterTriangleNormalAndTangentAttributes() instead.")
	STATICMESHDESCRIPTION_API void RegisterPolygonNormalAndTangentAttributes();

	STATICMESHDESCRIPTION_API void RegisterTriangleNormalAndTangentAttributes();

	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TVertexAttributesRef<float> GetVertexCornerSharpnesses() { return MeshDescription.VertexAttributes().GetAttributesRef<float>(MeshAttribute::Vertex::CornerSharpness); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TVertexAttributesConstRef<float> GetVertexCornerSharpnesses() const { return MeshDescription.VertexAttributes().GetAttributesRef<float>(MeshAttribute::Vertex::CornerSharpness); }

	TVertexInstanceAttributesRef<FVector2f> GetVertexInstanceUVs() { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate); }
	TVertexInstanceAttributesConstRef<FVector2f> GetVertexInstanceUVs() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate); }

	TVertexInstanceAttributesRef<FVector3f> GetVertexInstanceNormals() { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Normal); }
	TVertexInstanceAttributesConstRef<FVector3f> GetVertexInstanceNormals() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Normal); }

	TVertexInstanceAttributesRef<FVector3f> GetVertexInstanceTangents() { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Tangent); }
	TVertexInstanceAttributesConstRef<FVector3f> GetVertexInstanceTangents() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Tangent); }

	TVertexInstanceAttributesRef<float> GetVertexInstanceBinormalSigns() { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign); }
	TVertexInstanceAttributesConstRef<float> GetVertexInstanceBinormalSigns() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign); }

	TVertexInstanceAttributesRef<FVector4f> GetVertexInstanceColors() { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color); }
	TVertexInstanceAttributesConstRef<FVector4f> GetVertexInstanceColors() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color); }

	TEdgeAttributesRef<bool> GetEdgeHardnesses() { return MeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard); }
	TEdgeAttributesConstRef<bool> GetEdgeHardnesses() const { return MeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard); }

	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TEdgeAttributesRef<float> GetEdgeCreaseSharpnesses() { return MeshDescription.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TEdgeAttributesConstRef<float> GetEdgeCreaseSharpnesses() const { return MeshDescription.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness); }

	TTriangleAttributesRef<FVector3f> GetTriangleNormals() { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Normal); }
	TTriangleAttributesConstRef<FVector3f> GetTriangleNormals() const { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Normal); }

	TTriangleAttributesRef<FVector3f> GetTriangleTangents() { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Tangent); }
	TTriangleAttributesConstRef<FVector3f> GetTriangleTangents() const { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Tangent); }

	TTriangleAttributesRef<FVector3f> GetTriangleBinormals() { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Binormal); }
	TTriangleAttributesConstRef<FVector3f> GetTriangleBinormals() const { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Binormal); }

	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesRef<FVector3f> GetPolygonNormals() { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Normal); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonNormals() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Normal); }

	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesRef<FVector3f> GetPolygonTangents() { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Tangent); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonTangents() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Tangent); }

	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesRef<FVector3f> GetPolygonBinormals() { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Binormal); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonBinormals() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Binormal); }

	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesRef<FVector3f> GetPolygonCenters() { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Center); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonCenters() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Center); }

	TPolygonGroupAttributesRef<FName> GetPolygonGroupMaterialSlotNames() { return MeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName); }
	TPolygonGroupAttributesConstRef<FName> GetPolygonGroupMaterialSlotNames() const { return MeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName); }
};


class FStaticMeshConstAttributes : public FMeshConstAttributes
{
public:

	explicit FStaticMeshConstAttributes(const FMeshDescription& InMeshDescription)
		: FMeshConstAttributes(InMeshDescription)
	{}

	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TVertexAttributesConstRef<float> GetVertexCornerSharpnesses() const { return MeshDescription.VertexAttributes().GetAttributesRef<float>(MeshAttribute::Vertex::CornerSharpness); }
	TVertexInstanceAttributesConstRef<FVector2f> GetVertexInstanceUVs() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate); }
	TVertexInstanceAttributesConstRef<FVector3f> GetVertexInstanceNormals() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Normal); }
	TVertexInstanceAttributesConstRef<FVector3f> GetVertexInstanceTangents() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Tangent); }
	TVertexInstanceAttributesConstRef<float> GetVertexInstanceBinormalSigns() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign); }
	TVertexInstanceAttributesConstRef<FVector4f> GetVertexInstanceColors() const { return MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color); }
	TEdgeAttributesConstRef<bool> GetEdgeHardnesses() const { return MeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TEdgeAttributesConstRef<float> GetEdgeCreaseSharpnesses() const { return MeshDescription.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness); }
	TTriangleAttributesConstRef<FVector3f> GetTriangleNormals() const { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Normal); }
	TTriangleAttributesConstRef<FVector3f> GetTriangleTangents() const { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Tangent); }
	TTriangleAttributesConstRef<FVector3f> GetTriangleBinormals() const { return MeshDescription.TriangleAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Triangle::Binormal); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonNormals() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Normal); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonTangents() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Tangent); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonBinormals() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Binormal); }
	UE_DEPRECATED(4.26, "This attribute is no longer supported, please remove code pertaining to it.")
	TPolygonAttributesConstRef<FVector3f> GetPolygonCenters() const { return MeshDescription.PolygonAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Polygon::Center); }
	TPolygonGroupAttributesConstRef<FName> GetPolygonGroupMaterialSlotNames() const { return MeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName); }
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
