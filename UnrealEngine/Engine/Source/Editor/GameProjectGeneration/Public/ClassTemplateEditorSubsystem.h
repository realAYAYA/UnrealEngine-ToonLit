// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "ClassTemplateEditorSubsystem.generated.h"

// Forward Declarations
class FSubsystemCollectionBase;
class FText;
class UClass;


UCLASS(Abstract)
class GAMEPROJECTGENERATION_API UClassTemplate : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	// Returns the directory containing the text file template for
	// the given base generated class.
	virtual FString GetDirectory() const;

	// Reads the header template text from disk.  If failure to read from
	// disk, returns false and provides text reason.
	bool ReadHeader(FString& OutHeaderFileText, FText& OutFailReason) const;

	// Reads the source template text from disk.  If failure to read from
	// disk, returns false and provides text reason.
	bool ReadSource(FString& OutSourceFileText, FText& OutFailReason) const;

	const UClass* GetGeneratedBaseClass() const;

protected:
	// Sets the generated base class associated with the given template
	void SetGeneratedBaseClass(UClass* InClass);

	// Returns the filename associated with the provided class template
	// without an extension.  Defaults to class name.
	virtual FString GetFilename() const;

	// Returns full header filename including '.h.template' extension
	FString GetHeaderFilename() const;

	// Returns full sourcefilename including '.cpp.template' extension
	FString GetSourceFilename() const;

private:
	// Base UClass of which template class corresponds.
	UPROPERTY(Transient)
	TObjectPtr<UClass> GeneratedBaseClass;
};

UCLASS(Abstract)
class GAMEPROJECTGENERATION_API UPluginClassTemplate : public UClassTemplate
{
	GENERATED_BODY()

public:
	// Returns the directory containing the text file template for
	// the given base generated class.
	virtual FString GetDirectory() const override;

protected:
	UPROPERTY(Transient)
	FString PluginName;
};

UCLASS()
class GAMEPROJECTGENERATION_API UClassTemplateEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

private:
	using FTemplateRegistry = TMap<TWeakObjectPtr<const UClass>, TWeakObjectPtr<const UClassTemplate>>;
	FTemplateRegistry TemplateRegistry;

public:
	UClassTemplateEditorSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Registers all currently loaded template classes with the internal registry.
	void RegisterTemplates();

	// Returns path to the directory containing all engine class templates.
	static FString GetEngineTemplateDirectory();

	// Returns whether or not class has registered template
	bool ContainsClassTemplate(const UClass* InClass) const;

	// Returns class template if one is registered.
	const UClassTemplate* FindClassTemplate(const UClass* InClass) const;

	friend class UClassTemplate;

private:
	// Registers a template class with the subsystem
	void Register(const UClassTemplate* InClassTemplate);

	// Unregisters a template class with the subsystem
	bool Unregister(const UClassTemplate* InClassTemplate);
};
