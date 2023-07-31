//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononCommon.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY(LogSteamAudio);

static TMap<EQualitySettings, SteamAudio::FSimulationQualitySettings> GetRealtimeQualityPresets()
{
	TMap<EQualitySettings, SteamAudio::FSimulationQualitySettings> Presets;
	SteamAudio::FSimulationQualitySettings Settings;

	Settings.Bounces = 2;
	Settings.Rays = 4096;
	Settings.SecondaryRays = 512;
	Presets.Add(EQualitySettings::LOW, Settings);

	Settings.Bounces = 4;
	Settings.Rays = 8192;
	Settings.SecondaryRays = 1024;
	Presets.Add(EQualitySettings::MEDIUM, Settings);

	Settings.Bounces = 8;
	Settings.Rays = 16384;
	Settings.SecondaryRays = 2048;
	Presets.Add(EQualitySettings::HIGH, Settings);

	Settings.Bounces = 0;
	Settings.Rays = 0;
	Settings.SecondaryRays = 0;
	Presets.Add(EQualitySettings::CUSTOM, Settings);

	return Presets;
}

static TMap<EQualitySettings, SteamAudio::FSimulationQualitySettings> GetBakedQualityPresets()
{
	TMap<EQualitySettings, SteamAudio::FSimulationQualitySettings> Presets;
	SteamAudio::FSimulationQualitySettings Settings;

	Settings.Bounces = 64;
	Settings.Rays = 16384;
	Settings.SecondaryRays = 2048;
	Presets.Add(EQualitySettings::LOW, Settings);

	Settings.Bounces = 128;
	Settings.Rays = 32768;
	Settings.SecondaryRays = 4096;
	Presets.Add(EQualitySettings::MEDIUM, Settings);

	Settings.Bounces = 256;
	Settings.Rays = 65536;
	Settings.SecondaryRays = 8192;
	Presets.Add(EQualitySettings::HIGH, Settings);

	Settings.Bounces = 0;
	Settings.Rays = 0;
	Settings.SecondaryRays = 0;
	Presets.Add(EQualitySettings::CUSTOM, Settings);

	return Presets;
}

namespace SteamAudio
{
	TMap<EQualitySettings, FSimulationQualitySettings> RealtimeSimulationQualityPresets = GetRealtimeQualityPresets();
	TMap<EQualitySettings, FSimulationQualitySettings> BakedSimulationQualityPresets = GetBakedQualityPresets();

	FString BasePath;
	FString RuntimePath;
	FString DynamicRuntimePath;
	FString EditorOnlyPath;
	FString DynamicEditorOnlyPath;

	void* UnrealAlloc(const size_t size, const size_t alignment)
	{
		return FMemory::Malloc(size, alignment);
	}

	void UnrealFree(void* ptr)
	{
		FMemory::Free(ptr);
	}

	void UnrealLog(char* msg)
	{
		FString Message(msg);
		UE_LOG(LogSteamAudio, Log, TEXT("%s"), *Message); 
	}

	IPLhandle GlobalContext = nullptr;

	FVector UnrealToPhononFVector(const FVector& UnrealCoords, const bool bScale)
	{
		FVector PhononCoords;
		PhononCoords.X = UnrealCoords.Y;
		PhononCoords.Y = UnrealCoords.Z; 
		PhononCoords.Z = -UnrealCoords.X; 
		return bScale ? PhononCoords * SCALEFACTOR : PhononCoords;
	}

	IPLVector3 UnrealToPhononIPLVector3(const FVector& UnrealCoords, const bool bScale)
	{
		IPLVector3 PhononCoords;
		PhononCoords.x = UnrealCoords.Y;
		PhononCoords.y = UnrealCoords.Z;
		PhononCoords.z = -UnrealCoords.X;
		
		if (bScale)
		{
			PhononCoords.x *= SCALEFACTOR;
			PhononCoords.y *= SCALEFACTOR;
			PhononCoords.z *= SCALEFACTOR;
		}

		return PhononCoords;
	}

	FVector PhononToUnrealFVector(const FVector& coords, const bool bScale)
	{
		FVector UnrealCoords;
		UnrealCoords.X = -coords.Z;
		UnrealCoords.Y = coords.X;
		UnrealCoords.Z = coords.Y;
		
		if (bScale)
		{
			UnrealCoords.X /= SCALEFACTOR;
			UnrealCoords.Y /= SCALEFACTOR;
			UnrealCoords.Z /= SCALEFACTOR;
		}

		return UnrealCoords;
	}

	IPLVector3 PhononToUnrealIPLVector3(const FVector& coords, const bool bScale)
	{
		IPLVector3 UnrealCoords = {0, 0, 0};

		// TODO

		return UnrealCoords;
	}

	IPLVector3 IPLVector3FromFVector(const FVector& Coords)
	{
		IPLVector3 IplVector;
		IplVector.x = Coords.X;
		IplVector.y = Coords.Y;
		IplVector.z = Coords.Z;
		return IplVector;
	}

	FVector FVectorFromIPLVector3(const IPLVector3& Coords)
	{
		FVector Vector;
		Vector.X = Coords.x;
		Vector.Y = Coords.y;
		Vector.Z = Coords.z;
		return Vector;
	}

	/**
	 * Given a Unreal transform, convert it to a 4x4 column-major transformation matrix. OutMatrix is assumed to be a contiguous array of
	 * 16 floats.
	 */
	void GetMatrixForTransform(const FTransform& Transform, float* OutMatrix, bool bOutputRowMajor /* = false */, bool bApplyScale /* = true */)
	{
		check(OutMatrix);

		auto PhononTranslation = SteamAudio::UnrealToPhononFVector(Transform.GetTranslation());
		auto PhononScale = SteamAudio::UnrealToPhononFVector(Transform.GetScale3D(), bApplyScale);

		FQuat RotationQuatConverted;
		RotationQuatConverted.X = -Transform.GetRotation().Y;
		RotationQuatConverted.Y = -Transform.GetRotation().Z;
		RotationQuatConverted.Z = Transform.GetRotation().X;
		RotationQuatConverted.W = Transform.GetRotation().W;

		FQuatRotationTranslationMatrix RotationTranslationMatrix(RotationQuatConverted, PhononTranslation);
		FScaleMatrix ScaleMatrix(PhononScale);
		auto ConvertedMatrix = (ScaleMatrix * RotationTranslationMatrix).GetTransposed();
		
		// Convert to column major if bOutputRowMajor is false, otherwise output column major
		if (bOutputRowMajor)
		{
			// Row major order (flatten)
			for (int32 i = 0; i < 4; ++i)
			{
				for (int32 j = 0; j < 4; ++j)
				{
					OutMatrix[i * 4 + j] = ConvertedMatrix.M[i][j];
				}
			}		
		}
		else
		{
			// Column major order (conversion from row major order)
			for (int32 i = 0; i < 4; ++i)
			{
				for (int32 j = 0; j < 4; ++j)
				{
					OutMatrix[j * 4 + i] = ConvertedMatrix.M[i][j];
				}
			}
		}
	}

	IPLMatrix4x4 GetIPLMatrix(float* InMatrix)
	{
		check(InMatrix);

		IPLMatrix4x4 PhononMatrix;
		FMemory::Memcpy(&PhononMatrix, InMatrix, sizeof(PhononMatrix));
		
		return PhononMatrix;
	}

	FString StrippedMapName(const FString& MapName)
	{
		const FRegexPattern PieMapPattern(TEXT("UEDPIE_\\d_"));
		FRegexMatcher Matcher(PieMapPattern, MapName);
		FString StrippedMapName = MapName;

		if (Matcher.FindNext())
		{
			int32 Beginning = Matcher.GetMatchBeginning();
			int32 Ending = Matcher.GetMatchEnding();
			StrippedMapName = MapName.Mid(Ending, MapName.Len());
		}

		return StrippedMapName;
	}

	void* LoadDll(const FString& DllFile, bool bErrorOnLoadFailure)
	{
		UE_LOG(LogSteamAudio, Display, TEXT("Attempting to load %s"), *DllFile);

		void* DllHandle = nullptr;

		if (FPaths::FileExists(DllFile))
		{
			DllHandle = FPlatformProcess::GetDllHandle(*DllFile);
		}
		else
		{
			if (bErrorOnLoadFailure)
			{
				UE_LOG(LogSteamAudio, Error, TEXT("Failed to load missing file. %s"), *DllFile);
			}
			else
			{
				UE_LOG(LogSteamAudio, Display, TEXT("File does not exist. %s"), *DllFile);
			}
		}

		if (!DllHandle)
		{
			if (bErrorOnLoadFailure)
			{
				UE_LOG(LogSteamAudio, Error, TEXT("Failure to load %s."), *DllFile)
			}
			else
			{
				UE_LOG(LogSteamAudio, Display, TEXT("Unable to load %s."), *DllFile);
			}
		}
		else
		{
			UE_LOG(LogSteamAudio, Display, TEXT("Loaded %s."), *DllFile);
		}

		return DllHandle;
	}

	void LogSteamAudioStatus(const IPLerror Status)
	{
		if (Status != IPL_STATUS_SUCCESS)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Error: %s"), *StatusToFString(Status));
		}
	}

	/**
	 * Returns a string representing the given status.
	 */
	FString StatusToFString(const IPLerror Status)
	{
		switch (Status)
		{
		case IPL_STATUS_SUCCESS:
			return FString(TEXT("Success."));
		case IPL_STATUS_FAILURE:
			return FString(TEXT("Failure."));
		case IPL_STATUS_OUTOFMEMORY:
			return FString(TEXT("Out of memory."));
		case IPL_STATUS_INITIALIZATION:
			return FString(TEXT("Initialization error."));
		default:
			return FString(TEXT("Unknown error."));
		}
	}

	void ClosestHit(const IPLfloat32* Origin, const IPLfloat32* Direction, const IPLfloat32 MinDistance, const IPLfloat32 MaxDistance,
		IPLfloat32* HitDistance, IPLfloat32* HitNormal, IPLMaterial** HitMaterial, IPLvoid* UserData)
	{
		UWorld* World = (UWorld*)UserData;

		// PhysX doesn't handle infinity properly, leads to NaNs
		float AdjustedMax = MaxDistance;

		if (MaxDistance > 999999.0f)
			AdjustedMax = 999999.0f;
		else if (MaxDistance < -999999.0f)
			AdjustedMax = -999999.0f;

		FVector PhononOrigin(Origin[0], Origin[1], Origin[2]);
		FVector PhononDirection(Direction[0], Direction[1], Direction[2]);
		FVector PhononStart = PhononOrigin + PhononDirection * MinDistance;
		FVector PhononEnd = PhononOrigin + PhononDirection * AdjustedMax;

		FVector Start = PhononToUnrealFVector(PhononStart);
		FVector End = PhononToUnrealFVector(PhononEnd);
		FCollisionQueryParams TraceParams;
		TraceParams.bTraceComplex = true;

		FHitResult Result;
		bool HitStatic = World->LineTraceSingleByObjectType(Result, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), TraceParams);

		*HitDistance = Result.Distance * SCALEFACTOR;

		FVector PhononNormal = UnrealToPhononFVector(Result.Normal, false);
		HitNormal[0] = PhononNormal.X;
		HitNormal[1] = PhononNormal.Y;
		HitNormal[2] = PhononNormal.Z;
		*HitMaterial = nullptr;
	}

	void AnyHit(const IPLfloat32* Origin, const IPLfloat32* Direction, const IPLfloat32 MinDistance, const IPLfloat32 MaxDistance,
		IPLint32* HitExists, IPLvoid* UserData)
	{
		UWorld* World = (UWorld*)UserData;

		// PhysX doesn't handle infinity properly, leads to NaNs
		float AdjustedMax = MaxDistance;

		if (MaxDistance > 999999.0f)
			AdjustedMax = 999999.0f;
		else if (MaxDistance < -999999.0f)
			AdjustedMax = -999999.0f;

		FVector PhononOrigin(Origin[0], Origin[1], Origin[2]);
		FVector PhononDirection(Direction[0], Direction[1], Direction[2]);
		FVector PhononStart = PhononOrigin + PhononDirection * MinDistance;
		FVector PhononEnd = PhononOrigin + PhononDirection * AdjustedMax;

		FVector Start = PhononToUnrealFVector(PhononStart);
		FVector End = PhononToUnrealFVector(PhononEnd);
		FCollisionQueryParams TraceParams;
		TraceParams.bTraceComplex = true;

		FHitResult Result;
		bool HitStatic = World->LineTraceSingleByObjectType(Result, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), TraceParams);

		*HitExists = (int)HitStatic;
	}
}
