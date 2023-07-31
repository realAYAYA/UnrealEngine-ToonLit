// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/Optional.h"
#include "Serialization/Archive.h"

#include "DMXMVRGeneralSceneDescription.generated.h"

class FXmlFile;
class UDMXEntityFixturePatch;
class UDMXLibrary;
class UDMXMVRAssetImportData;
class UDMXMVRRootNode;
class UDMXMVRFixtureNode;


/** MVR General Scene Description Object */
UCLASS()
class DMXRUNTIME_API UDMXMVRGeneralSceneDescription
	: public UObject
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDMXMVRGeneralSceneDescription();

	/** Gets the MVR Fixture Nodes in this General Scene Description */
	void GetFixtureNodes(TArray<UDMXMVRFixtureNode*>& OutFixtureNodes) const;

	/** Returns the Fixture Node corresponding to the UUID, or nullptr if it cannot be found */
	UDMXMVRFixtureNode* FindFixtureNode(const FGuid& FixtureUUID) const;

#if WITH_EDITOR
	/** Creates an MVR General Scene Description from an Xml File */
	static UDMXMVRGeneralSceneDescription* CreateFromXmlFile(TSharedRef<FXmlFile> GeneralSceneDescriptionXml, UObject* Outer, FName Name, EObjectFlags Flags = RF_NoFlags);

	/** Creates an MVR General Scene Description from a DMX Library */
	static UDMXMVRGeneralSceneDescription* CreateFromDMXLibrary(const UDMXLibrary& DMXLibrary, UObject* Outer, FName Name, EObjectFlags Flags = RF_NoFlags);

	/**
	 * Writes the Library to the General Scene Description, effectively removing inexisting and adding
	 * new MVR Fixtures, according to what MVR Fixture UUIDs the Fixture Patches of the Library contain.
	 */
	void WriteDMXLibraryToGeneralSceneDescription(const UDMXLibrary& DMXLibrary);

	/** Returns true if an Xml File can be created. Gives a reason if no MVR can be exported */
	bool CanCreateXmlFile(FText& OutReason) const;

	/** Creates an General Scene Description Xml File from this. */
	TSharedPtr<FXmlFile> CreateXmlFile() const;

	/** Returns MVR Asset Import Data for this asset */
	FORCEINLINE UDMXMVRAssetImportData* GetMVRAssetImportData() const { return MVRAssetImportData; }
#endif

private:
#if WITH_EDITOR
	/** Writes the Fixture Patch to the General Scene Description, adding a new MVR Fixture if required */
	void WriteFixturePatchToGeneralSceneDescription(const UDMXEntityFixturePatch& FixturePatch);

	/** Parses a General Scene Description Xml File. Only ment to be used for initialization (ensured) */
	void ParseGeneralSceneDescriptionXml(const TSharedRef<FXmlFile>& GeneralSceneDescriptionXml);
#endif

	/** Returns the Fixture IDs currently in use, as number, sorted from lowest to highest */
	TArray<int32> GetNumericalFixtureIDsInUse(const UDMXLibrary& DMXLibrary);

	/** The Root Node of the General Scene Description */
	UPROPERTY()
	TObjectPtr<UDMXMVRRootNode> RootNode;

#if WITH_EDITORONLY_DATA
	/** Import Data for this asset */
	UPROPERTY()
	TObjectPtr<UDMXMVRAssetImportData> MVRAssetImportData;
#endif
};
