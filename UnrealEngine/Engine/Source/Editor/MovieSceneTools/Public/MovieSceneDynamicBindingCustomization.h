// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneDirectorBlueprintEndpointCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Types/SlateEnums.h"

class UK2Node;
class UBlueprint;
class UEdGraphPin;
class IPropertyHandle;
class FDetailWidgetRow;
class SWidget;

struct FMovieSceneDynamicBinding;

enum class ECheckBoxState : uint8;

/**
 * Customization for dynamic spawn/possession endpoint picker.
 */
class MOVIESCENETOOLS_API FMovieSceneDynamicBindingCustomization : public FMovieSceneDirectorBlueprintEndpointCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(UMovieScene* InMovieScene, FGuid InObjectBinding);

protected:

	virtual void GetPayloadVariables(UObject* EditObject, void* RawData, FPayloadVariableMap& OutPayloadVariables) const override;
	virtual bool SetPayloadVariable(UObject* EditObject, void* RawData, FName FieldName, const FMovieSceneDirectorBlueprintVariableValue& NewVariableValue) override;
	virtual UK2Node* FindEndpoint(UMovieSceneSequence* Sequence, UBlueprint* Blueprint, UObject* EditObject, void* RawData) const override;
	virtual void GetWellKnownParameterPinNames(UObject* EditObject, void* RawData, TArray<FName>& OutWellKnownParameters) const override;
	virtual void GetWellKnownParameterCandidates(UK2Node* Endpoint, TArray<FWellKnownParameterCandidates>& OutCandidates) const override;
	virtual bool SetWellKnownParameterPinName(UObject* EditObject, void* RawData, int32 ParameterIndex, FName BoundPinName) override;
	virtual FMovieSceneDirectorBlueprintEndpointDefinition GenerateEndpointDefinition(UMovieSceneSequence* Sequence) override;
	virtual void OnCreateEndpoint(UMovieSceneSequence* Sequence, UBlueprint* Blueprint, const TArray<UObject*> EditObjects, const TArray<void*> RawData, const FMovieSceneDirectorBlueprintEndpointDefinition& EndpointDefinition, UK2Node* NewEndpoint) override;
	virtual void OnSetEndpoint(UMovieSceneSequence* Sequence, UBlueprint* Blueprint, const TArray<UObject*> EditObjects, const TArray<void*> RawData, const FMovieSceneDirectorBlueprintEndpointDefinition& EndpointDefinition, UK2Node* NewEndpoint) override;
	virtual void GetEditObjects(TArray<UObject*>& OutObjects) const override;
	virtual void OnCollectQuickBindActions(UBlueprint* Blueprint, FBlueprintActionMenuBuilder& MenuBuilder) override;

private:

	void SetEndpointImpl(UMovieScene* MovieScene, FMovieSceneDynamicBinding* DynamicBinding, UBlueprint* Blueprint, UK2Node* NewEndpoint);
	void EnsureBlueprintExtensionCreated(UMovieScene* MovieScene, UBlueprint* Blueprint);

	void CollectResolverLibraryBindActions(UBlueprint* Blueprint, FBlueprintActionMenuBuilder& MenuBuilder, bool bIsRebinding);

private:

	UMovieScene* EditedMovieScene;
	FGuid ObjectBinding;
};

