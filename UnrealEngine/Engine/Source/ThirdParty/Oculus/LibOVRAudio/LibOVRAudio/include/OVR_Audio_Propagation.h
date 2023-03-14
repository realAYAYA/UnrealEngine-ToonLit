/********************************************************************************//**
\file      OVR_Audio_Propagation.h
\brief     OVR Audio SDK public header file
\copyright Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.
************************************************************************************/
#ifndef OVR_Audio_Propagation_h
#define OVR_Audio_Propagation_h

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

  #pragma pack(push, 1) // Make sure all structs are packed for consistency.

    /***********************************************************************************/
    /* Geometry API */
	
	/** \brief A handle to a material that applies filtering to reflected and transmitted sound. 0/NULL/nullptr represent an invalid handle. */
    typedef struct ovrAudioMaterial_* ovrAudioMaterial;
	
	/** \brief An enumeration of the scalar types supported for geometry data. */
    typedef enum
    {
        ovrAudioScalarType_Int8,
        ovrAudioScalarType_UInt8,
        ovrAudioScalarType_Int16,
        ovrAudioScalarType_UInt16,
        ovrAudioScalarType_Int32,
        ovrAudioScalarType_UInt32,
        ovrAudioScalarType_Int64,
        ovrAudioScalarType_UInt64,
        ovrAudioScalarType_Float16,
        ovrAudioScalarType_Float32,
        ovrAudioScalarType_Float64,
    } ovrAudioScalarType;

	/** \brief The type of mesh face that is used to define geometry.
	  *
	  * For all face types, the vertices should be provided such that they are in counter-clockwise
	  * order when the face is viewed from the front. The vertex order is used to determine the
	  * surface normal orientation.
	  */
    typedef enum
    {
		/** \brief A face type that is defined by 3 vertex indices. */
        ovrAudioFaceType_Triangles = 0,
		/** \brief A face type that is defined by 4 vertex indices. The vertices are assumed to be coplanar. */
        ovrAudioFaceType_Quads = 1,
        ovrAudioFaceType_COUNT
    } ovrAudioFaceType;

	/** \brief The properties for audio materials. All properties are frequency dependent. */
	typedef enum
	{
		/** \brief The fraction of sound arriving at a surface that is absorbed by the material.
		  *
		  * This value is in the range 0 to 1, where 0 indicates a perfectly reflective material, and
		  * 1 indicates a perfectly absorptive material. Absorption is inversely related to the reverberation time,
		  * and has the strongest impact on the acoustics of an environment. The default absorption is 0.1.
		  */
		ovrAudioMaterialProperty_Absorption = 0,
		/** \brief The fraction of sound arriving at a surface that is transmitted through the material.
		  *
		  * This value is in the range 0 to 1, where 0 indicates a material that is acoustically opaque,
		  * and 1 indicates a material that is acoustically transparent.
		  * To preserve energy in the simulation, the following condition must hold: (1 - absorption + transmission) <= 1
		  * If this condition is not met, the transmission and absorption coefficients will be modified to
		  * enforce energy conservation. The default transmission is 0.
		  */
		ovrAudioMaterialProperty_Transmission = 1,
		/** \brief The fraction of sound arriving at a surface that is scattered.
		  *
		  * This property in the range 0 to 1 controls how diffuse the reflections are from a surface,
		  * where 0 indicates a perfectly specular reflection and 1 indicates a perfectly diffuse reflection.
		  * The default scattering is 0.5.
		  */
		ovrAudioMaterialProperty_Scattering = 2,
		ovrAudioMaterialProperty_COUNT
	} ovrAudioMaterialProperty;
	
	/** \brief A struct that is used to provide the vertex data for a mesh. */
    typedef struct
    {
        /** \brief A pointer to a buffer of vertex data with the format described in this structure. This cannot be null. */
        const void* vertices;
        /** \brief The offset in bytes of the 0th vertex within the buffer. */
        size_t byteOffset;
        /** \brief The number of vertices that are contained in the buffer. */
        size_t vertexCount;
        /** \brief If non-zero, the stride in bytes between consecutive vertices. */
        size_t vertexStride;
        /** \brief The primitive type of vertex coordinates. Each vertex is defined by 3 consecutive values of this type. */
        ovrAudioScalarType vertexType;
    } ovrAudioMeshVertices;
	
	/** \brief A struct that is used to provide the index data for a mesh. */
    typedef struct
    {
        /** \brief A pointer to a buffer of index data with the format described in this structure. This cannot be null. */
        const void* indices;
        /** \brief The offset in bytes of the 0th index within the buffer. */
        size_t byteOffset;
        /** \brief The total number of indices that are contained in the buffer. */
        size_t indexCount;
        /** \brief The primitive type of the indices in the buffer. This must be an integer type. */
        ovrAudioScalarType indexType;
    } ovrAudioMeshIndices;
	
	/** \brief A struct that defines a grouping of mesh faces and the material that should be applied to the faces. */
    typedef struct
    {
		/** \brief The offset in the index buffer of the first index in the group. */
        size_t indexOffset;
		/** \brief The number of faces that this group uses from the index buffer.
		  *
		  * The number of bytes read from the index buffer for the group is determined by the formula: (faceCount)*(verticesPerFace)*(bytesPerIndex)
		  */
        size_t faceCount;
		/** \brief The type of face that the group uses. This determines how many indices are needed to define a face. */
        ovrAudioFaceType faceType;
		/** \brief A handle to the material that should be assigned to the group. If equal to 0/NULL/nullptr, a default material is used instead. */
        ovrAudioMaterial material;
    } ovrAudioMeshGroup;
	
	/** \brief A struct that completely defines an audio mesh. */
    typedef struct
    {
		/** \brief The vertices that the mesh uses. */
        ovrAudioMeshVertices vertices;
		/** \brief The indices that the mesh uses. */
        ovrAudioMeshIndices indices;
		/** \brief A pointer to an array of ovrAudioMeshGroup that define the material groups in the mesh.
		  *
		  * The size of the array must be at least groupCount. This cannot be null.
		  */
        const ovrAudioMeshGroup* groups;
		/** \brief The number of groups that are part of the mesh. */
        size_t groupCount;
    } ovrAudioMesh;
	
    /** \brief A handle to geometry that sound interacts with. 0/NULL/nullptr represent an invalid handle. */
    typedef struct ovrAudioGeometry_* ovrAudioGeometry;
	
    /** \brief A function pointer that reads bytes from an arbitrary source and places them into the output byte array.
	  *
	  * The function should return the number of bytes that were successfully read, or 0 if there was an error.
	  */
    typedef size_t(*ovrAudioSerializerReadCallback)(void* userData, void* bytes, size_t byteCount);
    /** \brief A function pointer that writes bytes to an arbitrary destination.
	  *
	  * The function should return the number of bytes that were successfully written, or 0 if there was an error.
	  */
    typedef size_t(*ovrAudioSerializerWriteCallback)(void* userData, const void* bytes, size_t byteCount);
    /** \brief A function pointer that seeks within the data stream.
	  *
	  * The function should seek by the specified signed offset relative to the current stream position.
	  * The function should return the actual change in stream position. Return 0 if there is an error or seeking is not supported.
	  */
    typedef int64_t(*ovrAudioSerializerSeekCallback)(void* userData, int64_t seekOffset);

    /** \brief A structure that contains function pointers to reading/writing data to an arbitrary source/sink. */
    typedef struct
    {
		/** \brief A function pointer that reads bytes from an arbitrary source. This pointer may be null if only writing is required. */
        ovrAudioSerializerReadCallback read;
		/** \brief A function pointer that writes bytes to an arbitrary destination. This pointer may be null if only reading is required. */
        ovrAudioSerializerWriteCallback write;
		/** \brief A function pointer that seeks within the data stream. This pointer may be null if seeking is not supported. */
        ovrAudioSerializerSeekCallback seek;
		/** \brief A pointer to user-defined data that will be passed in as the first argument to the serialization functions. */
        void* userData;
    } ovrAudioSerializer;

  #pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // OVR_Audio_Propagation_h
