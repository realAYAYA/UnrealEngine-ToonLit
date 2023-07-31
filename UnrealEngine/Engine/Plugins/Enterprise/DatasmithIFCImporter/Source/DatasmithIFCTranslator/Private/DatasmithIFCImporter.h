// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "MeshDescription.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

#include "IFC/IFCReader.h"

struct FDatasmithImportContext;
struct FAnalyticsEventAttribute;
class UDatasmithIFCImportOptions;
class UDatasmithStaticMeshIFCImportData;
class IDatasmithMeshActorElement;
class IDatasmithMeshElement;
class IDatasmithBaseMaterialElement;
class IDatasmithActorElement;
class IDatasmithScene;
class IDatasmithLevelSequenceElement;

namespace IFC
{
	class FFileReader;
	class FStaticMeshFactory;
}

DECLARE_LOG_CATEGORY_EXTERN(LogDatasmithIFCImport, Log, All);

class FDatasmithIFCImporter : FNoncopyable
{
public:
	FDatasmithIFCImporter(TSharedRef<IDatasmithScene>& OutScene, UDatasmithIFCImportOptions* InOptions);
	~FDatasmithIFCImporter();

	/** Updates the used import options to InOptions */
	void SetImportOptions(UDatasmithIFCImportOptions* InOptions);

	/** Returns any logged messages and clears them afterwards. */
	const TArray<IFC::FLogMessage>& GetLogMessages() const;

	/** Open and load a scene file. */
	bool OpenFile(const FString& InFileName);

	/** Finalize import of the scene into the engine. */
	bool SendSceneToDatasmith();

	/** Set the analytics attributes specific to this factory */
	void SetupCustomAnalyticsAttributes(TArray<FAnalyticsEventAttribute>& EventAttributes) const;

	/**  Return all FMeshDescriptions that have been created for a particular imported IDatasmithMeshElement */
	void GetGeometriesForMeshElementAndRelease(const TSharedRef<IDatasmithMeshElement> MeshElement, TArray<FMeshDescription>& OutMeshDescriptions);

	/** Mesh global ID used when reimport. */
	FString GetMeshGlobalId(const TSharedRef<IDatasmithMeshElement> MeshElement);

	/** Clean up any unused memory or data. */
	void UnloadScene();

private:
	FString GetFilteredName(const FString& Name);
	FString GetUniqueName(const FString& Name);
	FString ConvertGlobalIDToName(const FString& GlobalId); // IFC global id is unique but it contains illegal characters($) and case-sensitive. Converting is to HEX solves these issues.

	TSharedPtr<IDatasmithMeshActorElement> CreateStaticMeshActor(const IFC::FObject& InObject, int64 MeshID);

	TSharedPtr<IDatasmithBaseMaterialElement> CreateMaterial(const IFC::FMaterial& InMaterial);

	TSharedPtr<IDatasmithActorElement> ConvertNode(const IFC::FObject& InObject);

	TSharedPtr<IDatasmithMeshActorElement> ConvertMeshNode(const IFC::FObject& InObject);

	void SetActorElementTransform(TSharedPtr<IDatasmithActorElement> ActorElement, const FTransform &Transform);

	void ApplyMaterials();

private:
	/** Output Datasmith scene */
	TSharedRef<IDatasmithScene> DatasmithScene;

	mutable TArray<IFC::FLogMessage>		LogMessages;
	TUniquePtr<IFC::FFileReader>			IFCReader;
	TUniquePtr<IFC::FStaticMeshFactory>     IFCStaticMeshFactory;

	const UDatasmithIFCImportOptions*		ImportOptions;

	TSet<FString>							ImportedActorNames;
	TMap<int64, FString>					ImportedMeshes;

	TMap<int64, TSharedPtr<IDatasmithBaseMaterialElement>> ImportedMaterials;

	TMap<IDatasmithMeshElement*, int64> MeshElementToIFCMeshIndex;
	TMap<int64, TSharedRef<IDatasmithMeshElement>> IFCMeshIndexToMeshElement;

	// Keep track of everything we import, because at the end we will import
	// all unreferenced objects separately
	TSet<int64> ImportedIFCInstances;
};

