// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"


class IMassGameplayEditor;
class FAssetTypeActions_Base;
struct FGraphPanelNodeFactory;

/**
* The public interface to this module
*/
class MASSGAMEPLAYEDITOR_API FMassGameplayEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	void RegisterSectionMappings();
};
