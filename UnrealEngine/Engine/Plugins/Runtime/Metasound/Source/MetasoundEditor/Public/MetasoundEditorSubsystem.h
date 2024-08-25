// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AssetRegistry/AssetData.h"
#include "EditorSubsystem.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "MetasoundDocumentInterface.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundEditorSubsystem.generated.h"


// Forward Declarations
class UMetaSoundBuilderBase;

/** The subsystem in charge of editor MetaSound functionality */
UCLASS()
class METASOUNDEDITOR_API UMetaSoundEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// Build the given builder to a MetaSound asset
	// @param Author - Sets the author on the given builder's document.
	// @param AssetName - Name of the asset to build.
	// @param PackagePath - Path of package to build asset to.
	// @param TemplateSoundWave - SoundWave settings such as attenuation, modulation, and sound class will be copied from the optional TemplateSoundWave.
	// For preset builders, TemplateSoundWave will override the template values from the referenced asset.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor", meta = (WorldContext = "Parent", ExpandEnumAsExecs = "OutResult"))
	UPARAM(DisplayName = "MetaSound Asset") TScriptInterface<IMetaSoundDocumentInterface> BuildToAsset(
		UPARAM(DisplayName = "Builder") UMetaSoundBuilderBase* InBuilder,
		const FString& Author,
		const FString& AssetName,
		const FString& PackagePath,
		EMetaSoundBuilderResult& OutResult,
		UPARAM(DisplayName = "Template SoundWave") const USoundWave* TemplateSoundWave = nullptr);

	// Sets the visual location to InLocation of a given node InNode of a given builder's document.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound|Builder|Editor", meta = (ExpandEnumAsExecs = "OutResult"))
	void SetNodeLocation(
		UPARAM(DisplayName = "Builder") UMetaSoundBuilderBase * InBuilder,
		UPARAM(DisplayName = "Node Handle") const FMetaSoundNodeHandle& InNode,
		UPARAM(DisplayName = "Location") const FVector2D& InLocation,
		EMetaSoundBuilderResult& OutResult);
	
	// Initialize the UObject asset, with an optional MetaSound to be referenced if the asset is a preset
	void InitAsset(UObject& InNewMetaSound, UObject* InReferencedMetaSound = nullptr);

	// Initialize UMetasoundEditorGraph for a given MetaSound object
	void InitEdGraph(UObject& InMetaSound);

	// Wraps RegisterGraphWithFrontend logic in Frontend with any additional logic required to refresh editor & respective editor object state.
	// @param InMetaSound - MetaSound to register
	// @param bInForceSynchronize - Forces the synchronize flag for all open graphs being registered by this call (all referenced graphs and
	// referencing graphs open in editors)
	void RegisterGraphWithFrontend(UObject& InMetaSound, bool bInForceViewSynchronization = false);

	// Register toolbar extender that will be displayed in the MetaSound Asset Editor.
	void RegisterToolbarExtender(TSharedRef<FExtender> InExtender);

	// Unregisters toolbar extender that is displayed in the MetaSound Asset Editor.
	bool UnregisterToolbarExtender(TSharedRef<FExtender> InExtender);

	// Get the default author for a MetaSound asset
	const FString GetDefaultAuthor();

	// Returns all currently toolbar extenders registered to be displayed within the MetaSound Asset Editor.
	const TArray<TSharedRef<FExtender>>& GetToolbarExtenders() const;

	static UMetaSoundEditorSubsystem& GetChecked();
	static const UMetaSoundEditorSubsystem& GetConstChecked();

private:
	// Copy over sound wave settings such as attenuation, modulation, and sound class from the template sound wave to the MetaSound
	void SetSoundWaveSettingsFromTemplate(USoundWave& NewMetasound, const USoundWave& TemplateSoundWave) const;

	// Editor Toolbar Extenders
	TArray<TSharedRef<FExtender>> EditorToolbarExtenders;
};
