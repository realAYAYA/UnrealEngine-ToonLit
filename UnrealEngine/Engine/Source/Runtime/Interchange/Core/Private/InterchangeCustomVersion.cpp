// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FInterchangeCustomVersion::GUID("92738C43-2988-4D9C-9A3D-9BBE6EFF9FC0");
FCustomVersionRegistration GRegisterInterchangeCustomVersion(FInterchangeCustomVersion::GUID, FInterchangeCustomVersion::LatestVersion, TEXT("InterchangeAssetImportDataVer"));