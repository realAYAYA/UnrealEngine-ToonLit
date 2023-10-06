// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"
#include "Templates/SubclassOf.h"

#include "DatasmithAssetUserData.generated.h"

/** Asset user data that can be used with Datasmith on Actors and other objects  */
UCLASS(BlueprintType, meta = (ScriptName = "DatasmithUserData", DisplayName = "Datasmith User Data"))
class DATASMITHCONTENT_API UDatasmithAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:

	// Meta-data are available at runtime in game, i.e. used in blueprint to display build-boarded information
	typedef TMap<FName, FString> FMetaDataContainer;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Datasmith User Data", meta = (ScriptName = "Metadata", DisplayName = "Metadata"))
	TMap<FName, FString> MetaData;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap< TSubclassOf< class UDatasmithObjectTemplate >, TObjectPtr<UDatasmithObjectTemplate> > ObjectTemplates;

	virtual bool IsPostLoadThreadSafe() const override;
	virtual void PostLoad() override;
#endif

	static FString GetDatasmithUserDataValueForKey(UObject* Object, FName Key, bool bPartialMatchKey = false);
	static TArray<FString> GetDatasmithUserDataValuesForKey(UObject* Object, FName Key, bool bPartialMatchKey = false);
	static UDatasmithAssetUserData* GetDatasmithUserData(UObject* Object);
	static bool SetDatasmithUserDataValueForKey(UObject* Object, FName Key, const FString & Value);

	// Meta data keys for Datasmith objects
	static const TCHAR* UniqueIdMetaDataKey;
};
