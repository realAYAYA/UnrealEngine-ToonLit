// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeoReferencingSystem.h"

#include "DrawDebugHelpers.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MathUtil.h"
 
#include "HAL/PlatformFileManager.h"
#include "IPlatformFilePak.h"

#include "UFSProjSupport.h"

THIRD_PARTY_INCLUDES_START
#include "sqlite3.h"
#include "proj.h"
THIRD_PARTY_INCLUDES_END

#define ECEF_EPSG_FSTRING FString(TEXT("EPSG:4978"))

// LWC_TODO - To be replaced once FVector::Normalize will use a smaller number than 1e-8
#define GEOREF_DOUBLE_SMALL_NUMBER			(1.e-50)




AGeoReferencingSystem* AGeoReferencingSystem::GetGeoReferencingSystem(UObject* WorldContextObject)
{
	AGeoReferencingSystem* Actor = nullptr;

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsOfClass(World, AGeoReferencingSystem::StaticClass(), Actors);
		int NbActors = Actors.Num();
		if (NbActors == 0)
		{
			UE_LOG(LogGeoReferencing, Error, TEXT("GeoReferencingSystem actor not found. Please add one to your world to configure your geo referencing system."));
		}
		else if (NbActors > 1)
		{
			UE_LOG(LogGeoReferencing, Error, TEXT("Multiple GeoReferencingSystem actors found. Only one actor should be used to configure your geo referencing system"));
		}
		else
		{
			Actor = Cast<AGeoReferencingSystem>(Actors[0]);
		}
	}

	return Actor;
}

class AGeoReferencingSystem::FGeoReferencingSystemInternals
{
public:
	FGeoReferencingSystemInternals()
		: ProjContext(nullptr)
		, ProjProjectedToGeographic(nullptr)
		, ProjProjectedToECEF(nullptr)
		, ProjGeographicToECEF(nullptr)
	{
	}

	// Private PROJ Utilities
	void InitPROJLibrary();
	void DeInitPROJLibrary();
	PJ* GetPROJProjection(FString SourceCRS, FString DestinationCRS);
	bool GetEllipsoid(FString CRSString, FEllipsoid& Ellipsoid);
	
	FMatrix GetWorldFrameToECEFFrame(const FEllipsoid& Ellipsoid, const FVector& ECEFLocation);

	PJ_CONTEXT* ProjContext;
	PJ* ProjProjectedToGeographic;
	PJ* ProjProjectedToECEF;
	PJ* ProjGeographicToECEF;
	FEllipsoid ProjectedEllipsoid;
	FEllipsoid GeographicEllipsoid;

	// Transformation caches 
	// Flat Planet
	FVector WorldOriginLocationProjected; // Offset between the UE world and the Projected CRS Origin. (Expressed in ProjectedCRS units).

	// Round Planet
	FMatrix WorldFrameToECEFFrame; // Matrix to transform a vector from EU to ECEF CRS
	FMatrix ECEFFrameToWorldFrame; // Matrix to transform a vector from ECEF to UE CRS - Inverse of the previous one kept in cache. 

	FMatrix WorldFrameToUEFrame;
	FMatrix UEFrameToWorldFrame;
};

/////// INIT / DEINIT

void AGeoReferencingSystem::PostLoad()
{
	Super::PostLoad();

	Initialize();
}

void AGeoReferencingSystem::PostActorCreated()
{
	Super::PostActorCreated();

	Initialize();
}

void AGeoReferencingSystem::Initialize()
{
	Impl = MakePimpl<FGeoReferencingSystemInternals>();

	Impl->InitPROJLibrary();

	// Should we consider other conventions ? Or North Offset like in SunPosition?
	Impl->WorldFrameToUEFrame = FMatrix( 
		FVector(1.0, 0.0, 0.0),		// Easting (X) is UE World X
		FVector(0.0, -1.0, 0.0),	// Northing (Y) is UE World -Y because of left-handed convention
		FVector(0.0, 0.0, 1.0),		// Up (Z) is UE World Z 
		FVector(0.0, 0.0, 0.0));	// No Origin offset


	Impl->UEFrameToWorldFrame = Impl->WorldFrameToUEFrame.Inverse();

	ApplySettings();
}

void AGeoReferencingSystem::BeginDestroy()
{
	Super::BeginDestroy();

	if (Impl)
	{
		Impl->DeInitPROJLibrary();
	}
}

#pragma region Old deprecated Prototypes

void AGeoReferencingSystem::EngineToProjected(const FVector& EngineCoordinates, FCartesianCoordinates& ProjectedCoordinates)
{
	FVector Result;
	EngineToProjected(EngineCoordinates, Result);
	ProjectedCoordinates = FCartesianCoordinates(Result);
}

void AGeoReferencingSystem::ProjectedToEngine(const FCartesianCoordinates& ProjectedCoordinates, FVector& EngineCoordinates)
{
	ProjectedToEngine(ProjectedCoordinates.ToVector(), EngineCoordinates);
}

void AGeoReferencingSystem::EngineToECEF(const FVector& EngineCoordinates, FCartesianCoordinates& ECEFCoordinates)
{
	FVector Result;
	EngineToECEF(EngineCoordinates, Result);
	ECEFCoordinates = FCartesianCoordinates(Result);
}

void AGeoReferencingSystem::ECEFToEngine(const FCartesianCoordinates& ECEFCoordinates, FVector& EngineCoordinates)
{
	ECEFToEngine(ECEFCoordinates.ToVector(), EngineCoordinates);
}


void AGeoReferencingSystem::ProjectedToGeographic(const FCartesianCoordinates& ProjectedCoordinates, FGeographicCoordinates& GeographicCoordinates)
{
	ProjectedToGeographic(ProjectedCoordinates.ToVector(), GeographicCoordinates);
}

void AGeoReferencingSystem::GeographicToProjected(const FGeographicCoordinates& GeographicCoordinates, FCartesianCoordinates& ProjectedCoordinates)
{
	FVector Result;
	GeographicToProjected(GeographicCoordinates, Result);
	ProjectedCoordinates = FCartesianCoordinates(Result);
}

void AGeoReferencingSystem::ProjectedToECEF(const FCartesianCoordinates& ProjectedCoordinates, FCartesianCoordinates& ECEFCoordinates)
{
	FVector Result;
	ProjectedToECEF(ProjectedCoordinates.ToVector(), Result);
	ECEFCoordinates = FCartesianCoordinates(Result);
}

void AGeoReferencingSystem::ECEFToProjected(const FCartesianCoordinates& ECEFCoordinates, FCartesianCoordinates& ProjectedCoordinates)
{
	FVector Result;
	ECEFToProjected(ECEFCoordinates.ToVector(), Result);
	ProjectedCoordinates = FCartesianCoordinates(Result);
}

void AGeoReferencingSystem::GeographicToECEF(const FGeographicCoordinates& GeographicCoordinates, FCartesianCoordinates& ECEFCoordinates)
{
	FVector Result;
	GeographicToECEF(GeographicCoordinates, Result);
	ECEFCoordinates = FCartesianCoordinates(Result);
}

void AGeoReferencingSystem::ECEFToGeographic(const FCartesianCoordinates& ECEFCoordinates, FGeographicCoordinates& GeographicCoordinates)
{
	ECEFToGeographic(ECEFCoordinates.ToVector(), GeographicCoordinates);
}

void AGeoReferencingSystem::GetENUVectorsAtProjectedLocation(const FCartesianCoordinates& ProjectedCoordinates, FVector& East, FVector& North, FVector& Up)
{
	GetENUVectorsAtProjectedLocation(ProjectedCoordinates.ToVector(), East, North, Up);
}

void AGeoReferencingSystem::GetENUVectorsAtECEFLocation(const FCartesianCoordinates& ECEFCoordinates, FVector& East, FVector& North, FVector& Up)
{
	GetENUVectorsAtECEFLocation(ECEFCoordinates.ToVector(), East, North, Up);
}

void AGeoReferencingSystem::GetECEFENUVectorsAtECEFLocation(const FCartesianCoordinates& ECEFCoordinates, FVector& ECEFEast, FVector& ECEFNorth, FVector& ECEFUp)
{
	GetECEFENUVectorsAtECEFLocation(ECEFCoordinates.ToVector(), ECEFEast, ECEFNorth, ECEFUp);
}

FTransform AGeoReferencingSystem::GetTangentTransformAtProjectedLocation(const FCartesianCoordinates& ProjectedCoordinates)
{
	return GetTangentTransformAtProjectedLocation(ProjectedCoordinates.ToVector());
}

FTransform AGeoReferencingSystem::GetTangentTransformAtECEFLocation(const FCartesianCoordinates& ECEFCoordinates)
{
	return GetTangentTransformAtECEFLocation(ECEFCoordinates.ToVector());
}
#pragma endregion


void AGeoReferencingSystem::EngineToProjected(const FVector& EngineCoordinates, FVector& ProjectedCoordinates)
{
	switch (PlanetShape)
	{
	case EPlanetShape::RoundPlanet:
	{
		// In RoundPlanet, we have to go through the ECEF transform as an intermediate step. 
		FVector ECEFCoordinates;
		EngineToECEF(EngineCoordinates, ECEFCoordinates);
		ECEFToProjected(ECEFCoordinates, ProjectedCoordinates);
	}
	break;

	case EPlanetShape::FlatPlanet:
	default:
	{
		// in FlatPlanet, the transform is simply a translation
		// Before any conversion, consider the internal UE rebasing
		FIntVector UERebasingOffset = GetWorld()->OriginLocation;
		FVector UERebasedCoordinates(
			EngineCoordinates.X + UERebasingOffset.X,
			EngineCoordinates.Y + UERebasingOffset.Y,
			EngineCoordinates.Z + UERebasingOffset.Z);

		// Convert UE units to meters, invert the Y coordinate because of left-handed UE Frame
		FVector UEWorldCoordinates = UERebasedCoordinates * FVector(0.01, -0.01, 0.01);

		// Add the defined origin offset
		ProjectedCoordinates = UEWorldCoordinates + Impl->WorldOriginLocationProjected;
	}
	break;
	}
}

void AGeoReferencingSystem::ProjectedToEngine(const FVector& ProjectedCoordinates, FVector& EngineCoordinates)
{
	switch (PlanetShape)
	{
	case EPlanetShape::RoundPlanet:
	{
		// In RoundPlanet, we have to go through the ECEF transform as an intermediate step. 
		FVector ECEFCoordinates;
		ProjectedToECEF(ProjectedCoordinates, ECEFCoordinates);
		ECEFToEngine(ECEFCoordinates, EngineCoordinates);
	}
	break;

	case EPlanetShape::FlatPlanet:
	default:
	{
		// in FlatPlanet, the transform is simply a translation
		// Remove the Origin location, and convert to UE Units, while inverting the Z Axis
		FVector UEWorldCoordinates = (ProjectedCoordinates - Impl->WorldOriginLocationProjected);

		// Convert UE units to meters, invert the Y coordinate because of left-handed UE Frame
		FVector UERebasedCoordinates = UEWorldCoordinates * FVector(100.0, -100.0, 100.0);

		// Consider the UE internal rebasing
		FIntVector UERebasingOffset = GetWorld()->OriginLocation;
		EngineCoordinates = UERebasedCoordinates - FVector(UERebasingOffset.X, UERebasingOffset.Y, UERebasingOffset.Z);
	}
	break;
	}
}

void AGeoReferencingSystem::EngineToECEF(const FVector& EngineCoordinates, FVector& ECEFCoordinates)
{
	switch (PlanetShape)
	{
	case EPlanetShape::RoundPlanet:
	{
		// Before any conversion, consider the internal UE rebasing
		FIntVector UERebasingOffset = GetWorld()->OriginLocation;
		FVector UERebasedCoordinates(
			EngineCoordinates.X + UERebasingOffset.X,
			EngineCoordinates.Y + UERebasingOffset.Y,
			EngineCoordinates.Z + UERebasingOffset.Z);

		// Convert UE units to meters, invert the Y coordinate because of left-handed UE Frame
		FVector UEWorldCoordinates = UERebasedCoordinates * FVector(0.01, -0.01, 0.01);

		if (bOriginAtPlanetCenter)
		{
			// Easy case, UE is ECEF... And we did the rebasing, so return the Global coordinates
			ECEFCoordinates = UEWorldCoordinates; // TOCHECK Types
		}
		else
		{
			ECEFCoordinates = Impl->WorldFrameToECEFFrame.TransformPosition(UEWorldCoordinates);
		}
	}
	break;

	case EPlanetShape::FlatPlanet:
	default:
	{
		// In FlatPlanet, we have to go through the Projected transform as an intermediate step. 
		FVector ProjectedCoordinates;
		EngineToProjected(EngineCoordinates, ProjectedCoordinates);
		ProjectedToECEF(ProjectedCoordinates, ECEFCoordinates);
	}
	break;
	}
}

void AGeoReferencingSystem::ECEFToEngine(const FVector& ECEFCoordinates, FVector& EngineCoordinates)
{
	switch (PlanetShape)
	{
	case EPlanetShape::RoundPlanet:
	{
		FVector UEWorldCoordinates;
		if (bOriginAtPlanetCenter)
		{
			// Easy case, UE is ECEF... And we did the rebasing, so return the Global coordinates
			UEWorldCoordinates = ECEFCoordinates;
		}
		else
		{
			UEWorldCoordinates = Impl->ECEFFrameToWorldFrame.TransformPosition(ECEFCoordinates);
		}

		// Convert UE units to meters, invert the Y coordinate because of left-handed UE Frame
		FVector UERebasedCoordinates = UEWorldCoordinates * FVector(100.0, -100.0, 100.0);

		// Consider the UE internal rebasing
		FIntVector UERebasingOffset = GetWorld()->OriginLocation;
		EngineCoordinates = UERebasedCoordinates - FVector(UERebasingOffset.X, UERebasingOffset.Y, UERebasingOffset.Z);
	}
	break;

	case EPlanetShape::FlatPlanet:
	default:
	{
		// In FlatPlanet, we have to go through the Projected transform as an intermediate step. 
		FVector ProjectedCoordinates;
		ECEFToProjected(ECEFCoordinates, ProjectedCoordinates);
		ProjectedToEngine(ProjectedCoordinates, EngineCoordinates);
	}
	break;
	}
}


void AGeoReferencingSystem::EngineToGeographic(const FVector& EngineCoordinates, FGeographicCoordinates& GeographicCoordinates)
{
	switch (PlanetShape)
	{
		case EPlanetShape::RoundPlanet:
		{
			FVector ECEFCoordinates;
			EngineToECEF(EngineCoordinates, ECEFCoordinates);
			ECEFToGeographic(ECEFCoordinates, GeographicCoordinates);
		}
	break;

		case EPlanetShape::FlatPlanet:
		default:
		{
			FVector ProjectedCoordinates;
			EngineToProjected(EngineCoordinates, ProjectedCoordinates);
			ProjectedToGeographic(ProjectedCoordinates, GeographicCoordinates);

		}
	break;
	}
}

void AGeoReferencingSystem::GeographicToEngine(const FGeographicCoordinates& GeographicCoordinates, FVector& EngineCoordinates)
{
	switch (PlanetShape)
	{
		case EPlanetShape::RoundPlanet:
		{
			FVector ECEFCoordinates;
			GeographicToECEF(GeographicCoordinates, ECEFCoordinates);
			ECEFToEngine(ECEFCoordinates, EngineCoordinates);
		}
	break;

		case EPlanetShape::FlatPlanet:
		default:
		{
			FVector ProjectedCoordinates;
			GeographicToProjected(GeographicCoordinates,ProjectedCoordinates);
			ProjectedToEngine(ProjectedCoordinates, EngineCoordinates);
		}
	break;
	}
}


void AGeoReferencingSystem::ProjectedToGeographic(const FVector& ProjectedCoordinates, FGeographicCoordinates& GeographicCoordinates)
{
	PJ_COORD input, output;
	input = proj_coord(ProjectedCoordinates.X, ProjectedCoordinates.Y, ProjectedCoordinates.Z, 0);

	output = proj_trans(Impl->ProjProjectedToGeographic, PJ_FWD, input);
	GeographicCoordinates.Latitude = output.lpz.phi;
	GeographicCoordinates.Longitude = output.lpz.lam;
	GeographicCoordinates.Altitude = output.lpz.z;
}

void AGeoReferencingSystem::GeographicToProjected(const FGeographicCoordinates& GeographicCoordinates, FVector& ProjectedCoordinates)
{
	PJ_COORD input, output;
	input = proj_coord(GeographicCoordinates.Longitude, GeographicCoordinates.Latitude, GeographicCoordinates.Altitude, 0);

	output = proj_trans(Impl->ProjProjectedToGeographic, PJ_INV, input);
	ProjectedCoordinates.X = output.xyz.x;
	ProjectedCoordinates.Y = output.xyz.y;
	ProjectedCoordinates.Z = output.xyz.z;
}

void AGeoReferencingSystem::ProjectedToECEF(const FVector& ProjectedCoordinates, FVector& ECEFCoordinates)
{
	PJ_COORD input, output;
	input = proj_coord(ProjectedCoordinates.X, ProjectedCoordinates.Y, ProjectedCoordinates.Z, 0);

	output = proj_trans(Impl->ProjProjectedToECEF, PJ_FWD, input);
	ECEFCoordinates.X = output.xyz.x;
	ECEFCoordinates.Y = output.xyz.y;
	ECEFCoordinates.Z = output.xyz.z;
}

void AGeoReferencingSystem::ECEFToProjected(const FVector& ECEFCoordinates, FVector& ProjectedCoordinates)
{
	PJ_COORD input, output;
	input = proj_coord(ECEFCoordinates.X, ECEFCoordinates.Y, ECEFCoordinates.Z, 0);

	output = proj_trans(Impl->ProjProjectedToECEF, PJ_INV, input);
	ProjectedCoordinates.X = output.xyz.x;
	ProjectedCoordinates.Y = output.xyz.y;
	ProjectedCoordinates.Z = output.xyz.z;
}

void AGeoReferencingSystem::GeographicToECEF(const FGeographicCoordinates& GeographicCoordinates, FVector& ECEFCoordinates)
{
	PJ_COORD input, output;
	input = proj_coord(GeographicCoordinates.Longitude, GeographicCoordinates.Latitude, GeographicCoordinates.Altitude, 0);

	output = proj_trans(Impl->ProjGeographicToECEF, PJ_FWD, input);
	ECEFCoordinates.X = output.xyz.x;
	ECEFCoordinates.Y = output.xyz.y;
	ECEFCoordinates.Z = output.xyz.z;
}

void AGeoReferencingSystem::ECEFToGeographic(const FVector& ECEFCoordinates, FGeographicCoordinates& GeographicCoordinates)
{
	PJ_COORD input, output;
	input = proj_coord(ECEFCoordinates.X, ECEFCoordinates.Y, ECEFCoordinates.Z, 0);

	output = proj_trans(Impl->ProjGeographicToECEF, PJ_INV, input);
	GeographicCoordinates.Latitude = output.lpz.phi;
	GeographicCoordinates.Longitude = output.lpz.lam;
	GeographicCoordinates.Altitude = output.lpz.z;
}

// ENU & Transforms

void AGeoReferencingSystem::GetENUVectorsAtEngineLocation(const FVector& EngineCoordinates, FVector& East, FVector& North, FVector& Up)
{
	FVector ECEFLocation;
	EngineToECEF(EngineCoordinates, ECEFLocation);
	GetENUVectorsAtECEFLocation(ECEFLocation, East, North, Up);
}

void AGeoReferencingSystem::GetENUVectorsAtProjectedLocation(const FVector& ProjectedCoordinates, FVector& East, FVector& North, FVector& Up)
{
	FVector ECEFLocation;
	ProjectedToECEF(ProjectedCoordinates, ECEFLocation);
	GetENUVectorsAtECEFLocation(ECEFLocation, East, North, Up);
}

void AGeoReferencingSystem::GetENUVectorsAtGeographicLocation(const FGeographicCoordinates& GeographicCoordinates, FVector& East, FVector& North, FVector& Up)
{
	FVector ECEFLocation;
	GeographicToECEF(GeographicCoordinates, ECEFLocation);
	GetENUVectorsAtECEFLocation(ECEFLocation, East, North, Up);
}

void AGeoReferencingSystem::GetENUVectorsAtECEFLocation(const FVector& ECEFCoordinates, FVector& East, FVector& North, FVector& Up)
{
	// Compute Tangent matrix at ECEF location
	FEllipsoid& Ellipsoid = Impl->GeographicEllipsoid;

	if (bOriginLocationInProjectedCRS)
	{
		Ellipsoid = Impl->ProjectedEllipsoid;
	}
	FMatrix WorldFrameToECEFFrameAtLocation;
	WorldFrameToECEFFrameAtLocation = Impl->GetWorldFrameToECEFFrame(Ellipsoid, ECEFCoordinates);

	switch (PlanetShape)
	{
	case EPlanetShape::RoundPlanet:
	{
		FMatrix UEtoECEF = WorldFrameToECEFFrameAtLocation * Impl->ECEFFrameToWorldFrame * Impl->UEFrameToWorldFrame;
		UEtoECEF.GetUnitAxes(East, North, Up);
	}
	break;

	case EPlanetShape::FlatPlanet:
	default:
		// PROJ don't provide anything to project direction vectors. Let's do it by hand...
		FVector EasternPoint = ECEFCoordinates + WorldFrameToECEFFrameAtLocation.TransformVector(FVector(1.0, 0.0, 0.0)); // 1m from origin to the East
		FVector NorthernPoint = ECEFCoordinates + WorldFrameToECEFFrameAtLocation.TransformVector(FVector(0.0, 1.0, 0.0)); // 1m from origin to the North
		
		FVector ProjectedOrigin, ProjectedEastern, ProjectedNorthern;
		ECEFToProjected(ECEFCoordinates, ProjectedOrigin);
		ECEFToProjected(EasternPoint, ProjectedEastern);
		ECEFToProjected(NorthernPoint, ProjectedNorthern);

		FVector EastDirection(ProjectedEastern.X - ProjectedOrigin.X, ProjectedEastern.Y - ProjectedOrigin.Y, ProjectedEastern.Z - ProjectedOrigin.Z);
		FVector NorthDirection(ProjectedNorthern.X - ProjectedOrigin.X, ProjectedNorthern.Y - ProjectedOrigin.Y, ProjectedNorthern.Z - ProjectedOrigin.Z);

		EastDirection.Normalize(GEOREF_DOUBLE_SMALL_NUMBER);
		NorthDirection.Normalize(GEOREF_DOUBLE_SMALL_NUMBER);

		East = FVector(EastDirection.X, -EastDirection.Y, EastDirection.Z);
		North = FVector(NorthDirection.X, -NorthDirection.Y, NorthDirection.Z);
		Up = FVector::CrossProduct(North, East);
		break;
	}
}

void AGeoReferencingSystem::GetECEFENUVectorsAtECEFLocation(const FVector& ECEFCoordinates, FVector& ECEFEast, FVector& ECEFNorth, FVector& ECEFUp)
{
	// Compute Tangent matrix at ECEF location
	FEllipsoid& Ellipsoid = Impl->GeographicEllipsoid;
	
	if (bOriginLocationInProjectedCRS)
	{
		Ellipsoid = Impl->ProjectedEllipsoid;
	}
	
	FMatrix WorldFrameToECEFFrameAtLocation;
	WorldFrameToECEFFrameAtLocation = Impl->GetWorldFrameToECEFFrame(Ellipsoid, ECEFCoordinates);
	WorldFrameToECEFFrameAtLocation.GetUnitAxes(ECEFEast, ECEFNorth, ECEFUp);
}

FTransform AGeoReferencingSystem::GetTangentTransformAtEngineLocation(const FVector& EngineCoordinates)
{
	FVector ECEFLocation;
	EngineToECEF(EngineCoordinates, ECEFLocation);
	return GetTangentTransformAtECEFLocation(ECEFLocation);
}

FTransform AGeoReferencingSystem::GetTangentTransformAtProjectedLocation(const FVector& ProjectedCoordinates)
{
	FVector ECEFLocation;
	ProjectedToECEF(ProjectedCoordinates, ECEFLocation);
	return GetTangentTransformAtECEFLocation(ECEFLocation);
}

FTransform AGeoReferencingSystem::GetTangentTransformAtGeographicLocation(const FGeographicCoordinates& GeographicCoordinates)
{
	FVector ECEFLocation;
	GeographicToECEF(GeographicCoordinates, ECEFLocation);
	return GetTangentTransformAtECEFLocation(ECEFLocation);
}

FTransform AGeoReferencingSystem::GetTangentTransformAtECEFLocation(const FVector& ECEFCoordinates)
{
	if (PlanetShape == EPlanetShape::RoundPlanet)
	{
		// Compute Tangent matrix at ECEF location
		FEllipsoid& Ellipsoid = Impl->GeographicEllipsoid;
		if (bOriginLocationInProjectedCRS)
		{
			Ellipsoid = Impl->ProjectedEllipsoid;
		}
		FMatrix WorldFrameToECEFFrameAtLocation = Impl->GetWorldFrameToECEFFrame(Ellipsoid, ECEFCoordinates);
		

		FMatrix UEtoECEF = Impl->UEFrameToWorldFrame * WorldFrameToECEFFrameAtLocation * Impl->ECEFFrameToWorldFrame * Impl->WorldFrameToUEFrame; 
		FVector UEOrigin;
		ECEFToEngine(ECEFCoordinates, UEOrigin);
		UEtoECEF.SetOrigin(UEOrigin);

		return FTransform(UEtoECEF);
	}
	else
	{
		FVector East, North, Up, UEOrigin;
		GetENUVectorsAtECEFLocation(ECEFCoordinates, East, North, Up);
		ECEFToEngine(ECEFCoordinates, UEOrigin);
		FMatrix TransformMatrix(East, North, Up, UEOrigin);
		return FTransform(TransformMatrix);
	}
}

FTransform AGeoReferencingSystem::GetPlanetCenterTransform()
{
	// Compute Origin location in ECEF. 
	if (PlanetShape == EPlanetShape::RoundPlanet)
	{
		if (bOriginAtPlanetCenter)
		{
			return FTransform::Identity;
		}
		else
		{
			FMatrix TransformMatrix = Impl->UEFrameToWorldFrame * Impl->ECEFFrameToWorldFrame * Impl->WorldFrameToUEFrame;
			// Don't go to transform yet, we must stay in double to apply the rebasing offset. 

			// Get Origin, and convert UE units to meters, invert the Y coordinate because of left-handed UE Frame
			FVector UEOrigin = TransformMatrix.GetOrigin() * FVector(100.0, -100.0, 100.0);
			
			// Consider the UE internal rebasing to compute Origin
			FIntVector UERebasingOffset = GetWorld()->OriginLocation;
			FVector UERebasedCoordinates = UEOrigin - FVector(UERebasingOffset.X, UERebasingOffset.Y, UERebasingOffset.Z);
			FVector Origin = FVector(UERebasedCoordinates.X, UERebasedCoordinates.Y, UERebasedCoordinates.Z);
			TransformMatrix.SetOrigin(Origin);

			return FTransform(TransformMatrix);
		}
	}
	else
	{
		// Makes not sense in Flat planet mode... 
		return FTransform::Identity;
	}
}

// Public PROJ Utilities

bool AGeoReferencingSystem::IsCRSStringValid(FString CRSString, FString& Error)
{
	if (Impl->ProjContext == nullptr)
	{
		Error = FString("Proj Context has not been initialized");
		return false;
	}

	// Try to create a CRS from this string
	FTCHARToUTF8 Convert(*CRSString);
	const ANSICHAR* UtfString = Convert.Get();
	PJ* CRS = proj_create(Impl->ProjContext, UtfString);

	if (CRS == nullptr)
	{
		int ErrorNumber = proj_context_errno(Impl->ProjContext);
		Error = FString(proj_errno_string(ErrorNumber));
		return false;
	}

	proj_destroy(CRS);
	return true;
}

// Ellipsoid

double AGeoReferencingSystem::GetGeographicEllipsoidMaxRadius()
{
	return Impl->GeographicEllipsoid.GetMaximumRadius();
}

double AGeoReferencingSystem::GetGeographicEllipsoidMinRadius()
{
	return Impl->GeographicEllipsoid.GetMinimumRadius();
}

double AGeoReferencingSystem::GetProjectedEllipsoidMaxRadius()
{
	return Impl->ProjectedEllipsoid.GetMaximumRadius();
}

double AGeoReferencingSystem::GetProjectedEllipsoidMinRadius()
{
	return Impl->ProjectedEllipsoid.GetMinimumRadius();
}

bool AGeoReferencingSystem::FGeoReferencingSystemInternals::GetEllipsoid(FString CRSString, FEllipsoid& Ellipsoid)
{
	FTCHARToUTF8 ConvertCRSString(*CRSString);
	const ANSICHAR* CRS = ConvertCRSString.Get();
	bool bSuccess = true;

	PJ* CRSPJ = proj_create(ProjContext, CRS);
	if (CRSPJ != nullptr)
	{
		PJ* EllipsoidPJ = proj_get_ellipsoid(ProjContext, CRSPJ);
		if (EllipsoidPJ != nullptr)
		{
			double SemiMajorMetre; // semi-major axis in meter
			double SemiMinorMetre; // semi-minor axis in meter
			double InvFlattening;  // inverse flattening.
			int IsSemiMinorComputed; // if the semi-minor value was computed. If FALSE, its value comes from the definition

			if (proj_ellipsoid_get_parameters(ProjContext, EllipsoidPJ, &SemiMajorMetre, &SemiMinorMetre, &IsSemiMinorComputed, &InvFlattening))
			{
				Ellipsoid = FEllipsoid(SemiMajorMetre, SemiMajorMetre, SemiMinorMetre);
			}
			else
			{
				int ErrorNumber = proj_context_errno(ProjContext);
				FString ProjError = FString(proj_errno_string(ErrorNumber));
				UE_LOG(LogGeoReferencing, Error, TEXT("AGeoReferencingSystem::GetEllipsoid failed in proj_ellipsoid_get_parameters : %s "), *ProjError);
				bSuccess = false;
			}

			proj_destroy(EllipsoidPJ);
		}
		else
		{
			int ErrorNumber = proj_context_errno(ProjContext);
			FString ProjError = FString(proj_errno_string(ErrorNumber));
			UE_LOG(LogGeoReferencing, Error, TEXT("AGeoReferencingSystem::GetEllipsoid failed in proj_get_ellipsoid : %s "), *ProjError);
			bSuccess = false;
		}

		proj_destroy(CRSPJ);
	}
	else
	{
		int ErrorNumber = proj_context_errno(ProjContext);
		FString ProjError = FString(proj_errno_string(ErrorNumber));
		UE_LOG(LogGeoReferencing, Error, TEXT("AGeoReferencingSystem::GetEllipsoid failed in proj_create : %s "), *ProjError);
		UE_LOG(LogGeoReferencing, Display, TEXT("CRSString was : %s "), *CRSString);
		bSuccess = false;
	}

	return bSuccess;
}

void AGeoReferencingSystem::ApplySettings()
{
	// Apply Projection settings

	// Projected -> Geographic
	if (Impl->ProjProjectedToGeographic != nullptr)
	{
		proj_destroy(Impl->ProjProjectedToGeographic);
	}
	Impl->ProjProjectedToGeographic = Impl->GetPROJProjection(ProjectedCRS, GeographicCRS);

	// Projected -> Geocentric
	if (Impl->ProjProjectedToECEF != nullptr)
	{
		proj_destroy(Impl->ProjProjectedToECEF);
	}
	Impl->ProjProjectedToECEF = Impl->GetPROJProjection(ProjectedCRS, ECEF_EPSG_FSTRING);

	// Geographic -> Geocentric
	if (Impl->ProjGeographicToECEF != nullptr)
	{
		proj_destroy(Impl->ProjGeographicToECEF);
	}
	Impl->ProjGeographicToECEF = Impl->GetPROJProjection(GeographicCRS, ECEF_EPSG_FSTRING);

	bool bProjectedEllipsoidSuccess = Impl->GetEllipsoid(ProjectedCRS, Impl->ProjectedEllipsoid);
	bool bGeographicEllipsoidSuccess = Impl->GetEllipsoid(GeographicCRS, Impl->GeographicEllipsoid);

	bool bSuccess = Impl->ProjProjectedToGeographic != nullptr && Impl->ProjProjectedToECEF != nullptr && Impl->ProjGeographicToECEF != nullptr && bProjectedEllipsoidSuccess && bGeographicEllipsoidSuccess;

#if WITH_EDITOR
	if (!bSuccess)
	{
		// Show an error notification
		const FText NotificationErrorText = NSLOCTEXT("GeoReferencing", "GeoReferencingCRSError", "Error in one CRS definition string - Check log");
		FNotificationInfo Info(NotificationErrorText);
		Info.ExpireDuration = 2.0f;
		Info.bUseSuccessFailIcons = true;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}
#endif

	// Apply Origin settings

	switch (PlanetShape)
	{
	case EPlanetShape::RoundPlanet:
		// Matrice UE to ECEF :
		if (bOriginAtPlanetCenter)
		{
			// Theoritically, we should never use this Identity matrices since the transformations already handle that using a shorter code path, but let's keep consistency  
			Impl->ECEFFrameToWorldFrame.SetIdentity();
			Impl->WorldFrameToECEFFrame.SetIdentity();
		}
		else
		{
			// We need to compute the ENU Vectors at the origin point. For that, the origin has to be expressed first in ECEF. 
			// Express origin in ECEF, and get the ENU vectors
			FVector ECEFOrigin;
			Impl->WorldFrameToECEFFrame.SetIdentity();
			if (bOriginLocationInProjectedCRS)
			{
				FVector ProjectedOrigin(
					OriginProjectedCoordinatesEasting,
					OriginProjectedCoordinatesNorthing,
					OriginProjectedCoordinatesUp);
				ProjectedToECEF(ProjectedOrigin, ECEFOrigin);

				Impl->WorldFrameToECEFFrame = Impl->GetWorldFrameToECEFFrame(Impl->ProjectedEllipsoid, ECEFOrigin);
			}
			else
			{
				GeographicToECEF(FGeographicCoordinates(OriginLongitude, OriginLatitude, OriginAltitude), ECEFOrigin);
				Impl->WorldFrameToECEFFrame = Impl->GetWorldFrameToECEFFrame(Impl->GeographicEllipsoid, ECEFOrigin);
			}
			Impl->ECEFFrameToWorldFrame = Impl->WorldFrameToECEFFrame.Inverse();
		}
		break;

	case EPlanetShape::FlatPlanet:
	default:
		if (bOriginLocationInProjectedCRS)
		{
			// World origin is expressed using Projected coordinates. Take them as is (in double)
			Impl->WorldOriginLocationProjected = FVector(
				OriginProjectedCoordinatesEasting,
				OriginProjectedCoordinatesNorthing,
				OriginProjectedCoordinatesUp);
		}
		else
		{
			// World origin is expressed using Geographic coordinates. Convert them to the projected CRS to have the offset
			FVector OriginProjected;
			GeographicToProjected(FGeographicCoordinates(OriginLongitude, OriginLatitude, OriginAltitude), OriginProjected);
			Impl->WorldOriginLocationProjected = FVector(OriginProjected.X, OriginProjected.Y, OriginProjected.Z);
		}
		break;
	}
	return;
}

static void ProjLog(void* app_data, int level, const char* message)
{
	FUTF8ToTCHAR Message (message);
	switch (level)
	{
	case PJ_LOG_ERROR:
		UE_LOG(LogGeoReferencing, Error, TEXT("%s"), Message.Get());
		break;
	case PJ_LOG_DEBUG:
		UE_LOG(LogGeoReferencing, Verbose, TEXT("%s"), Message.Get());
		break;
	case PJ_LOG_TRACE:
		UE_LOG(LogGeoReferencing, VeryVerbose, TEXT("%s"), Message.Get());
		break;
	}
}

void AGeoReferencingSystem::FGeoReferencingSystemInternals::InitPROJLibrary()
{
	// Initialize proj context
	ProjContext = proj_context_create();
	if (ProjContext == nullptr)
	{
		UE_LOG(LogGeoReferencing, Error, TEXT("proj_context_create() failed"));
		return;
	}

	// Connect PROJ logging
	proj_log_func(ProjContext, nullptr, ProjLog);
	proj_log_level(ProjContext, PJ_LOG_TRACE);

	// Calculate and register the search path to the PROJ data
	FString PluginBaseDir = IPluginManager::Get().FindPlugin("GeoReferencing")->GetBaseDir();
	FString ProjDataPath = FPaths::Combine(*PluginBaseDir, TEXT("Resources/PROJ"));
	FTCHARToUTF8 Utf8ProjDataPath(*ProjDataPath);
	const char* ProjSearchPaths[] =
	{
		Utf8ProjDataPath.Get(),
	};
	proj_context_set_search_paths(ProjContext, sizeof(ProjSearchPaths)/sizeof(ProjSearchPaths[0]), ProjSearchPaths);

	proj_context_set_autoclose_database(ProjContext, true);
	// Non-editor builds use UFS extensions to read PROJ data from UFS/Pak
	if (!GIsEditor)
	{
		// Connect the UFS support for SQLite to PROJ
		proj_context_set_sqlite3_vfs_name(ProjContext, "unreal-fs");

		// Setup UFS for PROJ
		if (!proj_context_set_fileapi(ProjContext, &FUFSProj::FunctionTable, nullptr))
		{
			UE_LOG(LogGeoReferencing, Error, TEXT("proj_context_set_fileapi() failed"));
		}
	}
}

void AGeoReferencingSystem::FGeoReferencingSystemInternals::DeInitPROJLibrary()
{
	// Destroy projections
	if (ProjProjectedToGeographic != nullptr)
	{
		proj_destroy(ProjProjectedToGeographic);
		ProjProjectedToGeographic = nullptr;
	}
	if (ProjProjectedToECEF != nullptr)
	{
		proj_destroy(ProjProjectedToECEF);
		ProjProjectedToECEF = nullptr;
	}
	if (ProjGeographicToECEF != nullptr)
	{
		proj_destroy(ProjGeographicToECEF);
		ProjGeographicToECEF = nullptr;
	}

	// Destroy proj context
	if (ProjContext != nullptr)
	{
		proj_context_destroy(ProjContext); /* may be omitted in the single threaded case */
		ProjContext = nullptr;
	}
}

PJ* AGeoReferencingSystem::FGeoReferencingSystemInternals::GetPROJProjection(FString SourceCRS, FString DestinationCRS)
{
	FTCHARToUTF8 ConvertSource(*SourceCRS);
	FTCHARToUTF8 ConvertDestination(*DestinationCRS);
	const ANSICHAR* Source = ConvertSource.Get();
	const ANSICHAR* Destination = ConvertDestination.Get();

	PJ* TempPJ = proj_create_crs_to_crs(ProjContext, Source, Destination, nullptr);
	if (TempPJ == nullptr)
	{
		int ErrorNumber = proj_context_errno(ProjContext);
		FString ProjError = FString(proj_errno_string(ErrorNumber));
		UE_LOG(LogGeoReferencing, Error, TEXT("AGeoReferencingSystem::BuildProjection failed in proj_create_crs_to_crs : %s "), *ProjError);

		UE_LOG(LogGeoReferencing, Display, TEXT("SourceCRS was : %s "), *SourceCRS);
		UE_LOG(LogGeoReferencing, Display, TEXT("DestinationCRS was : %s "), *DestinationCRS);

		return nullptr;
	}

	/* This will ensure that the order of coordinates for the input CRS */
	/* will be longitude, latitude, whereas EPSG:4326 mandates latitude, longitude */
	PJ* P_for_GIS = proj_normalize_for_visualization(ProjContext, TempPJ);
	if (P_for_GIS == nullptr)
	{
		int ErrorNumber = proj_context_errno(ProjContext);
		FString ProjError = FString(proj_errno_string(ErrorNumber));
		UE_LOG(LogGeoReferencing, Error, TEXT("AGeoReferencingSystem::BuildProjection failed in proj_normalize_for_visualization : %s "), *ProjError);
	}

	proj_destroy(TempPJ);
	return P_for_GIS;
}

FMatrix AGeoReferencingSystem::FGeoReferencingSystemInternals::GetWorldFrameToECEFFrame(const FEllipsoid& Ellipsoid, const FVector& ECEFLocation)
{
	// See ECEF standard : https://commons.wikimedia.org/wiki/File:ECEF_ENU_Longitude_Latitude_right-hand-rule.svg
	if (FMathd::Abs(ECEFLocation.X) < FMathd::Epsilon &&
		FMathd::Abs(ECEFLocation.Y) < FMathd::Epsilon)
	{
		// Special Case - On earth axis... 
		double Sign = 1.0;
		if (FMathd::Abs(ECEFLocation.Z) < FMathd::Epsilon)
		{
			// At origin - Should not happen, but consider it's the same as north pole
			// Leave Sign = 1
		}
		else
		{
			// At South or North pole - Axis are set to be continuous with other points
			Sign = FMathd::SignNonZero(ECEFLocation.Z);
		}

		return FMatrix(
			FVector::YAxisVector, 			// East = Y
			-FVector::XAxisVector * Sign,	// North = Sign * X
			FVector::ZAxisVector * Sign,	// Up = Sign*Z
			ECEFLocation);
	}
	else
	{
		FVector Up = Ellipsoid.GeodeticSurfaceNormal(ECEFLocation);
		FVector East(-ECEFLocation.Y, ECEFLocation.X, 0.0); 
		East.Normalize(GEOREF_DOUBLE_SMALL_NUMBER);
		FVector North = Up.Cross(East);
		return FMatrix(	East, North, Up, ECEFLocation);
	}
}

#if WITH_EDITOR
void AGeoReferencingSystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get the name of the property that was changed  
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, ProjectedCRS) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, GeographicCRS) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, bOriginAtPlanetCenter) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, bOriginLocationInProjectedCRS) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, OriginLatitude) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, OriginLongitude) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, OriginAltitude) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, OriginProjectedCoordinatesEasting) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, OriginProjectedCoordinatesNorthing) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, OriginProjectedCoordinatesUp) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AGeoReferencingSystem, PlanetShape))
	{
		ApplySettings();
	}

	// Call the base class version  
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif