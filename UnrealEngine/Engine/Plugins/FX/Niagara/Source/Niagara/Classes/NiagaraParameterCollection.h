// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraParameterStore.h"
#include "NiagaraCommon.h"
#include "NiagaraCompileHash.h"
#include "NiagaraParameterCollection.generated.h"

class UMaterialParameterCollection;
class UNiagaraParameterCollection;

UCLASS()
class NIAGARA_API UNiagaraParameterCollectionInstance : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UNiagaraParameterCollectionInstance();

	//~UObject interface
	virtual void PostLoad()override;
	//~UObject interface
	
	bool IsDefaultInstance()const;
	
	void SetParent(UNiagaraParameterCollection* InParent);
	UNiagaraParameterCollection* GetParent()const { return Collection; }

	FNiagaraParameterStore& GetParameterStore() { return ParameterStorage; }

	bool AddParameter(const FNiagaraVariable& Parameter);
	bool RemoveParameter(const FNiagaraVariable& Parameter);
	void RenameParameter(const FNiagaraVariable& Parameter, FName NewName);
	void Empty();
	void GetParameters(TArray<FNiagaraVariable>& OutParameters);

	void Tick(UWorld* World);

	//TODO: Abstract to some interface to allow a hierarchy like UMaterialInstance?
	UPROPERTY(EditAnywhere, Category=Instance)
	TObjectPtr<UNiagaraParameterCollection> Collection;

	/**
	When editing instances, we must track which parameters are overridden so we can pull in any changes to the default.
	*/
	UPROPERTY()
	TArray<FNiagaraVariable> OverridenParameters;
	bool OverridesParameter(const FNiagaraVariable& Parameter)const;
	void SetOverridesParameter(const FNiagaraVariable& Parameter, bool bOverrides);

	/** Synchronizes this instance with any changes with it's parent collection. */
	void SyncWithCollection();

	void Bind(UWorld* World);

private:
	void RefreshSourceParameters(UWorld* World, const TArray<TPair<FName, float>>& ScalarParameters, const TArray<TPair<FName, FLinearColor>>& VectorParameters);
	bool EnsureNotBoundToMaterialParameterCollection(FName InVariableName, FString CallingFunction) const;

	UPROPERTY()
	FNiagaraParameterStore ParameterStorage;

	FRWLock DirtyParameterLock;
	TArray<TPair<FName, float>> DirtyScalarParameters;
	TArray<TPair<FName, FLinearColor>> DirtyVectorParameters;

	//TODO: These overrides should be settable per platform.
	//UPROPERTY()
	//TMap<FString, FNiagaraParameterStore>

	//~UObject interface
#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)override;
#endif
	//~UObject interface
public:
	//Accessors from Blueprint. For now just exposing common types but ideally we can expose any somehow in future.
	
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Bool Parameter"))
	bool GetBoolParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Float Parameter"))
	float GetFloatParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Int Parameter"))
	int32 GetIntParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Vector2D Parameter"))
	FVector2D GetVector2DParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Vector Parameter"))
	FVector GetVectorParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Vector4 Parameter"))
	FVector4 GetVector4Parameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Quaternion Parameter"))
	FQuat GetQuatParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Color Parameter"))
	FLinearColor GetColorParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Bool Parameter"))
	void SetBoolParameter(const FString& InVariableName, bool InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Float Parameter"))
	void SetFloatParameter(const FString& InVariableName, float InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Int Parameter"))
	void SetIntParameter(const FString& InVariableName, int32 InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Vector2D Parameter"))
	void SetVector2DParameter(const FString& InVariableName, FVector2D InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Vector Parameter"))
	void SetVectorParameter(const FString& InVariableName, FVector InValue); // TODO[mg]: add position setter for LWC

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Vector4 Parameter"))
	void SetVector4Parameter(const FString& InVariableName, const FVector4& InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Color Parameter"))
	void SetColorParameter(const FString& InVariableName, FLinearColor InValue);
	
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Quaternion Parameter"))
	void SetQuatParameter(const FString& InVariableName, const FQuat& InValue);
};

/** Asset containing a collection of global parameters usable by Niagara. */
UCLASS()
class NIAGARA_API UNiagaraParameterCollection : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	//~UObject interface
#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)override;
#endif
	//~UObject interface

	int32 IndexOfParameter(const FNiagaraVariable& Var);

	int32 AddParameter(const FNiagaraVariable& Parameter);
	int32 AddParameter(FName Name, FNiagaraTypeDefinition Type);
	void RemoveParameter(const FNiagaraVariable& Parameter);
	void RenameParameter(FNiagaraVariable& Parameter, FName NewName);

	TArray<FNiagaraVariable>& GetParameters() { return Parameters; }

	const UMaterialParameterCollection* GetSourceCollection() const { return SourceMaterialCollection; }

	//TODO: Optional per platform overrides of the above.
	//TMap<FString, UNiagaraParameterCollectionOverride> PerPlatformOverrides;

	FORCEINLINE UNiagaraParameterCollectionInstance* GetDefaultInstance() { return DefaultInstance; }
	
	/**
	Takes the friendly name presented to the UI and converts to the real parameter name used under the hood.
	Converts from "ParameterName" to "CollectionUniqueName_ParameterName".
	*/
	FString ParameterNameFromFriendlyName(const FString& FriendlyName)const;
	/**
	Takes the real parameter name used under the hood and converts to the friendly name for use in the UI.
	Converts from "CollectionUniqueName_ParameterName" to "ParameterName".
	*/

	FNiagaraVariable CollectionParameterFromFriendlyParameter(const FNiagaraVariable& FriendlyParameter)const;
	FNiagaraVariable FriendlyParameterFromCollectionParameter(const FNiagaraVariable& CollectionParameter)const;

	FString FriendlyNameFromParameterName(FString ParameterName)const;
	FString GetFullNamespace()const;

	/** The compile Id is an indicator to any compiled scripts that reference this collection that contents may have changed and a recompile is recommended to be safe.*/
	FNiagaraCompileHash GetCompileHash() const;

	/** If for any reason this data has changed externally and needs recompilation and it isn't autodetected, use this method. */
	void RefreshCompileId();

	//~UObject interface
	virtual void PostLoad()override;
	//~UObject interface

	FName GetNamespace()const { return Namespace; }

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnChanged);

	FOnChanged OnChangedDelegate;
#endif

protected:
	
#if WITH_EDITORONLY_DATA
	void MakeNamespaceNameUnique();
	void AddDefaultSourceParameters();
#endif

	/** Namespace for this parameter collection. Is enforced to be unique across all parameter collections. */
	UPROPERTY(EditAnywhere, Category = "Parameter Collection", AssetRegistrySearchable)
	FName Namespace;
	
	UPROPERTY()
	TArray<FNiagaraVariable> Parameters;

	/** Optional set of MPC that can drive scalar and vector parameters */
	UPROPERTY(EditAnywhere, Category = "Parameter Collection")
	TObjectPtr<UMaterialParameterCollection> SourceMaterialCollection;
	
	UPROPERTY()
	TObjectPtr<UNiagaraParameterCollectionInstance> DefaultInstance;

	/** Used to track whenever something of note changes in this parameter collection that might invalidate a compilation downstream of a script/emitter/system.*/
	UPROPERTY()
	FGuid CompileId;
};