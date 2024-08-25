// Copyright Epic Games, Inc. All Rights Reserved.

#include "Element2StaticMesh.h"
#include "Synchronizer.h"
#include "DatasmithHashTools.h"
#include "Utils/3DElement2String.h"

#include "ConvexPolygon.hpp"
#include "ModelElement.hpp"

#undef TicksPerSecond

#include "DatasmithSceneFactory.h"
#include "DatasmithMesh.h"
#include "DatasmithMeshExporter.h"
#include "DatasmithSceneExporter.h"
#include "Paths.h"

BEGIN_NAMESPACE_UE_AC

inline const Geometry::Point3D& Vertex2Point3D(const ModelerAPI::Vertex& inV)
{
	return reinterpret_cast< const Geometry::Point3D& >(inV);
}

inline const Geometry::Vector3D& Vertor2Vector3D(const ModelerAPI::Vector& inV)
{
	return reinterpret_cast< const Geometry::Vector3D& >(inV);
}

// Vertex value and used flag or mootools index
class FElement2StaticMesh::FVertex
{
  public:
	enum
	{
		kInvalidVertex = -1
	};

	// Constructor
	FVertex()
		: Used(false)
		, Index(kInvalidVertex)
	{
	}

	ModelerAPI::Vertex LocalVertex; // The vertex in local coordinates
	ModelerAPI::Vertex WorldVertex; // The vertex in world coordinates
	bool			   Used; // Used flag
	int				   Index; // New index
};

// Constructor
FElement2StaticMesh::FElement2StaticMesh(const FSyncContext& InSyncContext)
	: SyncContext(InSyncContext)
	, bSomeHasTextures(false)
	, BugsCount(0)
	, GeometryShift(0, 0, 0)
{
}

FElement2StaticMesh::~FElement2StaticMesh() {}

// Compute name of mesh element
FString FElement2StaticMesh::ComputeMeshElementName(const TCHAR* InMeshFileHash)
{
	FDatasmithHash HashName;
	HashName.Update(InMeshFileHash);
	for (VecMaterialSyncData::SizeType i = 0; i < GlobalMaterialsUsed.Num(); ++i)
	{
		HashName.Update(GlobalMaterialsUsed[i]->GetDatasmithId());
	}
	return LexToString(HashName.GetHashValue());
}

#if DUMP_GEOMETRY

// Return a mesh dump
utf8_string FElement2StaticMesh::MeshAsString(const FDatasmithMesh& Mesh)
{
	utf8_string Dump(Utf8StringFormat("Mesh Name = %s\n", TCHAR_TO_UTF8(Mesh.GetName())));

	int32 VerticesCount = Mesh.GetVerticesCount();
	Dump += Utf8StringFormat("\tVertices Count = %d\n", VerticesCount);
	for (int32 IdxVertice = 0; IdxVertice < VerticesCount; ++IdxVertice)
	{
		FVector3f Vertex = Mesh.GetVertex(IdxVertice);
		Dump += Utf8StringFormat("\t\tVertice[%d] = {%f, %f, %f}\n", IdxVertice, Vertex.X, Vertex.Y, Vertex.Z);
	}

	int32 UVChannelCount = Mesh.GetUVChannelsCount();
	Dump += Utf8StringFormat("\tUV Channels Count = %d\n", UVChannelCount);
	for (int32 IdxChannel = 0; IdxChannel < UVChannelCount; ++IdxChannel)
	{
		int32 UVCount = Mesh.GetUVCount(IdxChannel);
		Dump += Utf8StringFormat("\t\tChannels[%d] Count = %d\n", IdxChannel, UVCount);
		for (int32 IdxUV = 0; IdxUV < UVCount; ++IdxUV)
		{
			FVector2D UV = Mesh.GetUV(IdxChannel, IdxUV);
			Dump += Utf8StringFormat("\t\t\tChannels[%d][%d] UV = {%f, %f}\n", IdxChannel, IdxUV, UV.X, UV.Y);
		}
	}

	int32 FacesCount = Mesh.GetFacesCount();
	Dump += Utf8StringFormat("\tFaces Count = %d\n", FacesCount);
	for (int32 IdxFace = 0; IdxFace < FacesCount; ++IdxFace)
	{
		int32 Vertex1;
		int32 Vertex2;
		int32 Vertex3;
		int32 MaterialId;
		Mesh.GetFace(IdxFace, Vertex1, Vertex2, Vertex3, MaterialId);
		Dump += Utf8StringFormat("\t\tVertex[%d] = {%d, %d, %d} Mat = %d\n", IdxFace, Vertex1, Vertex2, Vertex3,
								 MaterialId);
		FVector3f Point1 = Mesh.GetVertex(Vertex1);
		FVector3f Point2 = Mesh.GetVertex(Vertex2);
		FVector3f Point3 = Mesh.GetVertex(Vertex3);
		Dump += Utf8StringFormat("\t\t\t\t{{%f, %f, %f}, {%f, %f, %f}, {%f, %f, %f}}\n", Point1.X, Point1.Y, Point1.Z,
								 Point2.X, Point2.Y, Point2.Z, Point3.X, Point3.Y, Point3.Z);

		for (int32 IdxComponent = 0; IdxComponent < 3; IdxComponent++)
		{
			FVector3f Normal = Mesh.GetNormal(IdxFace * 3 + IdxComponent);
			Dump += Utf8StringFormat("\t\t\tNormal[%d][%d] = {%f, %f, %f}\n", IdxFace, IdxComponent, Normal.X, Normal.Y,
									 Normal.Z);
		}

		for (int32 IdxChannel = 0; IdxChannel < UVChannelCount; ++IdxChannel)
		{
			Mesh.GetFaceUV(IdxFace, IdxChannel, Vertex1, Vertex2, Vertex3);
			FVector2D UV1 = Mesh.GetUV(IdxChannel, Vertex1);
			FVector2D UV2 = Mesh.GetUV(IdxChannel, Vertex2);
			FVector2D UV3 = Mesh.GetUV(IdxChannel, Vertex3);
			Dump += Utf8StringFormat("\t\t\tUV[%d][%d] = {%d, %d, %d} === {{%f, %f}, {%f, %f}, {%f, %f}}\n", IdxFace,
									 IdxChannel, Vertex1, Vertex2, Vertex3, UV1.X, UV1.Y, UV2.X, UV2.Y, UV3.X, UV3.Y);
		}
	}

	return Dump;
}

// Dump a mesh
void FElement2StaticMesh::DumpMesh(const FDatasmithMesh& Mesh)
{
	static bool DoDump = false;
	if (DoDump)
	{
		UE_AC_TraceF("%s", MeshAsString(Mesh).c_str());
	}
}

// Return a mesh element dump
utf8_string FElement2StaticMesh::MeshElementAsString(const IDatasmithMeshElement& Mesh)
{
	utf8_string Dump;

	Dump += Utf8StringFormat("Mesh \"%s\"\n", TCHAR_TO_UTF8(Mesh.GetName()));
	Dump += Utf8StringFormat("\tLabel = \"%s\"\n", TCHAR_TO_UTF8(Mesh.GetLabel()));
	Dump += Utf8StringFormat("\tFile = \"%s\"\n", TCHAR_TO_UTF8(Mesh.GetFile()));
	const FVector3f Dim = Mesh.GetDimensions();
	Dump += Utf8StringFormat("\tDimensions = {%f, %f, %f}\n", Dim.X, Dim.Y, Dim.Z);
	Dump += Utf8StringFormat("\tArea = %f\n", Mesh.GetArea());
	Dump +=
		Utf8StringFormat("\tWidth = %f, Height = %f, Depth = %f\n", Mesh.GetWidth(), Mesh.GetHeight(), Mesh.GetDepth());
	Dump += Utf8StringFormat("\tMaterial Slot Count = %d\n", Mesh.GetMaterialSlotCount());

	return Dump;
}

// Dump a mesh element
void FElement2StaticMesh::DumpMeshElement(const IDatasmithMeshElement& Mesh)
{
	static bool DoDump = false;
	if (DoDump)
	{
		UE_AC_TraceF("%s", MeshElementAsString(Mesh).c_str());
	}
}

#endif

// Create a triangle for polygon vertex ≈ new Triangle(first, previous, last)
void FElement2StaticMesh::AddVertex(GS::Int32 InBodyVertex, const Geometry::Vector3D& VertexNormal)
{
	UE_AC_Assert(InBodyVertex > 0);
	int ObjectVertex = InBodyVertex + StartVertex - 1;

	// Get the vertex value
	FVertex& vertex = Vertices[ObjectVertex];
	if (!vertex.Used)
	{
		// Not already used, get value from body
		CurrentBody.GetVertex(InBodyVertex, &vertex.LocalVertex, ModelerAPI::CoordinateSystem::ElemLocal);

		vertex.LocalVertex.x = vertex.LocalVertex.x + GeometryShift.x;
		vertex.LocalVertex.y = vertex.LocalVertex.y + GeometryShift.y;
		vertex.LocalVertex.z = vertex.LocalVertex.z + GeometryShift.z;

		vertex.LocalVertex.x *= SyncContext.ScaleLength;
		vertex.LocalVertex.y *= SyncContext.ScaleLength;
		vertex.LocalVertex.z *= SyncContext.ScaleLength;

		CurrentBody.GetVertex(InBodyVertex, &vertex.WorldVertex, ModelerAPI::CoordinateSystem::World);
		vertex.Used = true;
	}

	// Get vertex texture coordinate index
	int											 ObjectUV = FTriangle::kInvalidIndex;
	const FMaterialsDatabase::FMaterialSyncData& MatData = *GlobalMaterialsUsed[CurrentTriangle.LocalMatID];
	bool										 bAlwaysSendUV = true;
	if (MatData.bHasTexture || bAlwaysSendUV)
	{
		// Get texture coordinate
		ModelerAPI::TextureCoordinate AcUV;
		try
		{
			CurrentPolygon.GetTextureCoordinate(&vertex.WorldVertex, &AcUV);
		}
		catch (...)
		{
			UE_AC_STAT(++SyncContext.Stats.TotalBugsCount);
			if (BugsCount++ == 0)
			{
				UE_AC_DebugF("FElement2StaticMesh::AddVertex - Exception in GetTextureCoordinate\n");
			}

			AcUV.u = 0;
			AcUV.v = 0;
		}

		// Rotate texture and size the texture (ideally build a material that implement this functionality)
		FTextureCoordinate UV;
		UV.u = AcUV.u; //(MatData.CosAngle * AcUV.u - MatData.SinAngle * AcUV.v) * MatData.InvXSize;
		UV.v = -AcUV.v; //(-MatData.SinAngle * AcUV.u - MatData.CosAngle * AcUV.v) * MatData.InvYSize;
#if PIVOT_0_5_0_5
		UV.u += 0.5;
		UV.v += 0.5;
#endif
		// Convert Vertex coordinate to texture coordinate.
		int* UVFound = UVs.Find(UV);
		if (UVFound == nullptr)
		{
			ObjectUV = (int)UVs.Num();
			UVs.Add(UV, ObjectUV);
			bSomeHasTextures = true;
		}
		else
		{
			ObjectUV = *UVFound;
		}
	}

	FVector3f CurrentNormal(float(VertexNormal.x), -float(VertexNormal.y), float(VertexNormal.z));

	// Create triangles
	if (VertexCount == 0)
	{
		// First polygon vertex is used for all triangles
		CurrentTriangle.V0 = ObjectVertex;
		CurrentTriangle.UV0 = ObjectUV;
		CurrentTriangle.Normals[0] = CurrentNormal;
	}
	else
	{
		if (VertexCount != 1)
		{
			// Third and following vertex
			CurrentTriangle.V2 = ObjectVertex;
			CurrentTriangle.UV2 = ObjectUV;
			CurrentTriangle.Normals[2] = CurrentNormal;
#if defined(DEBUG)
			if (CurrentTriangle.V0 == CurrentTriangle.V1 || CurrentTriangle.V0 == CurrentTriangle.V2 ||
				CurrentTriangle.V1 == CurrentTriangle.V2)
			{
				static bool bVertexConfusedSignaled = false;
				if (!bVertexConfusedSignaled)
				{
					bVertexConfusedSignaled = true;
					UE_AC_DebugF("FElement2StaticMesh::AddVertex - Triangle have 2 vertex confused\n");
				}
			}
			if (CurrentTriangle.UV0 == CurrentTriangle.UV1 || CurrentTriangle.UV0 == CurrentTriangle.UV2 ||
				CurrentTriangle.UV1 == CurrentTriangle.UV2)
			{
				static bool bUVConfusedSignaled = false;
				if (!bUVConfusedSignaled)
				{
					bUVConfusedSignaled = true;
					UE_AC_DebugF("FElement2StaticMesh::AddVertex - Triangle have 2 UV confused\n");
				}
			}
#endif
			Triangles.Add(CurrentTriangle);
		}
		CurrentTriangle.V1 = ObjectVertex;
		CurrentTriangle.UV1 = ObjectUV;
		CurrentTriangle.Normals[1] = CurrentNormal;
	}
	VertexCount++;
}

// Set the material for the current polygon
void FElement2StaticMesh::InitPolygonMaterial()
{
	CurrentPolygon.GetMaterialIndex(MaterialIndex);
	GS::Int32 MaterialIdx = MaterialIndex.GetOriginalModelerIndex();
	CurrentPolygon.GetPolygonTextureIndex(TextureIndex);
	GS::Int32 TextureIdx = TextureIndex.GetOriginalModelerIndex();

	const FMaterialsDatabase::FMaterialSyncData* CurrentMaterial = &SyncContext.GetMaterialsDatabase().GetMaterial(
		SyncContext, MaterialIdx, TextureIdx, bIsSurfaceBody ? kDoubleSide : kSingleSide);

	// Performance heuristic, we presume reuse of previous one
	if (LocalMaterialIndex >= GlobalMaterialsUsed.Num() || GlobalMaterialsUsed[LocalMaterialIndex] != CurrentMaterial)
	{
		for (LocalMaterialIndex = GlobalMaterialsUsed.Num();;)
		{
			if (LocalMaterialIndex == 0)
			{
				// Not found, add it to the vector and quit the loop
				LocalMaterialIndex = GlobalMaterialsUsed.Num();
				GlobalMaterialsUsed.Add(CurrentMaterial);
				break;
			}
			if (GlobalMaterialsUsed[--LocalMaterialIndex] == CurrentMaterial)
			{
				// found, quit the loop
				break;
			}
		}
	}

	UE_AC_Assert(LocalMaterialIndex < GlobalMaterialsUsed.Num() ||
				 GlobalMaterialsUsed[LocalMaterialIndex] == CurrentMaterial);
	CurrentTriangle.LocalMatID = (int)LocalMaterialIndex;
}

void FElement2StaticMesh::AddElementGeometry(const ModelerAPI::Element&		   InModelElement,
											 const Geometry::Vector3D&          InGeometryShift)
{
#if 0
	UE_AC_TraceF("Element\n%s\n", F3DElement2String::Element2String(InModelElement).c_str());
#endif
#if 0
	static GS::Guid BreakGuid("0A0BFAC4-B753-1144-A733-1E73939B3ECB");
	if (InModelElement.GetElemGuid() == BreakGuid)
	{
		UE_AC_DebugF("FElement2StaticMesh::AddElementGeometry - Break element found\n");
	}
#endif
	GeometryShift = InGeometryShift;

	// Collect geometry from element's bodies
	GS::Int32 NbBodies = InModelElement.GetTessellatedBodyCount();
	UE_AC_STAT(SyncContext.Stats.BodiesStats.Inc(NbBodies));

	for (GS::Int32 IndexBody = 1; IndexBody <= NbBodies; IndexBody++)
	{
		InModelElement.GetTessellatedBody(IndexBody, &CurrentBody);
		bIsSurfaceBody = CurrentBody.IsSurfaceBody();
		GS::Int32 NbVertices = CurrentBody.GetVertexCount();
		StartVertex = (int)Vertices.Num();
		Vertices.AddZeroed(NbVertices); // Set space for bodies vertex

		// Collect triangles from body's polygon
		GS::Int32 NbPolygons = CurrentBody.GetPolygonCount();
		UE_AC_STAT(SyncContext.Stats.PolygonsStats.Inc(NbPolygons));

		for (GS::Int32 IndexPolygon = 1; IndexPolygon <= NbPolygons; IndexPolygon++)
		{
			CurrentBody.GetPolygon(IndexPolygon, &CurrentPolygon);
			if (!CurrentPolygon.IsInvisible())
			{
				// Cutting plan create invisible polygons contour where it cut. So, we must not export theses polygons
				InitPolygonMaterial();
				if (CurrentPolygon.IsComplex())
				{
					UE_AC_STAT(++SyncContext.Stats.PolygonsComplex);
					// For a complex polygon we decompose it in convex polygons
					GS::Int32 NbPolys = CurrentPolygon.GetConvexPolygonCount();
					for (GS::Int32 i = 1; i <= NbPolys; i++)
					{
						// For all convex polygons
						UE_AC_STAT(++SyncContext.Stats.PolygonsConvex);
						ModelerAPI::ConvexPolygon ConvexPolygon;
						CurrentPolygon.GetConvexPolygon(i, &ConvexPolygon);
						GS::Int32 NbVerts = ConvexPolygon.GetVertexCount();
						VertexCount = 0; // Start polygon triangulation
						for (GS::Int32 j = 1; j <= NbVerts; j++)
						{
							AddVertex(ConvexPolygon.GetVertexIndex(j),
									  Vertor2Vector3D(ConvexPolygon.GetNormalVectorByVertex(
										  j, ModelerAPI::CoordinateSystem::ElemLocal)));
						}
					}
				}
				else
				{
					VertexCount = 0; // Start polygon triangulation
					GS::Int32 NbEdges = CurrentPolygon.GetEdgeCount();
					for (GS::Int32 IndexPedg = 1; IndexPedg <= NbEdges; IndexPedg++)
					{
						AddVertex(CurrentPolygon.GetVertexIndex(IndexPedg),
								  Vertor2Vector3D(CurrentPolygon.GetNormalVectorByVertex(
									  IndexPedg, ModelerAPI::CoordinateSystem::ElemLocal)));
					}
				}
			}
		}
	}
	if (!HasGeometry())
	{
		UE_AC_VerboseF("FElement2StaticMesh::AddElementGeometry - No material used for element %s\n",
					   InModelElement.GetElemGuid().ToUniString().ToUtf8());
	}
}

// Validate Vertice
inline void ValidateVertice(int32 InIndex, const FElement2StaticMesh::VecVertices& InVertices, int32 InVertexUsedCount)
{
	UE_AC_Assert(InIndex >= 0 && InIndex < InVertices.Num() && InVertices[InIndex].Index >= 0 &&
				 InVertices[InIndex].Index < InVertexUsedCount);
}

// Fill 3d maesh data from collected goemetry
void FElement2StaticMesh::FillMesh(FDatasmithMesh* OutMesh)
{
	// Count used vertices and set new index value
	int32				  VertexUsedCount = 0;
	VecVertices::SizeType NbVertices = Vertices.Num();
	VecVertices::SizeType IndexVertex;
	for (IndexVertex = 0; IndexVertex < NbVertices; IndexVertex++)
	{
		FVertex& vertex = Vertices[IndexVertex];
		if (vertex.Used)
		{
			vertex.Index = VertexUsedCount++;
		}
		else
		{
			vertex.Index = FVertex::kInvalidVertex;
		}
	}

	// Copy all used vertices
	OutMesh->SetVerticesCount(VertexUsedCount);
	UE_AC_STAT(SyncContext.Stats.TotalTrianglePts += VertexUsedCount);
	for (IndexVertex = 0; IndexVertex < NbVertices; IndexVertex++)
	{
		FVertex& vertex = Vertices[IndexVertex];
		if (vertex.Index != FVertex::kInvalidVertex)
		{
			OutMesh->SetVertex(vertex.Index, (float)vertex.LocalVertex.x, -(float)vertex.LocalVertex.y,
							   (float)vertex.LocalVertex.z);
		}
	}

	// Create a UV channel and fill it
	int32 UVChannel = OutMesh->GetUVChannelsCount();
	UE_AC_Assert(UVChannel == 0); // Must be 0
	OutMesh->AddUVChannel();
	OutMesh->SetUVCount(UVChannel, int32(UVs.Num()));
	UE_AC_STAT(SyncContext.Stats.TotalUVPts += int32(UVs.Num()));
	for (const TPair< FTextureCoordinate, int >& ItUV : UVs)
	{
		OutMesh->SetUV(UVChannel, ItUV.Value, ItUV.Key.u, ItUV.Key.v);
	}

	// Count valid triangles
	int32				   TrianglesValidCount = 0;
	VecTriangles::SizeType NbTriangles = Triangles.Num();
	VecTriangles::SizeType IndexTriangle;
	for (IndexTriangle = 0; IndexTriangle < NbTriangles; IndexTriangle++)
	{
		if (Triangles[IndexTriangle].IsValid())
		{
			TrianglesValidCount++;
		}
	}

	// Copy triangles to face, normals and UV
	UE_AC_STAT(SyncContext.Stats.TotalTriangles += TrianglesValidCount);
	OutMesh->SetFacesCount(TrianglesValidCount);
	int32 IndexFace = 0;
	for (IndexTriangle = 0; IndexTriangle < NbTriangles; IndexTriangle++)
	{
		const FTriangle& triangle = Triangles[IndexTriangle];
		if (triangle.IsValid())
		{
#if 0
			ValidateVertice(triangle.V0, Vertices, VertexUsedCount);
			ValidateVertice(triangle.V1, Vertices, VertexUsedCount);
			ValidateVertice(triangle.V2, Vertices, VertexUsedCount);
#endif
			OutMesh->SetFace(IndexFace, Vertices[triangle.V0].Index, Vertices[triangle.V1].Index,
							 Vertices[triangle.V2].Index, triangle.LocalMatID);

			for (int32 IndexComponent = 0; IndexComponent < 3; IndexComponent++)
			{
				const FVector3f& Normal = triangle.Normals[IndexComponent];
				OutMesh->SetNormal(IndexFace * 3 + IndexComponent, Normal.X, Normal.Y, Normal.Z);
			}

			OutMesh->SetFaceUV(IndexFace, UVChannel, triangle.UV0, triangle.UV1, triangle.UV2);

			IndexFace++;
		}
	}
}

// Create a datasmith mesh element
TSharedPtr< IDatasmithMeshElement > FElement2StaticMesh::CreateMesh()
{
	TSharedPtr< IDatasmithMeshElement > MeshElement;

	FDatasmithMesh Mesh;

	FillMesh(&Mesh);

	FDatasmithHash MeshHasher;
	MeshHasher.ComputeDatasmithMeshHash(Mesh);
	FMD5Hash MeshHash = MeshHasher.GetHashValue();
	Mesh.SetName(*LexToString(MeshHash));

	const FMeshDimensions* Dimensions = SyncContext.GetSyncDatabase().GetMeshIndexor().FindMesh(Mesh.GetName());

	FString MeshElementName = ComputeMeshElementName(Mesh.GetName());

#if DUMP_GEOMETRY
	DumpMesh(Mesh);
#endif

	// Define output path
	TCHAR	SubDir1[2] = {Mesh.GetName()[0], 0};
	TCHAR	SubDir2[2] = {Mesh.GetName()[1], 0};
	FString OutputPath(FPaths::Combine(SyncContext.GetSyncDatabase().GetAssetsFolderPath(), SubDir1, SubDir2));

	// If mesh already exist ?
	FString FullPath(FPaths::Combine(OutputPath, FPaths::SetExtension(Mesh.GetName(), TEXT("udsmesh"))));
	if (Dimensions == nullptr || !FPaths::FileExists(FullPath))
	{
		// Create a new mesh file
		UE_AC_STAT(SyncContext.Stats.TotalMeshesCreated++);
		FDatasmithMeshExporter MeshExporter;
		MeshElement =
			MeshExporter.ExportToUObject(*OutputPath, Mesh.GetName(), Mesh, nullptr, EDSExportLightmapUV::Never);
		if (MeshElement.IsValid())
		{
			MeshElement->SetName(*MeshElementName);
			MeshElement->SetFileHash(MeshHash);
			SyncContext.GetSyncDatabase().GetMeshIndexor().AddMesh(*MeshElement);
		}
	}
	else
	{
		// Reuse the previous mesh file
		UE_AC_STAT(SyncContext.Stats.TotalMeshesReused++);
		MeshElement = FDatasmithSceneFactory::CreateMesh(*MeshElementName);
		MeshElement->SetFile(*FullPath);
		MeshElement->SetFileHash(MeshHash);
		MeshElement->SetDimensions(Dimensions->Area, Dimensions->Width, Dimensions->Height, Dimensions->Depth);
	}

	if (MeshElement.IsValid())
	{
		for (VecMaterialSyncData::SizeType i = 0; i < GlobalMaterialsUsed.Num(); ++i)
		{
			MeshElement->SetMaterial(*GlobalMaterialsUsed[i]->GetDatasmithId(), (int32)i);
		}

#if DUMP_GEOMETRY
        DumpMeshElement(*MeshElement);
#endif
	}

	return MeshElement;
}

END_NAMESPACE_UE_AC
