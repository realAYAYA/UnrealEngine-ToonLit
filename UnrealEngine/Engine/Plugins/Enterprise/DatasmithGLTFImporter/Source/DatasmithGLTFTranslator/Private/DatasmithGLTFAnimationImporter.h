// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFAnimation.h"
#include "GLTFLogger.h"

#include "Misc/FrameRate.h"

class IDatasmithScene;
class IDatasmithTransformAnimationElement;
class IDatasmithLevelSequenceElement;
namespace GLTF
{
	struct FAsset;
}

class FDatasmithGLTFAnimationImporter : FNoncopyable
{
public:
	IDatasmithScene* CurrentScene;

public:
	FDatasmithGLTFAnimationImporter(TArray<GLTF::FLogMessage>& LogMessages);

	void CreateAnimations(const GLTF::FAsset& GLTFAsset, bool bAnimationFPSFromFile);

	const TArray<TSharedRef<IDatasmithLevelSequenceElement>>& GetImportedSequences() { return ImportedSequences; }

	void SetUniformScale(float Scale);

private:
	uint32 ResampleAnimationFrames(const GLTF::FAnimation& Animation,
		const TArray<GLTF::FAnimation::FChannel>& Channels,
		FFrameRate FrameRate,
		IDatasmithTransformAnimationElement& AnimationElement);

private:
	using FNodeChannelMap = TMap<const GLTF::FNode*, TArray<GLTF::FAnimation::FChannel> >;

	TArray<TSharedRef<IDatasmithLevelSequenceElement>> ImportedSequences;
	TArray<GLTF::FLogMessage>& LogMessages;
	FNodeChannelMap            NodeChannelMap;
	TArray<float>              FrameTimeBuffer;
	TArray<float>              FrameDataBuffer;
	float                      ScaleFactor;
};

inline void FDatasmithGLTFAnimationImporter::SetUniformScale(float InScale)
{
	ScaleFactor = InScale;
}
