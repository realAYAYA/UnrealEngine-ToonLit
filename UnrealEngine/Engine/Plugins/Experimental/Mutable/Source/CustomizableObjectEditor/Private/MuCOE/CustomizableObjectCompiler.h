// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Logging/TokenizedMessage.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CompilationMessageCache.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuT/Node.h"
#include "MuT/NodeObject.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"

class FCustomizableObjectCompileRunnable;
class FCustomizableObjectSaveDDRunnable;
class FReferenceCollector;
class FRunnableThread;
class FText;
class UCustomizableObjectNode;


class FCustomizableObjectCompiler : public FCustomizableObjectCompilerBase, public FGCObject
{
public:

	CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectCompiler();
	CUSTOMIZABLEOBJECTEDITOR_API virtual ~FCustomizableObjectCompiler() {}

	// FCustomizableObjectCompilerBase interface
	CUSTOMIZABLEOBJECTEDITOR_API bool IsRootObject(const class UCustomizableObject* Object) const override;
	
	CUSTOMIZABLEOBJECTEDITOR_API ECustomizableObjectCompilationState GetCompilationState() const override { return State;  };
	
	/** Check for pending compilation process. Returns true if an object has been updated. */
	CUSTOMIZABLEOBJECTEDITOR_API virtual bool Tick() override;

	/** In case Mutable Compile is disabled, fake the compilation process */
	CUSTOMIZABLEOBJECTEDITOR_API void MutableIsDisabledCase(UCustomizableObject* Object);

	/** Provided a CO object it provides the root CO it is connected. In other words : it returns the root of the entire
	 * mutable graph.
	 * @param InObject Customizable object whose root CO we are asking for.
	 * @return The CO that is the root of the provided Customizable Object. It can be equal to InObject if the provided
	 * object does not have any parent.
	 */
	virtual UCustomizableObject* GetRootObject(UCustomizableObject* InObject) override;

	/** Generate the Mutable Graph from the Unreal Graph. */
	mu::NodePtr Export(UCustomizableObject* Object, const FCompilationOptions& Options);

	void CompilerLog(const FText& Message, const TArray<const UCustomizableObjectNode*>& ArrayNode, EMessageSeverity::Type MessageSeverity = EMessageSeverity::Warning, bool bAddBaseObjectInfo = true);
	void CompilerLog(const FText& Message, const UCustomizableObjectNode* Node = nullptr, EMessageSeverity::Type MessageSeverity = EMessageSeverity::Warning, bool bAddBaseObjectInfo = true);
	void NotifyCompilationErrors() const;

	void FinishCompilation();
	void FinishSavingDerivedData();
	virtual void ForceFinishCompilation() override;

	void AddCompileNotification(const FText& CompilationStep) const;
	static void RemoveCompileNotification();

	/** Load required assets and compile.
	 *
	 * Loads assets which reference Object's package asynchronously before calling ProcessChildObjectsRecursively. */
	virtual void Compile(UCustomizableObject& Object, const FCompilationOptions& Options, bool bAsync) override;
	
	/** FSerializableObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectCompiler");
	}

	/** Simply add CO elements from ArrayAssetData to ArrayGCProtect when they've been loaded from ArrayAssetData */
	void UpdateArrayGCProtect();

	/** Getter of CurrentGAsyncLoadingTimeLimit */
	float GetCurrentGAsyncLoadingTimeLimit() { return CurrentGAsyncLoadingTimeLimit; }

	/** Setter for CurrentGAsyncLoadingTimeLimit */
	void SetCurrentGAsyncLoadingTimeLimit(float Value) { CurrentGAsyncLoadingTimeLimit = Value; }

	/** Getter of PreloadingReferencerAssets */
	bool GetPreloadingReferencerAssets() { return PreloadingReferencerAssets; }

	/** Setter for PreloadingReferencerAssets */
	void SetPreloadingReferencerAssets(bool Value) { PreloadingReferencerAssets = Value; }

	// This is used to restrict Group nodes to compile only the SelectedOption and discard all the others
	void AddCompileOnlySelectedOption(const FString& ParamName, const FString& OptionValue);
	// This clears all restrictions and let's a full compilation happen
	void ClearAllCompileOnlySelectedOption();

	// Function to clear the compiler if the editor was closed before starting the compilation 
	void ForceFinishBeforeStartCompilation(UCustomizableObject* Object);

	// Getter of AsynchronousStreamableHandlePtr
	TSharedPtr<struct FStreamableHandle> GetAsynchronousStreamableHandlePtr() { return AsynchronousStreamableHandlePtr; }

private:

	// Object containing all error and warning logs raised during compilation.
	FCompilationMessageCache CompilationLogsContainer;
	
private:
	ECustomizableObjectCompilationState State = ECustomizableObjectCompilationState::None;
	
	void CompileInternal(UCustomizableObject* Object, const FCompilationOptions& Options, bool bAsync = false);

	static void PreloadingReferencerAssetsCallback(UCustomizableObject* Object, FCustomizableObjectCompiler* CustomizableObjectCompiler, const FCompilationOptions Options, bool bAsync);
	
	void ProcessChildObjectsRecursively(UCustomizableObject* Object, class FAssetRegistryModule& AssetRegistryModule, struct FMutableGraphGenerationContext &GenerationContext);

	//
	FCompilationOptions Options;

	//
	TSharedPtr< FCustomizableObjectCompileRunnable > CompileTask;

	//
	TSharedPtr< FRunnableThread > CompileThread;

	//
	TSharedPtr< FCustomizableObjectSaveDDRunnable > SaveDDTask;

	//
	TSharedPtr< FRunnableThread > SaveDDThread;

	// Cache configuration settings from ini files 
	bool bAreExtraBoneInfluencesEnabled = false;

	// TODO: add references
	UCustomizableObject* CurrentObject = nullptr;

	/** Array where to put the names of the already processed child in ProcessChildObjectsRecursively */
	TArray<FName> ArrayAlreadyProcessedChild;


	// Will output to Mutable Log the warning and error messages generated during the CO compilation
	// and update the values of NumWarnings and NumErrors
	void UpdateCompilerLogData();


	/** If duplicated elements are found in each entry of ParameterNamesMap, a warning for
	the parameters with repeated name will be generated */
	void DisplayParameterWarning(struct FMutableGraphGenerationContext& GenerationContext);
	
	/** If duplicated node ids are found, usually due to duplicating CustomizableObjects Assets, a warning
	for the nodes with repeated ids will be generated */
	void DisplayDuplicatedNodeIdsWarning(struct FMutableGraphGenerationContext& GenerationContext);
	
	/** Display warnings for unnamed node objects */
	void DisplayUnnamedNodeObjectWarning(struct FMutableGraphGenerationContext& GenerationContext);
	
	/** Display warnings from discarded PhysicsAssets due to SkeletalBodySetups with no corresponding bones
	in the SkeletalMesh's RefSkeleton */
	//void DisplayDiscardedPhysicsAssetSingleWarning(struct FMutableGraphGenerationContext& GenerationContext);
	
	/** Display a warning for each node contains an orphan pin. */
	void DisplayOrphanNodesWarning(struct FMutableGraphGenerationContext& GenerationContext);
	
	mu::NodeObjectPtr GenerateMutableRoot(UCustomizableObject* Object, FMutableGraphGenerationContext& GenerationContext, FText& ErrorMessage, bool& bOutIsRootObject);

	/** Add to ArrayAssetData the FAssetData information of all referencers of static class type UCustomizableObject::StaticClass()
	* that reference the package given by the PathName parameter
	* @param PathName            [in]  path to the CO to be analyzed (for instance, CO->GetOuter()->GetPathName())
	* @param ArrayReferenceNames [out] array with the package names which are referenced by the CO with PathName given as parameter
	* @return nothing */
	void AddCachedReferencers(const FName& PathName, TArray<FName>& ArrayReferenceNames);

	/** Launches the compile task in another thread when compiling a CO in the editor
	* param ShowNotification [in] whether to show the compiling CO notification or not
	* @return nothing */
	void LaunchMutableCompile(bool ShowNotification);

	/** Launches the save derived data task in another thread after compiling a CO in the
	* editor
	* param ShowNotification [in] whether to show the saving DD notification or not
	* @return nothing */
	void SaveCODerivedData(bool ShowNotification);

	/** When compiling a CO in the editor, flag to know when the Unreal textures have been converted to Mutable textures */
	bool PendingTexturesToLoad;

	/** When compiling a CO in the editor, flag to know if there's a mutable task pending to be launched through LaunchMutableCompile */
	bool CompilationLaunchPending;

	/** Just used to clean ArrayAssetData
	* @return nothing */
	void CleanCachedReferencers();

	/** Will test if package path given by PackageName parameter is one of ArrayAssetData's elements FAssetData::PackageName value
	* @param PackageName [in] package name to test
	* @return true if cached, false otherwise */
	bool IsCachedInAssetData(const FString& PackageName);

	/** Find FAssetData ArrayAssetData with PackageName given by parameter
	* @param PackageName [in] package name to find in ArrayAssetData
	* @return pointer to element if any found, nullptr otherwise */
	FAssetData* GetCachedAssetData(const FString& PackageName);

	/** When compiling a CO, performs the texture conversion from Unreal to Mutable. If in editor, this method is called in
	* FCustomizableObjectCompiler::Tick with time limit, otherwise it is called in FCustomizableObjectCompiler::Compile with
	* no time limit to convert all textures in a row
	* @param UseTimeLimit [in] if true, the method will return after TimeLimit seconds have been used to convert textures, if false, all textures will be processed in a row
	* @return nothing */
	void UpdatePendingTextureConversion(bool UseTimeLimit);

	/** Helper function to compute the value for Unreal Engine variable s.AsyncLoadingTimeLimit while asynchronous loading is used.
	* Also assigned to MaxConvertToMutableTextureTime
	* @return value in milliseconds to use for AsyncLoadingTimeLimit and MaxConvertToMutableTextureTime */
	float ComputeAsyncLoadingTimeLimit();
	
public:
	
	virtual void GetCompilationMessages(TArray<FText>& OutWarningMessages, TArray<FText>& OutErrorMessages) const override;
	
private:
	/** Array with all the packages used to compile current Customizable Object */
	TArray<FAssetData> ArrayAssetData;

	/** Array used to protect from garbage collection those COs loaded asynchronously */
	TArray<UCustomizableObject*> ArrayGCProtect;

	/** Flag to know when asynchronous asset loading is being performed */
	bool PreloadingReferencerAssets;

	/** Copy of GAsyncLoadingTimeLimit while assets are loaded, previous value is restored after asset load */
	float CurrentGAsyncLoadingTimeLimit;

	/** Array with the textures that need to be asynchronously converted from Unreal to Mutable */
	TArray<FTextureUnrealToMutableTask> ArrayTextureUnrealToMutableTask;

	/** Counter to know how many of the textures in ArrayTextureUnrealToMutableTask have been converted from Unreal to Mutable */
	int32 CompletedUnrealToMutableTask;

	/** Time threshold used in UpdatePendingTextureConversion to stop converting textures from Unreal to Mutable until next tick */
	float MaxConvertToMutableTextureTime;

	// Stores the only option of an Int Param that should be compiled
	TMap<FString, FString> ParamNamesToSelectedOptions;

	// List of nodes used during generation, so that they can be found when processing mutable runtime error messages.
	TMap<const void*, const UCustomizableObjectNode*> GeneratedNodes;

	/** Pointer to the Asynchronous Preloading process call back */
	TSharedPtr<struct FStreamableHandle> AsynchronousStreamableHandlePtr;
	
};

