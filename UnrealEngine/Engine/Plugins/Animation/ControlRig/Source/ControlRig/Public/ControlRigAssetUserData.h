// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMAssetUserData.h"
#include "ControlRigGizmoLibrary.h"
#include "ControlRigAssetUserData.generated.h"

/**
* Namespaced user data which provides access to a linked shape library
*/
UCLASS(BlueprintType)
class CONTROLRIG_API UControlRigShapeLibraryLink : public UNameSpacedUserData
{
	GENERATED_BODY()

public:

	/** If assigned, the data asset link will provide access to the data asset's content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, BlueprintGetter = GetShapeLibrary, BlueprintSetter = SetShapeLibrary, Meta = (DisplayAfter="NameSpace"))
	TObjectPtr<UControlRigShapeLibrary> ShapeLibrary;

	UFUNCTION(BlueprintGetter)
	UControlRigShapeLibrary* GetShapeLibrary() const { return ShapeLibrary; }

	UFUNCTION(BlueprintSetter)
	void SetShapeLibrary(UControlRigShapeLibrary* InShapeLibrary);

	virtual const FUserData* GetUserData(const FString& InPath, FString* OutErrorMessage = nullptr) const override;
	virtual const TArray<const FUserData*>& GetUserDataArray(const FString& InParentPath = FString(), FString* OutErrorMessage = nullptr) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	UPROPERTY(transient)
	TArray<FName> ShapeNames;

	static inline constexpr TCHAR DefaultShapePath[] = TEXT("DefaultShape");
	static inline constexpr TCHAR ShapeNamesPath[] = TEXT("ShapeNames");
	static inline constexpr TCHAR ShapeLibraryNullFormat[] = TEXT("User data path '%s' could not be found (ShapeLibrary not provided)");
};