// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOWarper.h"
#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOLibrary.h"
#include "Misc/DisplayClusterHelpers.h"

#include "Render/Viewport/IDisplayClusterViewport.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// FViosoWarper
//////////////////////////////////////////////////////////////////////////////////////////////
bool FViosoWarper::IsValid()
{
	return pWarper && FLibVIOSO::Initialize();
}

bool FViosoWarper::Initialize(void* pDxDevice, const FViosoPolicyConfiguration& InConfigData)
{
	if (pDxDevice && FLibVIOSO::Initialize())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay FViosoWarper::Initialize);

		if (InConfigData.INIFile.IsEmpty())
		{
			// Create without INI file
			if (VWB_ERROR_NONE != FLibVIOSO::Create(pDxDevice, nullptr, nullptr, &pWarper, 0, nullptr))
			{
				// Failed to create default vioso container
				return false;
			}

			// Assign calib file
			FPlatformString::Strcpy(pWarper->calibFile, sizeof(VWB_Warper::calibFile), TCHAR_TO_ANSI(*InConfigData.CalibrationFile));

			/// set a moving range. This applies only for 3D mappings and dynamic eye point.
			/// This is a factor applied to the projector mapped MIN(width,height)/2
			/// The view plane is widened to cope with a movement to all sides, defaults to 1
			/// Check borderFit in log: 1 means all points stay on unwidened viewplane, 2 means, we had to double it.
			pWarper->autoViewC = 1;

			/// set to true to calculate view parameters while creating warper, defaults to false
			/// All further values are calculated/overwritten, if bAutoView is set.
			/// // set to true to calculate view parameters while creating warper, defaults to false
			pWarper->bAutoView = true;

			/// set to true to make the world turn and move with view direction and eye position, this is the case if the viewer gets
			/// moved by a motion platform, defaults to false
			pWarper->bTurnWithView = true;

			/// set to true to enable bicubic sampling from source texture
			pWarper->bBicubic = true;

			/// the calibration index in mapping file, defaults to 0,
			/// you also might set this to negated display number, to search for a certain display:
			pWarper->calibIndex = InConfigData.CalibrationIndex;

			/// the near plane distance
			/// the far plane distance, note: these values are used to create the projection matrix
			pWarper->nearDist = 1;
			pWarper->farDist = 20000;

			/// set a gamma correction value. This is only useful, if you changed the projector's gamma setting after calibration,
			/// as the gamma is already calculated inside blend map, or to fine-tune, defaults to 1 (no change)
			pWarper->gamma = InConfigData.Gamma;

			// the transformation matrix to go from VIOSO coordinates to IG coordinates, defaults to indentity
			// note VIOSO maps are always right-handed, to use with a left-handed world like DirectX, invert the z!
			FMatrix TransMatrix = InConfigData.BaseMatrix.GetTransposed();
			for (uint32 MatrixElementIndex = 0; MatrixElementIndex < 16; MatrixElementIndex++)
			{
				pWarper->trans[MatrixElementIndex] = (&TransMatrix.M[0][0])[MatrixElementIndex];
			}
		}
		else
		{
			// Create with INI file
			if (VWB_ERROR_NONE != FLibVIOSO::Create(pDxDevice, TCHAR_TO_ANSI(*InConfigData.INIFile), TCHAR_TO_ANSI(*InConfigData.ChannelName), &pWarper, 0, nullptr))
			{
				// Failed initialize vioso from ini file
				return false;
			}
		}

		// initialize VIOSO warper
		if (VWB_ERROR_NONE == FLibVIOSO::Init(pWarper))
		{
			return true;
		}
	}

	// Failed initialize vioso
	return false;
}

void FViosoWarper::Release()
{
	if (IsValid())
	{
		FLibVIOSO::Destroy(pWarper);
	}

	pWarper = nullptr;
}

bool FViosoWarper::Render(VWB_param RenderParam, VWB_uint StateMask)
{
	return IsValid() && (VWB_ERROR_NONE == FLibVIOSO::Render(pWarper, RenderParam, StateMask));
}

bool FViosoWarper::CalculateViewProjection(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, FMatrix& OutProjMatrix, const float WorldToMeters, const float NCP, const float FCP)
{
	// Convert to vioso space:
	FVector3f InViosoEyeLocation = (FVector3f)ToViosoLocation(InOutViewLocation, WorldToMeters);
	FVector3f InViosoEyeEulerRotation = (FVector3f)ToViosoEulerRotation(InOutViewRotation);

	if (VWB_ERROR_NONE == FLibVIOSO::GetViewClip(pWarper, &InViosoEyeLocation.X, &InViosoEyeEulerRotation.X, &ViewMatrix[0], &ViewClip[0]) && IsViewClipValid())
	{
		// Convert to UE coordinate system
		FVector OutViosoEyeLocation = GetViosoViewLocation();
		FVector OutViewLocation = FromViosoLocation(OutViosoEyeLocation, WorldToMeters);

		FVector  OutViosoEyeEulerRotation = GetViosoViewRotationEulerR_LHT();
		FRotator OutViewRotation = FromViosoEulerRotation(OutViosoEyeEulerRotation);

		//InOutViewLocation = OutViewLocation;
		InOutViewRotation = OutViewRotation;
		OutProjMatrix = GetProjMatrix(InViewport, InContextNum, NCP, FCP);
		return true;
	}

	return false;
}

bool FViosoWarper::IsViewClipValid() const
{
	if (isnan(_left) || isnan(_right) || isnan(_top) || isnan(_bottom))
	{
		return false;
	}
	
	return true;
}

const float UEViosoUnitsInMeter = 1.f;

FVector FViosoWarper::ToViosoLocation(const FVector& InPos, const float WorldToMeters) const
{
	float Scale = (UEViosoUnitsInMeter / WorldToMeters);

	// UE coords(x,y,z) = fw,right,up
	// UE+VIOSO(X,Y,Z) = RIGHT, UP, FW = (y,z,x)
	FVector ViosoLocation(-InPos.Y, -InPos.Z, InPos.X);
	return ViosoLocation * Scale;
}

FVector FViosoWarper::FromViosoLocation(const FVector& InPos, const float WorldToMeters) const
{
	float Scale = (WorldToMeters / UEViosoUnitsInMeter);

	// UE+VIOSO(X,Y,Z) = RIGHT, UP, FW
	// UE coords(x,y,z) = fw,right,up = (Z,X,Y)
	FVector UELocation(InPos.Z, -InPos.X, -InPos.Y);
	return UELocation * Scale;
}

FVector  FViosoWarper::ToViosoEulerRotation(const FRotator& InRotation) const
{
	return FMath::DegreesToRadians(InRotation.Euler());
}

FRotator FViosoWarper::FromViosoEulerRotation(const FVector& InEulerRotation) const
{
	return FRotator::MakeFromEuler(FMath::RadiansToDegrees(InEulerRotation));
}

FVector FViosoWarper::GetViosoViewRotationEulerR_RH() const
{
	float Roll, Pitch, Yaw;

	Yaw = atan2(_31, _33);

	const float SinYaw = sin(Yaw);
	const float CosYaw = cos(Yaw);

	Roll = -atan2(SinYaw * _32 + CosYaw * _12, SinYaw * _13 + CosYaw * _11);

	Pitch = -atan2(_23, sqrt(_31 * _31 + _33 * _33));

	return FVector(Roll, Pitch, Yaw);

}
FVector FViosoWarper::GetViosoViewRotationEulerR_LHT() const
{
	float Roll, Pitch, Yaw;

	Yaw = atan2(-_13, _33);

	const float SinYaw = sin(Yaw);
	const float CosYaw = cos(Yaw);

	Roll = -atan2(SinYaw * _32 + CosYaw * _12, SinYaw * _31 + CosYaw * _11);

	Pitch = -atan2(_23, sqrt(_13 * _13 + _33 * _33));

	return FVector(Roll, Pitch, Yaw);
}

FVector FViosoWarper::GetViosoViewLocation() const
{
	return FVector(_41, _42, _43);
}

FMatrix FViosoWarper::GetProjMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const float NCP, const float FCP) const
{
	InViewport->CalculateProjectionMatrix(InContextNum, -_left, _right, _top, -_bottom, NCP, FCP, false);
	return InViewport->GetContexts()[InContextNum].ProjectionMatrix;
}
