// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameter.h"
#include "AnimNextParameterLibrary.generated.h"

class UAnimNextParameter;
class UAnimNextParameterLibrary;
class UAnimNextParameterBlockBinding;
class UAnimNextParameterBlockBindingReference;
class UAnimNextParameterBlock_EditorData;
class FAnimationAnimNextParametersEditorTest_Library;
class FAnimationAnimNextParametersEditorTest_Block;

namespace UE::AnimNext::Editor
{
	struct FUtils;
	class SParameterBlockView;
	class SParameterLibraryView;
}

namespace UE::AnimNext::UncookedOnly
{
	// A delegate for subscribing / reacting to parameter library modifications.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnParameterLibraryModified, UAnimNextParameterLibrary* /* InLibrary */);
}

// Library entry used to export to asset registry
USTRUCT()
struct FAnimNextParameterLibraryAssetRegistryExportEntry
{
	GENERATED_BODY()

	FAnimNextParameterLibraryAssetRegistryExportEntry() = default;
	
	FAnimNextParameterLibraryAssetRegistryExportEntry(FName InName, const FAnimNextParamType& InType)
		: Name(InName)
		, Type(InType)
	{}
	
	UPROPERTY()
	FName Name;

	UPROPERTY()
	FAnimNextParamType Type;
};

// Library used to export to asset registry
USTRUCT()
struct FAnimNextParameterLibraryAssetRegistryExports
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAnimNextParameterLibraryAssetRegistryExportEntry> Parameters;
};

/** Defines a single parameter */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterLibrary : public UObject
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	ANIMNEXTUNCOOKEDONLY_API static const FName ExportsAssetRegistryTag;

	friend struct UE::AnimNext::Editor::FUtils;
	friend class UE::AnimNext::Editor::SParameterBlockView;
	friend class UE::AnimNext::Editor::SParameterLibraryView;
	friend class UAnimNextParameterBlockBinding;
	friend class UAnimNextParameterBlockBindingReference;
	friend class UAnimNextParameterBlock_EditorData;
	friend class FAnimationAnimNextParametersEditorTest_Library;
	friend class FAnimationAnimNextParametersEditorTest_Block;

	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameter* AddParameter(FName InName, const FAnimNextParamType& InType, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Library")
	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameter* AddParameter(FName InName, const EPropertyBagPropertyType& InValueType, const EPropertyBagContainerType& InContainerType, const UObject* InValueTypeObject = nullptr, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Library")
	ANIMNEXTUNCOOKEDONLY_API bool RemoveParameter(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Library")
	ANIMNEXTUNCOOKEDONLY_API bool RemoveParameters(const TArray<FName>& InNames, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Library")
	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameter* FindParameter(FName InName) const;

	// Returns the modified event which can be used to subscribe to changes to this parameter
	ANIMNEXTUNCOOKEDONLY_API UE::AnimNext::UncookedOnly::FOnParameterLibraryModified& OnModified();

	// UObject interface 
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	void ReportError(const TCHAR* InMessage) const;

	void BroadcastModified();
	
	/** Comment to display in editor */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta=(MultiLine))
	FString Comment;

	/** All parameters contained in this library - not saved, discovered at load time */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimNextParameter>> Parameters;

	// Delegate to subscribe to modifications
	UE::AnimNext::UncookedOnly::FOnParameterLibraryModified ModifiedDelegate;

	bool bSuspendNotifications = false;
#endif // #if WITH_EDITORONLY_DATA
};