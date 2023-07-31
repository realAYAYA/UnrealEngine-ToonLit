// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UDMXImportGDTF;
class UDMXGDTFImportUI;
class FXmlFile;
class UPackage;

struct FDMXGDTFImportArgs
{
public:
    FDMXGDTFImportArgs()
        : Parent(nullptr)
        , Name(NAME_None)
        , Flags(RF_NoFlags)
        , ImportUI(nullptr)
        , bCancelOperation(false)
    {}

    TWeakObjectPtr<UObject> Parent;
    FName Name;
    FString CurrentFilename;
    EObjectFlags Flags;
    TWeakObjectPtr<UDMXGDTFImportUI> ImportUI;
    bool bCancelOperation;
};

/**
 * GDTF Importer is read and parse GDTF input file
 */
class FDMXGDTFImporter
{
public:
    FDMXGDTFImporter(const FDMXGDTFImportArgs& InImportArgs);

    /** Try to load the file and parse the data */
    bool AttemptImportFromFile();

    /** Create new GDTF UObject from imported file */
    UDMXImportGDTF* Import();

private:
    bool ParseXML();

    /** Create GDTF from imported xml */
    UDMXImportGDTF* CreateGDTFDesctription();

    UPackage* GetPackage(const FString& InAssetName);
public:

    static void GetImportOptions(const TUniquePtr<FDMXGDTFImporter>& Importer, UDMXGDTFImportUI* ImportUI, bool bShowOptionDialog, const FString& FullPath, bool& OutOperationCanceled, bool& bOutImportAll, const FString& InFilename);
private:
    TSharedPtr<FXmlFile> XMLFile;

    FDMXGDTFImportArgs ImportArgs;

    bool bIsXMLParsedSuccessfully;

    FString LastPackageName;

    FString ImportName;
};