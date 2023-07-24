// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include <vector>

/*
 * Like many third party headers, OpenCV headers require some care when importing.
 *
 * When including opencv headers, the includes should be wrapped like this:

	#include PreOpenCVHeaders.h

	// your opencv include directives go here...

	#include PostOpenCVHeaders.h

 * note: On Linux platform there is a typedef conflict with (u)int64
 * to use OpenCV api with these types, use the helper type OpenCVUtils::int64

 */

#include "OpenCVHelper.generated.h"

#define OPENCV_INCLUDES_START static_assert(false, "Include PreOpenCVHeaders.h instead of using this macro.");

#define OPENCV_INCLUDES_END static_assert(false, "Include PostOpenCVHeaders.h instead of using this macro");

#if WITH_OPENCV

class UTexture2D;
class FString;
class FName;

namespace cv
{
	class Mat;
	template<typename _Tp> class Point_;
	template<typename _Tp> class Point3_;

	typedef Point_<float> Point2f;
	typedef Point3_<float> Point3f;
};
#endif	// WITH_OPENCV


class OPENCVHELPER_API FOpenCVHelper
{
public:
	/** Enumeration to specify any cartesian axis in positive or negative directions */
	enum class EAxis
	{
		X, Y, Z,
		Xn, Yn, Zn,
	};

	// These axes must match the order in which they are declared in EAxis
	inline static const TArray<FVector> UnitVectors =
	{
		{  1,  0,  0 }, //  X
		{  0,  1,  0 }, //  Y
		{  0,  0,  1 }, //  Z
		{ -1,  0,  0 }, // -X
		{  0, -1,  0 }, // -Y
		{  0,  0, -1 }, // -Z
	};

	static const FVector& UnitVectorFromAxisEnum(const EAxis Axis)
	{
		return UnitVectors[std::underlying_type_t<EAxis>(Axis)];
	};

	/** Converts in-place the coordinate system of the given FTransform by specifying the source axes in terms of the destionation axes */
	static void ConvertCoordinateSystem(FTransform& Transform, const EAxis DstXInSrcAxis, const EAxis DstYInSrcAxis, const EAxis DstZInSrcAxis);

	/** Converts in-place an FTransform in Unreal coordinates to OpenCV coordinates */
	static void ConvertUnrealToOpenCV(FTransform& Transform);

	/** Converts in-place an FTransform in OpenCV coordinates to Unreal coordinates */
	static void ConvertOpenCVToUnreal(FTransform& Transform);

#if WITH_OPENCV
public:
	/**
	 * Creates a Texture from the given Mat, if its properties (e.g. pixel format) are supported.
	 * 
	 * @param Mat The OpenCV Mat to convert.
	 * @param PackagePath Optional path to a package to create the texture in.
	 * @param TextureName Optional name for the texture. Required if PackagePath is not nullptr.
	 * 
	 * @return Texture created out of the given OpenCV Mat.
	 */
	static UTexture2D* TextureFromCvMat(cv::Mat& Mat, const FString* PackagePath = nullptr, const FName* TextureName = nullptr);
	static UTexture2D* TextureFromCvMat(cv::Mat& Mat, UTexture2D* InTexture);

	static double ComputeReprojectionError(const FTransform& CameraPose, const cv::Mat& CameraIntrinsicMatrix, const std::vector<cv::Point3f>& Points3d, const std::vector<cv::Point2f>& Points2d);
#endif	// WITH_OPENCV
};

/**
 * Mathematic camera model for lens distortion/undistortion.
 * Camera matrix =
 *  | F.X  0  C.x |
 *  |  0  F.Y C.Y |
 *  |  0   0   1  |
 * where F and C are normalized.
 */
USTRUCT(BlueprintType)
struct OPENCVHELPER_API FOpenCVLensDistortionParametersBase
{
	GENERATED_USTRUCT_BODY()

public:
	FOpenCVLensDistortionParametersBase()
		: K1(0.f)
		, K2(0.f)
		, P1(0.f)
		, P2(0.f)
		, K3(0.f)
		, K4(0.f)
		, K5(0.f)
		, K6(0.f)
		, F(FVector2D(1.f, 1.f))
		, C(FVector2D(0.5f, 0.5f))
		, bUseFisheyeModel(false)
	{
	}

public:
#if WITH_OPENCV
	/** Convert internal coefficients to OpenCV matrix representation */
	cv::Mat ConvertToOpenCVDistortionCoefficients() const;

	/** Convert internal normalized camera matrix to OpenCV pixel scaled matrix representation. */
	cv::Mat CreateOpenCVCameraMatrix(const FVector2D& InImageSize) const;
#endif //WITH_OPENCV

public:
	/** Compare two lens distortion models and return whether they are equal. */
	bool operator == (const FOpenCVLensDistortionParametersBase& Other) const
	{
		return (K1 == Other.K1 &&
			K2 == Other.K2 &&
			P1 == Other.P1 &&
			P2 == Other.P2 &&
			K3 == Other.K3 &&
			K4 == Other.K4 &&
			K5 == Other.K5 &&
			K6 == Other.K6 &&
			F == Other.F &&
			C == Other.C &&
			bUseFisheyeModel == Other.bUseFisheyeModel);
	}

	/** Compare two lens distortion models and return whether they are different. */
	bool operator != (const FOpenCVLensDistortionParametersBase& Other) const
	{
		return !(*this == Other);
	}

	/** Returns true if lens distortion parameters are for identity lens (or default parameters) */
	bool IsIdentity() const
	{
		return (K1 == 0.0f &&
			K2 == 0.0f &&
			P1 == 0.0f &&
			P2 == 0.0f &&
			K3 == 0.0f &&
			K4 == 0.0f &&
			K5 == 0.0f &&
			K6 == 0.0f &&
			F == FVector2D(1.0f, 1.0f) &&
			C == FVector2D(0.5f, 0.5f));
	}

	bool IsSet() const
	{
		return *this != FOpenCVLensDistortionParametersBase();
	}

public:
	/** Radial parameter #1. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K1;

	/** Radial parameter #2. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K2;

	/** Tangential parameter #1. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float P1;

	/** Tangential parameter #2. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float P2;

	/** Radial parameter #3. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K3;

	/** Radial parameter #4. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K4;

	/** Radial parameter #5. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K5;
	
	/** Radial parameter #6. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K6;

	/** Camera matrix's normalized Fx and Fy. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	FVector2D F;

	/** Camera matrix's normalized Cx and Cy. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	FVector2D C;

	/** Camera lens needs Fisheye camera model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	bool bUseFisheyeModel;
};
