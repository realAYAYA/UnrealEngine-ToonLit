// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"
#include "Converters/GLTFMeshSection.h"

class FPositionVertexBuffer;
class FColorVertexBuffer;
class FStaticMeshVertexBuffer;
class FSkinWeightVertexBuffer;
class FGLTFNormalArray;
class FGLTFUVArray;

typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*, const FPositionVertexBuffer*> IGLTFPositionBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*, const FColorVertexBuffer*> IGLTFColorBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*, const FStaticMeshVertexBuffer*> IGLTFNormalBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*, const FStaticMeshVertexBuffer*> IGLTFTangentBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*, const FStaticMeshVertexBuffer*, uint32> IGLTFUVBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*, const FSkinWeightVertexBuffer*, uint32> IGLTFBoneIndexBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*, const FSkinWeightVertexBuffer*, uint32> IGLTFBoneWeightBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFMeshSection*> IGLTFIndexBufferConverter;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FPositionVertexBuffer*> IGLTFPositionBufferConverterRaw;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFIndexArray*, FString> IGLTFIndexBufferConverterRaw;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFNormalArray*> IGLTFNormalBufferConverterRaw;
typedef TGLTFConverter<FGLTFJsonAccessor*, const FGLTFUVArray*> IGLTFUVBufferConverterRaw;


class GLTFEXPORTER_API FGLTFPositionBufferConverter : public FGLTFBuilderContext, public IGLTFPositionBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FPositionVertexBuffer* VertexBuffer) override;
};

class GLTFEXPORTER_API FGLTFColorBufferConverter : public FGLTFBuilderContext, public IGLTFColorBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FColorVertexBuffer* VertexBuffer) override;
};

class GLTFEXPORTER_API FGLTFNormalBufferConverter : public FGLTFBuilderContext, public IGLTFNormalBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer) override;

private:

	template <typename DestinationType, typename SourceType>
	FGLTFJsonBufferView* ConvertBufferView(const FGLTFMeshSection* MeshSection, const void* TangentData);
};

class GLTFEXPORTER_API FGLTFTangentBufferConverter : public FGLTFBuilderContext, public IGLTFTangentBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer) override;

private:

	template <typename DestinationType, typename SourceType>
	FGLTFJsonBufferView* ConvertBufferView(const FGLTFMeshSection* MeshSection, const void* TangentData);
};

class GLTFEXPORTER_API FGLTFUVBufferConverter : public FGLTFBuilderContext, public IGLTFUVBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer, uint32 UVIndex) override;

private:

	template <typename SourceType>
	FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FStaticMeshVertexBuffer* VertexBuffer, uint32 UVIndex, const uint8* SourceData) const;
};

class GLTFEXPORTER_API FGLTFBoneIndexBufferConverter : public FGLTFBuilderContext, public IGLTFBoneIndexBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset) override;

private:

	template <typename DestinationType>
	FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData) const;

	template <typename DestinationType, typename SourceType>
	FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData) const;

	template <typename DestinationType, typename SourceType, typename CallbackType>
	FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData, CallbackType GetVertexInfluenceOffsetCount) const;
};

class GLTFEXPORTER_API FGLTFBoneWeightBufferConverter : public FGLTFBuilderContext, public IGLTFBoneWeightBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset) override;

private:

	template <typename BoneIndexType>
	FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData) const;

	template <typename BoneIndexType, typename CallbackType>
	FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection, const FSkinWeightVertexBuffer* VertexBuffer, uint32 InfluenceOffset, const uint8* SourceData, CallbackType GetVertexInfluenceOffsetCount) const;
};

class GLTFEXPORTER_API FGLTFIndexBufferConverter : public FGLTFBuilderContext, public IGLTFIndexBufferConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFMeshSection* MeshSection) override;
};


class GLTFEXPORTER_API FGLTFPositionBufferConverterRaw : public FGLTFBuilderContext, public IGLTFPositionBufferConverterRaw
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FPositionVertexBuffer* VertexBuffer) override;
};

class GLTFEXPORTER_API FGLTFIndexBufferConverterRaw : public FGLTFBuilderContext, public IGLTFIndexBufferConverterRaw
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:
	virtual FGLTFJsonAccessor* Convert(const FGLTFIndexArray* IndexBuffer, FString MeshName) override;
};

class GLTFEXPORTER_API FGLTFNormalBufferConverterRaw : public FGLTFBuilderContext, public IGLTFNormalBufferConverterRaw
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFNormalArray* NormalSource) override;

private:

	FGLTFJsonBufferView* ConvertBufferView(const FGLTFNormalArray* NormalSource);
};

class GLTFEXPORTER_API FGLTFUVBufferConverterRaw : public FGLTFBuilderContext, public IGLTFUVBufferConverterRaw
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonAccessor* Convert(const FGLTFUVArray* UVSource) override;

private:

	FGLTFJsonBufferView* ConvertBufferView(const FGLTFUVArray* UVSource);
};