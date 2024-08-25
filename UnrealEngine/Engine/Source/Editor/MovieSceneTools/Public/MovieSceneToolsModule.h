// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/Guid.h"
#include "Curves/RichCurve.h"
#include "Containers/UnrealString.h"
#include "IMovieSceneTools.h"

struct FAssetData;

class UK2Node;
class UBlueprint;
class UMovieScene;
class UMovieSceneSection;
class UMovieSceneEventSectionBase;
class IMovieSceneToolsTrackImporter;
class ULevelSequence;
class IStructureDetailsView;

class IMovieSceneToolsTakeData
{
public:
	virtual bool GatherTakes(const UMovieSceneSection* Section, TArray<FAssetData>& AssetData, uint32& OutCurrentTakeNumber) = 0;
	virtual bool GetTakeNumber(const UMovieSceneSection* Section, FAssetData AssetData, uint32& OutTakeNumber) = 0;
	virtual bool SetTakeNumber(const UMovieSceneSection*, uint32 InTakeNumber) = 0;
};

//Interface to get notifications when an animation bake happens in case in needs to run custom code
class IMovieSceneToolsAnimationBakeHelper
{
public:
	virtual void StartBaking(UMovieScene* MovieScene) {};
	virtual void PreEvaluation(UMovieScene* MovieScene, FFrameNumber Frame) {};
	virtual void PostEvaluation(UMovieScene* MovieScene, FFrameNumber Frame) {};
	virtual void StopBaking(UMovieScene* MovieScene) {};
};

// Interface to allow external modules to register additional key struct instanced property type customizations
class IMovieSceneToolsKeyStructInstancedPropertyTypeCustomizer
{
public:
	virtual void RegisterKeyStructInstancedPropertyTypeCustomization(TSharedRef<IStructureDetailsView> StructureDetailsView, TWeakObjectPtr<UMovieSceneSection> WeakOwningSection) {};
};

/**
* Implements the MovieSceneTools module.
*/
class MOVIESCENETOOLS_API FMovieSceneToolsModule
	: public IMovieSceneTools
{
public:

	static inline FMovieSceneToolsModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FMovieSceneToolsModule >("MovieSceneTools");
	}

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterAnimationBakeHelper(IMovieSceneToolsAnimationBakeHelper* BakeHelper);
	void UnregisterAnimationBakeHelper(IMovieSceneToolsAnimationBakeHelper* BakeHelper);
	const TArray<IMovieSceneToolsAnimationBakeHelper*>& GetAnimationBakeHelpers() { return BakeHelpers; }
	void RegisterTakeData(IMovieSceneToolsTakeData*);
	void UnregisterTakeData(IMovieSceneToolsTakeData*);

	bool GatherTakes(const UMovieSceneSection* Section, TArray<FAssetData>& AssetData, uint32& OutCurrentTakeNumber);
	bool GetTakeNumber(const UMovieSceneSection* Section, FAssetData AssetData, uint32& OutTakeNumber);
	bool SetTakeNumber(const UMovieSceneSection* Section, uint32 InTakeNumber);

	void RegisterTrackImporter(IMovieSceneToolsTrackImporter*);
	void UnregisterTrackImporter(IMovieSceneToolsTrackImporter*);

	bool ImportAnimatedProperty(const FString& InPropertyName, const FRichCurve& InCurve, FGuid InBinding, UMovieScene* InMovieScene);
	bool ImportStringProperty(const FString& InPropertyName, const FString& InPropertyValue, FGuid InBinding, UMovieScene* InMovieScene);

	void RegisterKeyStructInstancedPropertyTypeCustomizer(IMovieSceneToolsKeyStructInstancedPropertyTypeCustomizer*);
	void UnregisterKeyStructInstancedPropertyTypeCustomizer(IMovieSceneToolsKeyStructInstancedPropertyTypeCustomizer*);

	// Called By SKeyEditInterface to allow external modules to add key struct instanced property type customizations
	void CustomizeKeyStructInstancedPropertyTypes(TSharedRef<IStructureDetailsView> StructureDetailsView, TWeakObjectPtr<UMovieSceneSection> Section);

private:

	void RegisterClipboardConversions();

	static void FixupPayloadParameterNameForSection(UMovieSceneEventSectionBase* Section, UK2Node* InNode, FName OldPinName, FName NewPinName);
	static void FixupPayloadParameterNameForDynamicBinding(UMovieScene* MovieScene, UK2Node* InNode, FName OldPinName, FName NewPinName);
	static bool UpgradeLegacyEventEndpointForSection(UMovieSceneEventSectionBase* Section);
	static void PostDuplicateEventSection(UMovieSceneEventSectionBase* Section);
	static void RemoveForCookEventSection(UMovieSceneEventSectionBase* Section);
	static bool IsTrackClassAllowed(UClass* InClass);
	static void PostDuplicateEvent(ULevelSequence* LevelSequence);

private:

	/** Registered delegate handles */
	FDelegateHandle BoolPropertyTrackCreateEditorHandle;
	FDelegateHandle BytePropertyTrackCreateEditorHandle;
	FDelegateHandle ColorPropertyTrackCreateEditorHandle;
	FDelegateHandle FloatPropertyTrackCreateEditorHandle;
	FDelegateHandle DoublePropertyTrackCreateEditorHandle;
	FDelegateHandle IntegerPropertyTrackCreateEditorHandle;
	FDelegateHandle FloatVectorPropertyTrackCreateEditorHandle;
	FDelegateHandle DoubleVectorPropertyTrackCreateEditorHandle;
	FDelegateHandle TransformPropertyTrackCreateEditorHandle;
	FDelegateHandle EulerTransformPropertyTrackCreateEditorHandle;
	FDelegateHandle VisibilityPropertyTrackCreateEditorHandle;
	FDelegateHandle ActorReferencePropertyTrackCreateEditorHandle;
	FDelegateHandle StringPropertyTrackCreateEditorHandle;
	FDelegateHandle ObjectTrackCreateEditorHandle;

	FDelegateHandle AnimationTrackCreateEditorHandle;
	FDelegateHandle AttachTrackCreateEditorHandle;
	FDelegateHandle AudioTrackCreateEditorHandle;
	FDelegateHandle EventTrackCreateEditorHandle;
	FDelegateHandle ParticleTrackCreateEditorHandle;
	FDelegateHandle ParticleParameterTrackCreateEditorHandle;
	FDelegateHandle PathTrackCreateEditorHandle;
	FDelegateHandle CameraCutTrackCreateEditorHandle;
	FDelegateHandle CinematicShotTrackCreateEditorHandle;
	FDelegateHandle SlomoTrackCreateEditorHandle;
	FDelegateHandle SubTrackCreateEditorHandle;
	FDelegateHandle TransformTrackCreateEditorHandle;
	FDelegateHandle ComponentMaterialTrackCreateEditorHandle;
	FDelegateHandle FadeTrackCreateEditorHandle;
	FDelegateHandle SpawnTrackCreateEditorHandle;
	FDelegateHandle LevelVisibilityTrackCreateEditorHandle;
	FDelegateHandle DataLayerTrackCreateEditorHandle;
	FDelegateHandle CameraShakeTrackCreateEditorHandle;
	FDelegateHandle MPCTrackCreateEditorHandle;
	FDelegateHandle PrimitiveMaterialCreateEditorHandle;
	FDelegateHandle CameraShakeSourceShakeCreateEditorHandle;
	FDelegateHandle CVarTrackCreateEditorHandle;
	FDelegateHandle CustomPrimitiveDataTrackCreateEditorHandle;
	FDelegateHandle BindingLifetimeTrackCreateEditorHandle;

	FDelegateHandle CameraCutTrackModelHandle;
	FDelegateHandle CinematicShotTrackModelHandle;
	FDelegateHandle BindingLifetimeTrackModelHandle;

	FDelegateHandle GenerateEventEntryPointsHandle;
	FDelegateHandle FixupDynamicBindingPayloadParameterNameHandle;
	FDelegateHandle FixupEventSectionPayloadParameterNameHandle;
	FDelegateHandle UpgradeLegacyEventEndpointHandle;

	FDelegateHandle OnObjectsReplacedHandle;

	TArray<IMovieSceneToolsTakeData*> TakeDatas;
	TArray<IMovieSceneToolsTrackImporter*> TrackImporters;

	TArray<IMovieSceneToolsAnimationBakeHelper*> BakeHelpers;
	TArray<IMovieSceneToolsKeyStructInstancedPropertyTypeCustomizer*> KeyStructInstancedPropertyTypeCustomizers;
};
