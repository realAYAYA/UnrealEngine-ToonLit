// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/DerivedDataGeometryCollectionCooker.h"
#include "Chaos/Core.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Serialization/MemoryWriter.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "Chaos/ChaosArchive.h"
#include "UObject/DestructionObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "Chaos/ErrorReporter.h"
#include "EngineUtils.h"

#if WITH_EDITOR

#include "NaniteBuilder.h"

FDerivedDataGeometryCollectionCooker::FDerivedDataGeometryCollectionCooker(UGeometryCollection& InGeometryCollection)
	: GeometryCollection(InGeometryCollection)
{
}

FString FDerivedDataGeometryCollectionCooker::GetDebugContextString() const
{
	return GeometryCollection.GetFullName();
}

bool FDerivedDataGeometryCollectionCooker::Build(TArray<uint8>& OutData)
{
	FMemoryWriter Ar(OutData, true);	// Must be persistent for BulkData to serialize
	Chaos::FChaosArchive ChaosAr(Ar);
	if (FGeometryCollection* Collection = GeometryCollection.GetGeometryCollection().Get())
	{
		FSharedSimulationParameters SharedParams;
		GeometryCollection.GetSharedSimulationParams(SharedParams);

		Chaos::FErrorReporter ErrorReporter(GeometryCollection.GetName());

		BuildSimulationData(ErrorReporter, *Collection, SharedParams);
		Collection->Serialize(ChaosAr);

		if (GeometryCollection.EnableNanite)
		{
			TUniquePtr<FGeometryCollectionNaniteData> NaniteData = UGeometryCollection::CreateNaniteData(Collection);
			NaniteData->Serialize(ChaosAr, &GeometryCollection);
		}
		else
		{
			// No Nanite data, write out zero resources
			int32 NumNaniteResources = 0;
			Ar << NumNaniteResources;
		}

		if (false && ErrorReporter.EncounteredAnyErrors())
		{
			bool bAllErrorsHandled = !ErrorReporter.ContainsUnhandledError();
			ErrorReporter.ReportError(*FString::Printf(TEXT("Could not cook content for Collection:%s"), *GeometryCollection.GetPathName()));
			if (bAllErrorsHandled)
			{
				ErrorReporter.HandleLatestError();
			}
			return false;	//Don't save into DDC if any errors found
		}

		return true;
	}

	return false;
}

const TCHAR* FDerivedDataGeometryCollectionCooker::GetVersionString() const
{
	const TCHAR* VersionString = TEXT("007038F125ED47A4AF928FAC54FD9493");

	static FString CachedNaniteVersionString;
	if (CachedNaniteVersionString.IsEmpty())
	{
		const TCHAR* NaniteVersionString = TEXT("CE9B42F81A5XX388E51GGNC8C19D3E");
		CachedNaniteVersionString = FString::Printf(TEXT("%s_%s_%s"), VersionString, NaniteVersionString, *Nanite::IBuilderModule::Get().GetVersionString());
	}

	return GeometryCollection.EnableNanite ? *CachedNaniteVersionString : VersionString;
}

FString FDerivedDataGeometryCollectionCooker::GetPluginSpecificCacheKeySuffix() const
{
	FString KeySuffix = FString::Printf(
		TEXT("%s_%s_%s_%d_%d"),
		Chaos::ChaosVersionGUID,
		*GeometryCollection.GetIdGuid().ToString(),
		*GeometryCollection.GetStateGuid().ToString(),
		FDestructionObjectVersion::Type::LatestVersion,
		FUE5MainStreamObjectVersion::Type::LatestVersion
	);

#if PLATFORM_CPU_ARM_FAMILY
	// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
	// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
	// b) we can remove it once we get arm64 to be consistent.
	KeySuffix.Append(TEXT("_arm64"));
#endif

	return KeySuffix;
}

#endif // WITH_EDITOR
