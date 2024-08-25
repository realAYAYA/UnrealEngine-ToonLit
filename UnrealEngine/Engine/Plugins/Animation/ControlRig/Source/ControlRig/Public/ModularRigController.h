// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularRigModel.h"
#include "ModularRigController.generated.h"

struct FRigModuleReference;

DECLARE_MULTICAST_DELEGATE_TwoParams(FModularRigModifiedEvent, EModularRigNotification /* type */, const FRigModuleReference* /* element */);

UCLASS(BlueprintType)
class CONTROLRIG_API UModularRigController : public UObject
{
	GENERATED_UCLASS_BODY()

	UModularRigController()
		: Model(nullptr)
		, bSuspendNotifications(false)
	{
	}

	FModularRigModel* Model;
	FModularRigModifiedEvent ModifiedEvent;
	bool bSuspendNotifications;

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	FString AddModule(const FName& InModuleName, TSubclassOf<UControlRig> InClass, const FString& InParentModulePath, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool CanConnectConnectorToElement(const FRigElementKey& InConnectorKey, const FRigElementKey& InTargetKey, FText& OutErrorMessage);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool ConnectConnectorToElement(const FRigElementKey& InConnectorKey, const FRigElementKey& InTargetKey, bool bSetupUndo = true, bool bAutoResolveOtherConnectors = true, bool bCheckValidConnection = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool DisconnectConnector(const FRigElementKey& InConnectorKey, bool bDisconnectSubModules = false, bool bSetupUndo = true);
	
	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	TArray<FRigElementKey> DisconnectCyclicConnectors(bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool AutoConnectSecondaryConnectors(const TArray<FRigElementKey>& InConnectorKeys, bool bReplaceExistingConnections, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool AutoConnectModules(const TArray<FString>& InModulePaths, bool bReplaceExistingConnections, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool SetConfigValueInModule(const FString& InModulePath, const FName& InVariableName, const FString& InValue, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool BindModuleVariable(const FString& InModulePath, const FName& InVariableName, const FString& InSourcePath, bool bSetupUndo = true);
	bool CanBindModuleVariable(const FString& InModulePath, const FName& InVariableName, const FString& InSourcePath, FText& OutErrorMessage);
	TArray<FString> GetPossibleBindings(const FString& InModulePath, const FName& InVariableName);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool UnBindModuleVariable(const FString& InModulePath, const FName& InVariableName, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool DeleteModule(const FString& InModulePath, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	FString RenameModule(const FString& InModulePath, const FName& InNewName, bool bSetupUndo = true);
	bool CanRenameModule(const FString& InModulePath, const FName& InNewName, FText& OutErrorMessage) const;

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	FString ReparentModule(const FString& InModulePath, const FString& InNewParentModulePath, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	FString MirrorModule(const FString& InModulePath, const FRigVMMirrorSettings& InSettings, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool SetModuleShortName(const FString& InModulePath, const FString& InNewShortName, bool bSetupUndo = true);
	bool CanSetModuleShortName(const FString& InModulePath, const FString& InNewShortName, FText& OutErrorMessage) const;

	static int32 GetMaxNameLength() { return 100; }
	static void SanitizeName(FRigName& InOutName, bool bAllowNameSpaces);
	static FRigName GetSanitizedName(const FRigName& InName, bool bAllowNameSpaces);
	bool IsNameAvailable(const FString& InParentModulePath, const FRigName& InDesiredName, FString* OutErrorMessage = nullptr) const;
	bool IsShortNameAvailable(const FRigName& InDesiredShortName, FString* OutErrorMessage = nullptr) const;
	FRigName GetSafeNewName(const FString& InParentModulePath, const FRigName& InDesiredName) const;
	FRigName GetSafeNewShortName(const FRigName& InDesiredShortName) const;
	FRigModuleReference* FindModule(const FString& InPath);
	const FRigModuleReference* FindModule(const FString& InPath) const;
	FModularRigModifiedEvent& OnModified() { return ModifiedEvent; }

private:

	void SetModel(FModularRigModel* InModel) { Model = InModel; }
	void Notify(const EModularRigNotification& InNotification, const FRigModuleReference* InElement);
	void UpdateShortNames();

	bool bAutomaticReparenting;
	
	friend struct FModularRigModel;
	friend class FModularRigControllerCompileBracketScope;
};

class CONTROLRIG_API FModularRigControllerCompileBracketScope
{
public:
   
	FModularRigControllerCompileBracketScope(UModularRigController *InController);

	~FModularRigControllerCompileBracketScope();

private:
	UModularRigController* Controller;
	bool bSuspendNotifications;
};
