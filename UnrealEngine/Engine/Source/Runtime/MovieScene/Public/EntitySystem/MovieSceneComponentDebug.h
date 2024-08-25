// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "Math/Vector4.h"
#include "Misc/FrameTime.h"
#include "UObject/ObjectKey.h"

#if UE_MOVIESCENE_ENTITY_DEBUG

class FName;

namespace UE
{
namespace MovieScene
{

/** Defines a static type identifier for the natvis visualizer */
enum class EComponentDebugType
{
	Unknown,
	Bool,
	Uint8,
	int16,
	Uint16,
	Int32,
	Float,
	Double,
	Vector2,
	Vector3,
	Vector4,
	FrameTime,
	ObjectKey,
	Object,
	Property,
	InstanceHandle,
	SequenceID,
	EntityID,
	Name,
	GroupID,
	BlendChannelID,
};

/**
 * Debug information for a component type
 */
struct FComponentTypeDebugInfo
{
	FString DebugName;
	const TCHAR* DebugTypeName = nullptr;
	EComponentDebugType Type = EComponentDebugType::Unknown;
};

template<typename T> struct TComponentDebugType                            { static const EComponentDebugType Type = EComponentDebugType::Unknown;           };
template<>           struct TComponentDebugType<bool>                      { static const EComponentDebugType Type = EComponentDebugType::Bool;              };
template<>           struct TComponentDebugType<uint8>                     { static const EComponentDebugType Type = EComponentDebugType::Uint8;             };
template<>           struct TComponentDebugType<int16>                     { static const EComponentDebugType Type = EComponentDebugType::int16;             };
template<>           struct TComponentDebugType<uint16>                    { static const EComponentDebugType Type = EComponentDebugType::Uint16;            };
template<>           struct TComponentDebugType<int32>                     { static const EComponentDebugType Type = EComponentDebugType::Int32;             };
template<>           struct TComponentDebugType<float>                     { static const EComponentDebugType Type = EComponentDebugType::Float;             };
template<>           struct TComponentDebugType<double>                    { static const EComponentDebugType Type = EComponentDebugType::Double;            };
template<>           struct TComponentDebugType<FVector2D>                 { static const EComponentDebugType Type = EComponentDebugType::Vector2;           };
template<>           struct TComponentDebugType<FVector>                   { static const EComponentDebugType Type = EComponentDebugType::Vector3;           };
template<>           struct TComponentDebugType<FVector4>                  { static const EComponentDebugType Type = EComponentDebugType::Vector4;           };
template<>           struct TComponentDebugType<FFrameTime>                { static const EComponentDebugType Type = EComponentDebugType::FrameTime;         };
template<>           struct TComponentDebugType<FObjectKey>                { static const EComponentDebugType Type = EComponentDebugType::ObjectKey;         };
template<>           struct TComponentDebugType<UObject*>                  { static const EComponentDebugType Type = EComponentDebugType::Object;            };
template<>           struct TComponentDebugType<FMovieSceneEntityID>       { static const EComponentDebugType Type = EComponentDebugType::EntityID;          };
template<>           struct TComponentDebugType<FName>                     { static const EComponentDebugType Type = EComponentDebugType::Name;              };

} // namespace MovieScene
} // namespace UE


#endif // UE_MOVIESCENE_ENTITY_DEBUG
