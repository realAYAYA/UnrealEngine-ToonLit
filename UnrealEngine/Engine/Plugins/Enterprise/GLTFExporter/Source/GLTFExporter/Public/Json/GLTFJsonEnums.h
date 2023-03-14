// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EGLTFJsonExtension
{
	KHR_LightsPunctual,
	KHR_MaterialsClearCoat,
	KHR_MaterialsUnlit,
	KHR_MaterialsVariants,
	KHR_MeshQuantization,
	KHR_TextureTransform,
	EPIC_AnimationPlayback,
	EPIC_BlendModes,
	EPIC_HDRIBackdrops,
	EPIC_LevelVariantSets,
	EPIC_LightmapTextures,
	EPIC_SkySpheres,
	EPIC_TextureHDREncoding
};

enum class EGLTFJsonShadingModel
{
	None = -1,
	Default,
	Unlit,
	ClearCoat,

	NumShadingModels
};

enum class EGLTFJsonHDREncoding
{
	None = -1,
	RGBM,
	RGBE
};

enum class EGLTFJsonCubeFace
{
	None = -1,
	PosX,
	NegX,
	PosY,
	NegY,
	PosZ,
	NegZ
};

enum class EGLTFJsonAccessorType
{
	None = -1,
	Scalar,
	Vec2,
	Vec3,
	Vec4,
	Mat2,
	Mat3,
	Mat4
};

enum class EGLTFJsonComponentType
{
	None = -1,
	Int8 = 5120,
	UInt8 = 5121,
	Int16 = 5122,
	UInt16 = 5123,
	Int32 = 5124, // unused
	UInt32 = 5125,
	Float = 5126
};

enum class EGLTFJsonBufferTarget
{
	None = -1,
	ArrayBuffer = 34962,
	ElementArrayBuffer = 34963
};

enum class EGLTFJsonPrimitiveMode
{
	Points = 0,
	Lines = 1,
	LineLoop = 2,
	LineStrip = 3,
	Triangles = 4,
	TriangleStrip = 5,
	TriangleFan = 6
};

enum class EGLTFJsonAlphaMode
{
	None = -1,
	Opaque,
	Mask,
	Blend
};

enum class EGLTFJsonBlendMode
{
	None = -1,
	Additive,
	Modulate,
	AlphaComposite
};

enum class EGLTFJsonMimeType
{
	None = -1,
	PNG,
	JPEG
};

enum class EGLTFJsonTextureFilter
{
	None = -1,
	Nearest = 9728,
	Linear = 9729,
	NearestMipmapNearest = 9984,
	LinearMipmapNearest = 9985,
	NearestMipmapLinear = 9986,
	LinearMipmapLinear = 9987
};

enum class EGLTFJsonTextureWrap
{
	None = -1,
	Repeat = 10497,
	MirroredRepeat = 33648,
	ClampToEdge = 33071
};

enum class EGLTFJsonInterpolation
{
	None = -1,
	Linear,
	Step,
	CubicSpline,
};

enum class EGLTFJsonTargetPath
{
	None = -1,
	Translation,
	Rotation,
	Scale,
	Weights
};

enum class EGLTFJsonCameraType
{
	None = -1,
	Orthographic,
	Perspective
};

enum class EGLTFJsonLightType
{
	None = -1,
	Directional,
	Point,
	Spot
};
