// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AppleVisionTypes.generated.h"

/** Base type for a detected feature */
USTRUCT(BlueprintType)
struct APPLEVISION_API FDetectedFeature
{
public:
	GENERATED_BODY()

	/** How confident the ML was in determining this feature and its type */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	float Confidence;
};

/** Area of the image that the computer vision task detected as being of a particular type */
USTRUCT(BlueprintType)
struct APPLEVISION_API FDetectedFeature2D :
	public FDetectedFeature
{
public:
	GENERATED_BODY()

	/** The bounding box of the detected feature in the image */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	FBox2D BoundingBox;
};

/** Area of the image that the computer vision task detected as being of a particular type */
USTRUCT(BlueprintType)
struct APPLEVISION_API FDetectedFeatureRegion :
	public FDetectedFeature
{
public:
	GENERATED_BODY()

	/** The set of points that encompass the detected feature area */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	TArray<FVector2D> Points;

	FORCEINLINE FDetectedFeatureRegion& operator=(const FDetectedFeatureRegion& Other)
	{
		Confidence = Other.Confidence;
		Points = Other.Points;
		return *this;
	}
};

/** Features of a face that can be detected */
UENUM(BlueprintType, Category="Apple Vision", meta=(Experimental))
enum class EDetectedFaceFeatureType : uint8
{
	Unkown,
	FaceContour,
	InnerLips,
	LeftEye,
	LeftEyebrow,
	LeftPupil,
	MedianLine,
	Nose,
	NoseCrest,
	OuterLips,
	RightEye,
	RightEyebrow,
	RightPupil
};

/** Area of the image that the computer vision task detected as being part of a face */
USTRUCT(BlueprintType)
struct APPLEVISION_API FDetectedFaceFeatureRegion :
	public FDetectedFeatureRegion
{
public:
	GENERATED_BODY()

	/** The type of region that was detected */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	EDetectedFaceFeatureType FeatureType;
};

/** Area of the image that the computer vision task detected as being part of a face */
USTRUCT(BlueprintType)
struct APPLEVISION_API FDetectedFaceFeature2D :
	public FDetectedFeature2D
{
public:
	GENERATED_BODY()

	/** The type of region that was detected */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	EDetectedFaceFeatureType FeatureType;
};

/** Area of the image that the computer vision task detected as being a face */
USTRUCT(BlueprintType)
struct APPLEVISION_API FDetectedFace :
	public FDetectedFeatureRegion
{
public:
	GENERATED_BODY()

	/** The bounding box of the detected face */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	FBox2D BoundingBox;

	/** The set of 2D features that were detected */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	TArray<FDetectedFaceFeature2D> Features;

	/** The set of region features that were detected */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	TArray<FDetectedFaceFeatureRegion> FeatureRegions;

	FORCEINLINE FDetectedFace& operator=(const FDetectedFace& Other)
	{
		Confidence = Other.Confidence;
		Points = Other.Points;
		BoundingBox = Other.BoundingBox;
		Features = Other.Features;
		FeatureRegions = Other.FeatureRegions;
		return *this;
	}
};

/** The result of a face detection request with information about the detected faces */
USTRUCT(BlueprintType)
struct APPLEVISION_API FFaceDetectionResult
{
public:
	GENERATED_BODY()

	FFaceDetectionResult() {}

	/** The set of faces that were detected */
	UPROPERTY(BlueprintReadOnly, Category="Apple Vision")
	TArray<FDetectedFace> DetectedFaces;

	FORCEINLINE FFaceDetectionResult& operator=(const FFaceDetectionResult& Other)
	{
		DetectedFaces = Other.DetectedFaces;
		return *this;
	}
};
