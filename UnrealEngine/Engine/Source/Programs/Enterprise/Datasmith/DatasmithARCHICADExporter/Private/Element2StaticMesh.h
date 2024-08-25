// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

#include "MaterialsDatabase.h"

#include "ModelMeshBody.hpp"
#include "Polygon.hpp"
#include "AttributeIndex.hpp"
#include "Transformation3D.hpp"
#include "TextureCoordinate.hpp"

#include "DatasmithMeshExporter.h"

#include "Map.h"

BEGIN_NAMESPACE_UE_AC

#if defined(DEBUG)
	#define DUMP_GEOMETRY 1 // For debug purpose
#else
	#define DUMP_GEOMETRY 0
#endif

// Encapsulate ModelerAPI::TextureCoordinate to used as TMap Key
class FTextureCoordinate : public ModelerAPI::TextureCoordinate
{
  public:
	FTextureCoordinate() {}
	FTextureCoordinate(const ModelerAPI::TextureCoordinate& InUV)
		: ModelerAPI::TextureCoordinate(InUV)
	{
	}
};

inline uint32 GetTypeHash(const FTextureCoordinate& A)
{
	return FCrc::MemCrc_DEPRECATED(&A, sizeof(A));
}

inline bool operator==(const FTextureCoordinate& UVA, const FTextureCoordinate& UVB)
{
	return UVA.u == UVB.u && UVA.v == UVB.v;
}

class FSyncContext;

// Class that will convert AC element to StaticMesh
class FElement2StaticMesh
{
  public:
	// Constructor
	FElement2StaticMesh(const FSyncContext& InSyncContext);

	// Destructor
	~FElement2StaticMesh();

	// Create a datasmith mesh element
	TSharedPtr< IDatasmithMeshElement > CreateMesh();

	// Collect geometry of the element
	void AddElementGeometry(const ModelerAPI::Element& InModelElement, const Geometry::Vector3D& InGeometryShift);

	// Return the numbers of bugs detected during conversion
	unsigned int GetBugsCount() const { return BugsCount; }

	// Return true of we have at least one face.
	bool HasGeometry() const { return GlobalMaterialsUsed.Num() != 0; }

#if DUMP_GEOMETRY
	// Return a mesh dump
	static utf8_string MeshAsString(const FDatasmithMesh& Mesh);

	// Dump a mesh
	static void DumpMesh(const FDatasmithMesh& Mesh);

	// Return a mesh element dump
	static utf8_string MeshElementAsString(const IDatasmithMeshElement& Mesh);

	// Dump a mesh element
	static void DumpMeshElement(const IDatasmithMeshElement& Mesh);
#endif

  private:
	// Compute name of mesh element
	FString ComputeMeshElementName(const TCHAR* InMeshFileHash);

	// Fill 3d maesh data from collected goemetry
	void FillMesh(FDatasmithMesh* OutMesh);

	// Create a triangle for polygon vertex ≈ new Triangle(first, previous, last)
	void AddVertex(GS::Int32 InBodyVertex, const Geometry::Vector3D& VertexNormal);

	// Set the material for the current polygon
	void InitPolygonMaterial();

	// Triangle data
	class FTriangle
	{
	  public:
		enum
		{
			kInvalidIndex = -1
		};

		// Default constructor
		FTriangle() {}

		bool IsValid() const { return V0 != V1 && V0 != V2 && V1 != V2; }

		int		V0 = kInvalidIndex;
		int		V1 = kInvalidIndex;
		int		V2 = kInvalidIndex;
		int		UV0 = kInvalidIndex;
		int		UV1 = kInvalidIndex;
		int		UV2 = kInvalidIndex;
		FVector3f Normals[3] = {};

		// Material
		int	 LocalMatID = 0;
		bool bIsCurved = false;
	};

	// Current context
	const FSyncContext& SyncContext;

	// Trasform to 'shift' original mesh vertices in local space
	Geometry::Vector3D GeometryShift;

	// Working variables
	bool				 bSomeHasTextures; // True if at least one triangles need uv
	ModelerAPI::MeshBody CurrentBody; // Current body that we collect geometry from
	bool				 bIsSurfaceBody; // Current body is a surface (ie need doble side material)
	ModelerAPI::Polygon	 CurrentPolygon; // Current polygon that we collect geometry from
	const FMaterialsDatabase::FMaterialSyncData* CurrentMaterial; // Current polygon global material
	int32										 LocalMaterialIndex = 0; // Current polygon local material
	FTriangle									 CurrentTriangle; // Current triangle data
	int											 StartVertex; // Number of vertex collected before current body
	unsigned int								 VertexCount; // Number of edges processed in the current polygon
	FVector										 CurrentNormal; // Current normal, setted before calling AddVertex
	ModelerAPI::AttributeIndex					 MaterialIndex;
	ModelerAPI::AttributeIndex					 TextureIndex;

	// Vertex value and used flag or index
	class FVertex;

  public:
	typedef TArray< FVertex > VecVertices;

  private:
	typedef TArray< FTriangle >									   VecTriangles;
	typedef TMap< FTextureCoordinate, int >						   MapUVs;
	typedef TArray< const FMaterialsDatabase::FMaterialSyncData* > VecMaterialSyncData;

	VecVertices			Vertices; // Vector of used vertices
	VecTriangles		Triangles; // Vector of collected triangles
	MapUVs				UVs; // Map of used UVs
	VecMaterialSyncData GlobalMaterialsUsed;

	unsigned int BugsCount; // Count of bugs during geometry conversion
};

END_NAMESPACE_UE_AC
