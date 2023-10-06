// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Math/Box.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"

#include "Misc/SecureHash.h"


class DATASMITHCORE_API FDatasmithMesh
{
public:
	FDatasmithMesh();
	~FDatasmithMesh();

	FDatasmithMesh( const FDatasmithMesh& Other );
	FDatasmithMesh( FDatasmithMesh&& Other );

	FDatasmithMesh& operator=( const FDatasmithMesh& Other );
	FDatasmithMesh& operator=( FDatasmithMesh&& Other );

	// Calculate mesh data hash(doesn't include name)
	FMD5Hash CalculateHash() const;

	void SetName(const TCHAR* InName);
	const TCHAR* GetName() const;

	//--------------------------
	// Faces
	//--------------------------
	/** Setting the amount of faces is mandatory before filling the array */
	void SetFacesCount(int32 NumFaces);

	/** Retrieves the amount of faces */
	int32 GetFacesCount() const;

	/**
	 * Sets the geometry of the face
	 *
	 * @param Index		value to choose the face that will be affected
	 * @param Vertex1	Index of the first geometric vertex that defines this face
	 * @param Vertex2	Index of the second geometric vertex that defines this face
	 * @param Vertex3	Index of the third geometric vertex that defines this face
	 */
	void SetFace(int32 Index, int32 Vertex1, int32 Vertex2, int32 Vertex3, int32 MaterialId = 0);
	void GetFace(int32 Index, int32& Vertex1, int32& Vertex2, int32& Vertex3, int32& MaterialId) const;

	/**
	 * Sets the smoothing mask of the face
	 *
	 * @param Index			Index of the face that will be affected
	 * @param SmoothingMask	32 bits mask, 0 means no smoothing
	 */
	void SetFaceSmoothingMask(int32 Index, uint32 SmoothingMask);

	/**
	 * Gets the smoothing mask of a face
	 *
	 * @param Index		Index of the face to retrieve the smoothing mask from
	 */
	uint32 GetFaceSmoothingMask(int32 Index) const;

	int32 GetMaterialsCount() const;
	bool IsMaterialIdUsed(int32 MaterialId) const;

	//--------------------------
	// Vertices
	//--------------------------
	/** Setting the amount of geometric vertices is mandatory before filling the array */
	void SetVerticesCount(int32 NumVerts);

	/** Retrieves the amount of geometric vertices. The validity of the vertex data is not guaranteed */
	int32 GetVerticesCount() const;

	/**
	 * Sets the 3d position of the vertex
	 *
	 * @param Index value to choose the vertex that will be affected
	 * @x position on the x axis
	 * @y position on the y axis
	 * @z position on the z axis
	 */
	void SetVertex(int32 Index, float X, float Y, float Z);
	FVector3f GetVertex(int32 Index) const;

	//--------------------------
	// Normals
	//--------------------------
	/**
	 * Sets the 3d normal
	 *
	 * @param Index value to choose the normal that will be affected
	 * @x direction on the x axis
	 * @y direction on the y axis
	 * @z direction on the z axis
	 */
	void SetNormal(int32 Index, float X, float Y, float Z);
	FVector3f GetNormal(int32 Index) const;

	//--------------------------
	// UVs
	//--------------------------
	/**
	 * Sets the amount of UV channels on this mesh
	 *
	 * @param ChannelCount	The number of UV channels
	 */
	void SetUVChannelsCount(int32 ChannelCount);

	/**
	 * Add a UV channel at the end
	 */
	void AddUVChannel();

	/**
	 * Remove the last UV channel
	 */
	void RemoveUVChannel();

	/** Gets the amount of UV channels on this mesh */
	int32 GetUVChannelsCount() const;

	/**
	 * Setting the amount of UV coordinates on the channel is mandatory before filling the array
	 *
	 * @param Channel	The channel to set the numbers of UVs
	 * @param NumVerts	The number of UVs in the channel
	 */
	void SetUVCount(int32 Channel, int32 NumVerts);

	/** Retrieves the amount of UV coordinates on the channel. The validity of the vertex data is not guaranteed */
	int32 GetUVCount(int32 Channel) const;

	/**
	 * Sets the 2d position of the UV vertex for the first uv mapping
	 *
	 * @param Index	Value to choose the vertex that will be affected
	 * @param U		Horizontal coordinate
	 * @param V		Vertical coordinate
	 */
	void SetUV(int32 Channel, int32 Index, double U, double V);

	/**
	 * Get the hash for a uv channel
	 */
	uint32 GetHashForUVChannel(int32 Channel) const;

	/**
	 * Gets the UV coordinates for a channel
	 *
	 * @param Channel	The channel we want to retrieve the UVs from
	 * @param Index		The UV coordinates to retrieve
	 */
	FVector2D GetUV(int32 Channel, int32 Index) const;

	/**
	 * Sets the channel UV coordinates of the face
	 *
	 * @param Index		Index of the face that will be affected
	 * @param Channel	UV channel, starting at 0
	 * @param Vertex1	Index of the first uv vertex that defines this face
	 * @param Vertex2	Index of the second uv vertex that defines this face
	 * @param Vertex3	Index of the third uv vertex that defines this face
	 */
	void SetFaceUV(int32 Index, int32 Channel, int32 Vertex1, int32 Vertex2, int32 Vertex3);

	/**
	 * Gets the UV coordinates of the vertices of a face
	 *
	 * @param Index		Index of the face to retrieve the UVs from
	 * @param Channel	UV channel, starting at 0
	 * @param Vertex1	Index of the first uv vertex that defines this face
	 * @param Vertex2	Index of the second uv vertex that defines this face
	 * @param Vertex3	Index of the third uv vertex that defines this face
	 */
	void GetFaceUV(int32 Index, int32 Channel, int32& Vertex1, int32& Vertex2, int32& Vertex3) const;

	/** Get the number of vertex color */
	int32 GetVertexColorCount() const;

	/**
	 * Set a vertex color
	 *
	 * @param Index		Index of the vertex color that will be affected
	 * @param Color		The color for the vertex
	 */
	void SetVertexColor(int32 Index, const FColor& Color);

	/**
	 * Get the color for a vertex
	 *
	 * @param Index		Index of the vertex color to retrieve
	 */
	FColor GetVertexColor(int32 Index) const;

	/**
	 * Sets the UV channel that will be used as the source UVs for lightmap UVs generation at import, defaults to channel 0.
	 * Will be overwritten during Mesh export if we choose to generate the lightmap source UVs.
	 */
	void SetLightmapSourceUVChannel(int32 Channel);

	/** Gets the UV channel that will be used for lightmap UVs generation at import */
	int32 GetLightmapSourceUVChannel() const;

	//--------------------------
	// LODs
	//--------------------------

	/** Adds a LOD mesh to this base LOD mesh */
	void AddLOD( const FDatasmithMesh& InLODMesh );
	void AddLOD( FDatasmithMesh&& InLODMesh );
	int32 GetLODsCount() const;

	/** Gets the FDatasmithMesh LOD at the given index, if the index is invalid returns nullptr */
	FDatasmithMesh* GetLOD( int32 Index );
	const FDatasmithMesh* GetLOD( int32 Index ) const;

	//--------------------------
	// Misc
	//--------------------------
	/** Returns the total surface area */
	float ComputeArea() const;

	/** Returns the bounding box containing all vertices of this mesh */
	FBox3f GetExtents() const;

private:
	class FDatasmithMeshImpl;

	FDatasmithMeshImpl* Impl;
};
