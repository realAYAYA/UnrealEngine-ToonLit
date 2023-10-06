// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementUObjectPackagePathToColumnQueries.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "UObject/Package.h"

void UTypedElementUObjectPackagePathFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync UObject package info to columns"),
			FObserver(FObserver::EEvent::Add, FTypedElementUObjectColumn::StaticStruct())
				.ForceToGameThread(true),
			[](const FTypedElementUObjectColumn& Object, FTypedElementPackagePathColumn& Path, FTypedElementPackageLoadedPathColumn& LoadedPath)
			{
				if (const UObject* ObjectInstance = Object.Object.Get(); ObjectInstance != nullptr)
				{
					const UPackage* Target = ObjectInstance->GetPackage();
					Target->GetPathName(nullptr, Path.Path);
					LoadedPath.LoadedPath = Target->GetLoadedPath();
				}
			}
		)
		.Compile()
	);
}
