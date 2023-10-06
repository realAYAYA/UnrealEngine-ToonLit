// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

#include "VisualLogger/VisualLogger.h"

#ifndef UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
#define UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG ENABLE_VISUAL_LOG
#endif

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG

#define UE_LEARNING_AGENTS_VLOG_STRING(Owner, Category, Verbosity, Location, Color, Format, ...) \
	UE_VLOG_LOCATION(Owner, Category, Verbosity, Location, 0.0f, Color, Format, ##__VA_ARGS__)

#define UE_LEARNING_AGENTS_VLOG_LOCATION(Owner, Category, Verbosity, Location, Radius, Color, Format, ...) \
	UE_VLOG_LOCATION(Owner, Category, Verbosity, Location, Radius, Color, Format, ##__VA_ARGS__)

#define UE_LEARNING_AGENTS_VLOG_ARROW(Owner, Category, Verbosity, Start, End, Color, Format, ...) \
	UE_VLOG_ARROW(Owner, Category, Verbosity, Start, End, Color, Format, ##__VA_ARGS__)

#define UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Start, End, Color, Format, ...) \
	UE_VLOG_SEGMENT(Owner, Category, Verbosity, Start, End, Color, Format, ##__VA_ARGS__)

#define UE_LEARNING_AGENTS_VLOG_CIRCLE(Owner, Category, Verbosity, Center, UpAxis, Radius, Color, Format, ...) \
	UE_VLOG_CIRCLE(Owner, Category, Verbosity, Center, UpAxis, Radius, Color, Format, ##__VA_ARGS__)

#define UE_LEARNING_AGENTS_VLOG_OBOX(Owner, Category, Verbosity, Box, Matrix, Color, Format, ...) \
	UE_VLOG_OBOX(Owner, Category, Verbosity, Box, Matrix, Color, Format, ##__VA_ARGS__)

#define UE_LEARNING_AGENTS_VLOG_PLANE(Owner, Category, Verbosity, Location, Rotation, Axis0, Axis1, Color, Format, ...) \
	UE_LEARNING_AGENTS_VLOG_OBOX(Owner, Category, Verbosity, FBox(FVector(-25.0f, -25.0f, -0.1f), FVector(25.0f, 25.0f, 0.1f)), UE::Learning::Agents::Debug::PlaneMatrix(Rotation, Location, Axis0, Axis1), Color, Format, ##__VA_ARGS__)

#define UE_LEARNING_AGENTS_VLOG_MATRIX(Owner, Category, Verbosity, Matrix, Color, Format, ...) \
	{ \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Matrix.TransformPosition(FVector::ZeroVector), Matrix.TransformPosition(15.0f * FVector::ForwardVector), FColor::Red, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Matrix.TransformPosition(FVector::ZeroVector), Matrix.TransformPosition(15.0f * FVector::RightVector), FColor::Green, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Matrix.TransformPosition(FVector::ZeroVector), Matrix.TransformPosition(15.0f * FVector::UpVector), FColor::Blue, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_STRING(Owner, Category, Verbosity, Matrix.TransformPosition(FVector::ZeroVector) + FVector(0.0f, 0.0f, 20.0f), Color, Format, ##__VA_ARGS__); \
	}

#define UE_LEARNING_AGENTS_VLOG_TRANSFORM(Owner, Category, Verbosity, Location, Rotation, Color, Format, ...) \
	{ \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Location, Location + 15.0f * Rotation.RotateVector(FVector::ForwardVector), FColor::Red, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Location, Location + 15.0f * Rotation.RotateVector(FVector::RightVector), FColor::Green, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Location, Location + 15.0f * Rotation.RotateVector(FVector::UpVector), FColor::Blue, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_OBOX(Owner, Category, Verbosity, FBox(5.0f * FVector(-1, -1, -1), 5.0f * FVector(1, 1, 1)), FTransform(Rotation, Location, FVector::OneVector).ToMatrixNoScale(), Color, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_STRING(Owner, Category, Verbosity, Location + FVector(0.0f, 0.0f, 20.0f), Color, Format, ##__VA_ARGS__); \
	}

#define UE_LEARNING_AGENTS_VLOG_ANGLE(Owner, Category, Verbosity, Angle, RelativeAngle, Location, Radius, Color, Format, ...) \
	{ \
		UE_LEARNING_AGENTS_VLOG_CIRCLE(Owner, Category, Verbosity, Location, FVector::UpVector, Radius, Color, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Location, Location + Radius * FVector(FMath::Sin(RelativeAngle), FMath::Cos(RelativeAngle), 0.0f), Color, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_SEGMENT(Owner, Category, Verbosity, Location, Location + Radius * FVector(FMath::Sin(Angle), FMath::Cos(Angle), 0.0f), Color, TEXT("")); \
		UE_LEARNING_AGENTS_VLOG_LOCATION(Owner, Category, Verbosity, Location + Radius * FVector(FMath::Sin(Angle), FMath::Cos(Angle), 0.0f), Radius / 20.0f, Color, Format, ##__VA_ARGS__); \
	}

#endif

namespace UE::Learning::Agents::Debug
{
	LEARNINGAGENTS_API FMatrix PlaneMatrix(const FQuat Rotation, const FVector Position, const FVector Axis0, const FVector Axis1);

	LEARNINGAGENTS_API FVector GridOffsetForIndex(const int32 Idx, const int32 Num, const float Spacing = 20.0f, const float Height = 20.0f);

	LEARNINGAGENTS_API FString FloatArrayToStatsString(const TLearningArrayView<1, const float> Array);
}
