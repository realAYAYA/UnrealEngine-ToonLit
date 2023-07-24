// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithUtils.h"
#include "HAL/IConsoleManager.h"
#include "Math/Vector.h"
#include "Misc/Paths.h"
#include "Templates/TypeHash.h"

namespace CADLibrary
{
	CADTOOLS_API extern int32 GMaxImportThreads;

	enum EStitchingTechnique
	{
		StitchingNone = 0,
		StitchingHeal,
		StitchingSew,
	};

	enum class ESewOption : uint8  // Same as UE::CADKernel::ESewOption
	{
		None = 0x00u,	// No flags.

		ForceJoining = 0x01u,
		RemoveThinFaces = 0x02u,
		RemoveDuplicatedFaces = 0x04u,

		All = 0x07u
	};

	ENUM_CLASS_FLAGS(ESewOption);

	class FImportParameters
	{
	private:
		double ChordTolerance;
		double MaxEdgeLength;
		double MaxNormalAngle;
		EStitchingTechnique StitchingTechnique;
		FDatasmithUtils::EModelCoordSystem ModelCoordSys;
		
	public:
		CADTOOLS_API static bool bGDisableCADKernelTessellation;
		CADTOOLS_API static bool bGEnableCADCache;
		CADTOOLS_API static bool bGEnableTimeControl;
		CADTOOLS_API static bool bGOverwriteCache;
		CADTOOLS_API static bool bGPreferJtFileEmbeddedTessellation;
		CADTOOLS_API static bool bGSewMeshIfNeeded;
		CADTOOLS_API static bool bGRemoveDuplicatedTriangle;
		CADTOOLS_API static float GStitchingTolerance;
		CADTOOLS_API static bool bGStitchingForceSew;
		CADTOOLS_API static bool bGStitchingRemoveThinFaces;
		CADTOOLS_API static bool bGStitchingRemoveDuplicatedFaces;
		CADTOOLS_API static float GStitchingForceFactor;
		CADTOOLS_API static float GUnitScale;
		CADTOOLS_API static float GMeshingParameterFactor;
		CADTOOLS_API static int32 GMaxMaterialCountPerMesh;

	public:
		FImportParameters(FDatasmithUtils::EModelCoordSystem NewCoordinateSystem = FDatasmithUtils::EModelCoordSystem::ZUp_RightHanded)
			: ChordTolerance(0.2)
			, MaxEdgeLength(0.0)
			, MaxNormalAngle(20.0)
			, StitchingTechnique(EStitchingTechnique::StitchingNone)
			, ModelCoordSys(NewCoordinateSystem)
		{
		}
	
		void SetTesselationParameters(double InChordTolerance, double InMaxEdgeLength, double InMaxNormalAngle, CADLibrary::EStitchingTechnique InStitchingTechnique)
		{
			ChordTolerance = InChordTolerance * GMeshingParameterFactor;
			MaxEdgeLength = InMaxEdgeLength * GMeshingParameterFactor;
			MaxNormalAngle = InMaxNormalAngle;
			StitchingTechnique = InStitchingTechnique;
		}

		uint32 GetHash() const
		{
			uint32 Hash = 0;
			for (double Param : {ChordTolerance, MaxEdgeLength, MaxNormalAngle, (double) GStitchingForceFactor})
			{
				Hash = HashCombine(Hash, ::GetTypeHash(Param));
			}
			for (uint32 Param : {uint32(StitchingTechnique), uint32(ModelCoordSys)})
			{
				Hash = HashCombine(Hash, ::GetTypeHash(Param));
			}
			for (bool Param : {bGStitchingForceSew, bGStitchingRemoveThinFaces, bGStitchingRemoveDuplicatedFaces, bGDisableCADKernelTessellation, bGPreferJtFileEmbeddedTessellation})
			{
				Hash = HashCombine(Hash, ::GetTypeHash(Param));
			}
			return Hash;
		}

		friend FArchive& operator<<(FArchive& Ar, FImportParameters& ImportParameters)
		{
			Ar << ImportParameters.ChordTolerance;
			Ar << ImportParameters.MaxEdgeLength;
			Ar << ImportParameters.MaxNormalAngle;
			Ar << (uint32&) ImportParameters.StitchingTechnique;
			Ar << (uint8&) ImportParameters.ModelCoordSys;

			// these static variables have to be serialized to be transmitted to CADWorkers
			// because CADWorker has not access to CVars
			Ar << ImportParameters.bGOverwriteCache;
			Ar << ImportParameters.bGDisableCADKernelTessellation;
			Ar << ImportParameters.bGEnableTimeControl;
			Ar << ImportParameters.bGEnableCADCache;
			Ar << ImportParameters.bGPreferJtFileEmbeddedTessellation;
			Ar << ImportParameters.GStitchingTolerance;
			Ar << ImportParameters.bGStitchingForceSew;
			Ar << ImportParameters.bGStitchingRemoveThinFaces;
			Ar << ImportParameters.bGStitchingRemoveDuplicatedFaces;
			Ar << ImportParameters.GStitchingForceFactor;
			Ar << ImportParameters.GMaxMaterialCountPerMesh;
			return Ar;
		}

		double GetChordTolerance() const
		{
			return ChordTolerance;
		}

		double GetMaxNormalAngle() const
		{
			return MaxNormalAngle;
		}

		double GetMaxEdgeLength() const
		{
			return MaxEdgeLength;
		}

		EStitchingTechnique GetStitchingTechnique() const
		{
			return StitchingTechnique;
		}

		FDatasmithUtils::EModelCoordSystem GetModelCoordSys() const
		{
			return ModelCoordSys;
		}

		void SetModelCoordinateSystem(FDatasmithUtils::EModelCoordSystem NewCoordinateSystem)
		{
			ModelCoordSys = NewCoordinateSystem;
		}

		CADTOOLS_API friend uint32 GetTypeHash(const FImportParameters& ImportParameters);
	};

	namespace SewOption
	{

	static ESewOption GetFromImportParameters()
	{
		ESewOption Option = ESewOption::None;

		if (FImportParameters::bGStitchingForceSew)
		{
			Option |= ESewOption::ForceJoining;
		}

		if (FImportParameters::bGStitchingRemoveThinFaces)
		{
			Option |= ESewOption::RemoveThinFaces;
		}

		if (FImportParameters::bGStitchingRemoveDuplicatedFaces)
		{
			Option |= ESewOption::RemoveDuplicatedFaces;
		}

		return Option;
	}

	} // ns SewOption

	inline FString BuildCadCachePath(const TCHAR* CachePath, uint32 FileHash)
	{
		FString FileName = FString::Printf(TEXT("UEx%08x"), FileHash) + TEXT(".prc");
		return FPaths::Combine(CachePath, TEXT("cad"), FileName);
	}

	inline FString BuildCacheFilePath(const TCHAR* CachePath, const TCHAR* Folder, uint32 BodyHash)
	{
		FString BodyFileName = FString::Printf(TEXT("UEx%08x"), BodyHash);
		FString OutFileName = FPaths::Combine(CachePath, Folder, BodyFileName);

		if (FImportParameters::bGDisableCADKernelTessellation)
		{
			OutFileName += TEXT(".prc");
		}
		else
		{
			OutFileName += TEXT(".ugeom");
		}
		return OutFileName;
	}

	struct FMeshParameters
	{
		bool bNeedSwapOrientation = false;
		bool bIsSymmetric = false;
		FVector3f SymmetricOrigin;
		FVector3f SymmetricNormal;
	};
}
