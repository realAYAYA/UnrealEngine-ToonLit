// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenCVHelper.h"

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"

#if WITH_OPENCV

#include "PreOpenCVHeaders.h"

#include "opencv2/calib3d.hpp"
#include "PostOpenCVHeaders.h"

#endif	// WITH_OPENCV

void FOpenCVHelper::ConvertCoordinateSystem(FTransform& Transform, const EAxis SrcXInDstAxis, const EAxis SrcYInDstAxis, const EAxis SrcZInDstAxis)
{
	// Unreal Engine:
	//   Front : X
	//   Right : Y
	//   Up    : Z
	//
	// OpenCV:
	//   Front : Z
	//   Right : X
	//   Up    : Yn

	FMatrix M12 = FMatrix::Identity;

	M12.SetColumn(0, UnitVectorFromAxisEnum(SrcXInDstAxis));
	M12.SetColumn(1, UnitVectorFromAxisEnum(SrcYInDstAxis));
	M12.SetColumn(2, UnitVectorFromAxisEnum(SrcZInDstAxis));

	Transform.SetFromMatrix(M12.GetTransposed() * Transform.ToMatrixWithScale() * M12);
}

void FOpenCVHelper::ConvertUnrealToOpenCV(FTransform& Transform)
{
	ConvertCoordinateSystem(Transform, EAxis::Y, EAxis::Zn, EAxis::X);
}

void FOpenCVHelper::ConvertOpenCVToUnreal(FTransform& Transform)
{
	ConvertCoordinateSystem(Transform, EAxis::Z, EAxis::X, EAxis::Yn);
}

#if WITH_OPENCV
cv::Mat FOpenCVLensDistortionParametersBase::ConvertToOpenCVDistortionCoefficients() const
{
	if (bUseFisheyeModel)
	{
		cv::Mat DistortionCoefficients(1, 4, CV_64F);
		DistortionCoefficients.at<double>(0) = K1;
		DistortionCoefficients.at<double>(1) = K2;
		DistortionCoefficients.at<double>(2) = K3;
		DistortionCoefficients.at<double>(3) = K4;
		return DistortionCoefficients;
	}
	else
	{
		cv::Mat DistortionCoefficients(1, 8, CV_64F);
		DistortionCoefficients.at<double>(0) = K1;
		DistortionCoefficients.at<double>(1) = K2;
		DistortionCoefficients.at<double>(2) = P1;
		DistortionCoefficients.at<double>(3) = P2;
		DistortionCoefficients.at<double>(4) = K3;
		DistortionCoefficients.at<double>(5) = K4;
		DistortionCoefficients.at<double>(6) = K5;
		DistortionCoefficients.at<double>(7) = K6;
		return DistortionCoefficients;
	}
}

cv::Mat FOpenCVLensDistortionParametersBase::CreateOpenCVCameraMatrix(const FVector2D& InImageSize) const
{
	cv::Mat CameraMatrix = cv::Mat::eye(3, 3, CV_64F);
	CameraMatrix.at<double>(0, 0) = F.X * InImageSize.X;
	CameraMatrix.at<double>(1, 1) = F.Y * InImageSize.Y;
	CameraMatrix.at<double>(0, 2) = C.X * InImageSize.X;
	CameraMatrix.at<double>(1, 2) = C.Y * InImageSize.Y;
	return CameraMatrix;
}

UTexture2D* FOpenCVHelper::TextureFromCvMat(cv::Mat& Mat, const FString* PackagePath, const FName* TextureName)
{
	if ((Mat.cols <= 0) || (Mat.rows <= 0))
	{
		return nullptr;
	}

	// Currently we only support G8 and BGRA8

	if (Mat.depth() != CV_8U)
	{
		return nullptr;
	}

	EPixelFormat PixelFormat;
	ETextureSourceFormat SourceFormat;

	switch (Mat.channels())
	{
	case 1:
		PixelFormat = PF_G8;
		SourceFormat = TSF_G8;
		break;

	case 4:
		PixelFormat = PF_B8G8R8A8;
		SourceFormat = TSF_BGRA8;
		break;

	default:
		return nullptr;
	}

	UTexture2D* Texture = nullptr;

#if WITH_EDITOR
	if (PackagePath && TextureName)
	{
		Texture = NewObject<UTexture2D>(CreatePackage(**PackagePath), *TextureName, RF_Standalone | RF_Public);

		if (!Texture)
		{
			return nullptr;
		}

		const int32 NumSlices = 1;
		const int32 NumMips = 1;

		Texture->Source.Init(Mat.cols, Mat.rows, NumSlices, NumMips, SourceFormat, Mat.data);

		auto IsPowerOfTwo = [](int32 Value)
		{
			return (Value > 0) && ((Value & (Value - 1)) == 0);
		};

		if (!IsPowerOfTwo(Mat.cols) || !IsPowerOfTwo(Mat.rows))
		{
			Texture->MipGenSettings = TMGS_NoMipmaps;
		}

		Texture->SRGB = 0;

		FTextureFormatSettings FormatSettings;

		if (Mat.channels() == 1)
		{
			Texture->CompressionSettings = TextureCompressionSettings::TC_Grayscale;
			Texture->CompressionNoAlpha = true;
		}

		Texture->SetLayerFormatSettings(0, FormatSettings);

		Texture->SetPlatformData(new FTexturePlatformData());
		Texture->GetPlatformData()->SizeX = Mat.cols;
		Texture->GetPlatformData()->SizeY = Mat.rows;
		Texture->GetPlatformData()->PixelFormat = PixelFormat;

		Texture->UpdateResource();

		Texture->MarkPackageDirty();
	}
	else
#endif //WITH_EDITOR
	{
		Texture = UTexture2D::CreateTransient(Mat.cols, Mat.rows, PixelFormat);

		if (!Texture)
		{
			return nullptr;
		}

#if WITH_EDITORONLY_DATA
		Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		Texture->NeverStream = true;
		Texture->SRGB = 0;

		if (Mat.channels() == 1)
		{
			Texture->CompressionSettings = TextureCompressionSettings::TC_Grayscale;
#if WITH_EDITORONLY_DATA
			Texture->CompressionNoAlpha = true;
#endif
		}

		// Copy the pixels from the OpenCV Mat to the Texture

		FTexture2DMipMap& Mip0 = Texture->GetPlatformData()->Mips[0];
		void* TextureData = Mip0.BulkData.Lock(LOCK_READ_WRITE);

		const int32 PixelStride = Mat.channels();
		FMemory::Memcpy(TextureData, Mat.data, Mat.cols * Mat.rows * SIZE_T(PixelStride));

		Mip0.BulkData.Unlock();

		Texture->UpdateResource();
	}

	return Texture;
}

UTexture2D* FOpenCVHelper::TextureFromCvMat(cv::Mat& Mat, UTexture2D* InTexture)
{
	if (!InTexture)
	{
		return TextureFromCvMat(Mat);
	}

	if ((Mat.cols <= 0) || (Mat.rows <= 0))
	{
		return nullptr;
	}

	// Currently we only support G8 and BGRA8

	if (Mat.depth() != CV_8U)
	{
		return nullptr;
	}

	EPixelFormat PixelFormat;

	switch (Mat.channels())
	{
	case 1:
		PixelFormat = PF_G8;
		break;

	case 4:
		PixelFormat = PF_B8G8R8A8;
		break;

	default:
		return nullptr;
	}

	if ((InTexture->GetSizeX() != Mat.cols) || (InTexture->GetSizeY() != Mat.rows) || (InTexture->GetPixelFormat() != PixelFormat))
	{
		return nullptr;
	}

	// Copy the pixels from the OpenCV Mat to the Texture

	FTexture2DMipMap& Mip0 = InTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip0.BulkData.Lock(LOCK_READ_WRITE);

	const int32 PixelStride = Mat.channels();
	FMemory::Memcpy(TextureData, Mat.data, Mat.cols * Mat.rows * SIZE_T(PixelStride));

	Mip0.BulkData.Unlock();

	InTexture->UpdateResource();

	return InTexture;
}

double FOpenCVHelper::ComputeReprojectionError(const FTransform& CameraPose, const cv::Mat& CameraIntrinsicMatrix, const std::vector<cv::Point3f>& Points3d, const std::vector<cv::Point2f>& Points2d)
{
	// Ensure that the number of point correspondences is valid
	const int32 NumPoints3d = Points3d.size();
	const int32 NumPoints2d = Points2d.size();
	if ((NumPoints3d == 0) || (NumPoints2d == 0) || (NumPoints3d != NumPoints2d))
	{
		return -1.0;
	}

	const FMatrix CameraPoseMatrix = CameraPose.ToMatrixNoScale();

	const cv::Mat Tcam = (cv::Mat_<double>(3, 1) << CameraPoseMatrix.M[3][0], CameraPoseMatrix.M[3][1], CameraPoseMatrix.M[3][2]);

	cv::Mat Rcam = cv::Mat::zeros(3, 3, cv::DataType<double>::type);
	for (int32 Column = 0; Column < 3; ++Column)
	{
		FVector ColVec = CameraPoseMatrix.GetColumn(Column);
		Rcam.at<double>(Column, 0) = ColVec.X;
		Rcam.at<double>(Column, 1) = ColVec.Y;
		Rcam.at<double>(Column, 2) = ColVec.Z;
	}

	const cv::Mat Robj = Rcam.t();

	cv::Mat Rrod;
	cv::Rodrigues(Robj, Rrod);

	const cv::Mat Tobj = -Rcam.inv() * Tcam;

	std::vector<cv::Point2f> ReprojectedPoints2d;

	// The 2D points will be compared against the undistorted 2D points, so the distortion coefficients can be ignored
	cv::projectPoints(Points3d, Rrod, Tobj, CameraIntrinsicMatrix, cv::noArray(), ReprojectedPoints2d);

	if (ReprojectedPoints2d.size() != NumPoints2d)
	{
		return -1.0;
	}

	// Compute euclidean distance between captured 2D points and reprojected 2D points to measure reprojection error
	double ReprojectionError = 0.0;
	for (int32 Index = 0; Index < NumPoints2d; ++Index)
	{
		const cv::Point2f& A = Points2d[Index];
		const cv::Point2f& B = ReprojectedPoints2d[Index];
		const cv::Point2f Diff = A - B;

		ReprojectionError += (Diff.x * Diff.x) + (Diff.y * Diff.y); // cv::norm with NORM_L2SQR
	}

	return ReprojectionError;
}

#endif // WITH_OPENCV
