// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneValidator.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "DatasmithTypes.h"
#include "IDatasmithSceneElements.h"

namespace Validator {

class FNamePtr
{
  public:
	FNamePtr() {}

	FNamePtr(const TCHAR* InName)
		: Name(InName)
	{
	}

	bool operator<(const FNamePtr& InOther) const { return FCString::Strcmp(Name, InOther.Name) < 0; }

	bool operator==(FNamePtr InOther) const { return FCString::Strcmp(Name, InOther.Name) == 0; }

	const TCHAR* Name = nullptr;
};

inline uint32 GetTypeHash(const FNamePtr& A)
{
	return FCrc::Strihash_DEPRECATED(FCString::Strlen(A.Name), A.Name);
}

class FSceneValidator : public ISceneValidator
{
  public:
	static const TCHAR* LevelName(TInfoLevel Level);

	FSceneValidator(const TSharedRef< IDatasmithScene >& InScene);

	TSharedRef< IDatasmithScene > Scene;

	FString GetElementTypes(const IDatasmithElement& InElement);

	FString GetElementsDescription(const IDatasmithElement& InElement);

    virtual void CheckTexturesFiles() override;

    virtual void CheckMeshFiles() override;

    virtual void CheckElementsName() override;

    virtual void CheckActorsName(const IDatasmithActorElement& InActor) override;

    virtual void CheckDependances() override;

    virtual void CheckActorsDependances(const IDatasmithActorElement& InActor) override;

    virtual FString GetReports(TInfoLevel Level) override;

	typedef TMap< FNamePtr, const IDatasmithElement* > MapNameToElement;

	MapNameToElement NameToElementMap;

	class FUsage
	{
	  public:
		bool bExist = false;
		bool bIsRefered = false;
	};

	typedef TMap< FNamePtr, FUsage > FMapNameToUsage;

	FMapNameToUsage TexturesUsages;
	FMapNameToUsage MaterialsUsages;
	FMapNameToUsage MeshesUsages;
	FMapNameToUsage ActorsUsages;
	FMapNameToUsage LevelSequencesUsages;

	void AddElements(const IDatasmithElement& InElement, FMapNameToUsage* IOMap = nullptr);

	template < typename FmtType, typename... Types >
	void AddMessage(TInfoLevel Level, const FmtType& Fmt, Types... Args)
	{
		AddMessageImpl(Level, FString::Printf(Fmt, Args...));
	}

	void AddMessageImpl(TInfoLevel Level, const FString& Message);

	class FMessage
	{
	  public:
		FMessage(TInfoLevel InLevel, const FString& InMessage)
			: Level(InLevel)
			, Message(InMessage)
		{
		}

		TInfoLevel Level;
		FString	   Message;
	};

	TArray< FMessage > Messages;

	uint32_t MessagesCounts[kInfoLevelMax] = {};
};

} // namespace Validator
