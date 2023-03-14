// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace GLTF
{
	struct GLTFCORE_API FImage
	{
		enum class EFormat
		{
			Unknown,
			PNG,
			JPEG
		};

		FString Name;
		FString URI;
		EFormat Format;
		FString FilePath;

		// Image data is kept encoded in Format, to be decoded when needed by Unreal.
		uint32       DataByteLength;
		const uint8* Data;

		FImage()
		    : DataByteLength(0)
		    , Data(nullptr)
		{
		}
	};

	struct GLTFCORE_API FSampler
	{
		enum class EFilter
		{
			// valid for Min & Mag
			Nearest = 9728,
			Linear  = 9729,
			// valid for Min only
			NearestMipmapNearest = 9984,
			LinearMipmapNearest  = 9985,
			NearestMipmapLinear  = 9986,
			LinearMipmapLinear   = 9987
		};

		enum class EWrap
		{
			Repeat         = 10497,
			MirroredRepeat = 33648,
			ClampToEdge    = 33071
		};

		static const FSampler DefaultSampler;

		EFilter MinFilter;
		EFilter MagFilter;

		EWrap WrapS;
		EWrap WrapT;

		FSampler()
		    // Spec defines no default min/mag filter.
		    : MinFilter(EFilter::Linear)
		    , MagFilter(EFilter::Linear)
		    , WrapS(EWrap::Repeat)
		    , WrapT(EWrap::Repeat)
		{
		}
	};

	struct GLTFCORE_API FTexture
	{
		const FImage&   Source;
		const FSampler& Sampler;
		FString         Name;

		FTexture(const FString& InName, const FImage& InSource, const FSampler& InSampler)
		    : Source(InSource)
		    , Sampler(InSampler)
		    , Name(InName)
		{
		}
	};

}  // namespace GLTF
