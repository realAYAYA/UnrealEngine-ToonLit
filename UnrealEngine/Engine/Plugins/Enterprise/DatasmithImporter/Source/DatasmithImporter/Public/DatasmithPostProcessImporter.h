// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"

enum class EDatasmithImportActorPolicy : uint8;
class AActor;
class IDatasmithPostProcessElement;
class IDatasmithPostProcessVolumeElement;
struct FDatasmithImportContext;
struct FDatasmithPostProcessSettingsTemplate;

class FDatasmithPostProcessImporter
{
public:
	static FDatasmithPostProcessSettingsTemplate CopyDatasmithPostProcessToUEPostProcess(const TSharedPtr< IDatasmithPostProcessElement >& Src);
	static AActor* ImportPostProcessVolume( const TSharedRef< IDatasmithPostProcessVolumeElement >& PostProcessVolumeElement, FDatasmithImportContext& ImportContext, EDatasmithImportActorPolicy ImportActorPolicy );
};
