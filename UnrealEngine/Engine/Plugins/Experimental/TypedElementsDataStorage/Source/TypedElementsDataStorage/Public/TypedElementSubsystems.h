// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSubsystems.generated.h"

class ITypedElementDataStorageInterface;
class ITypedElementDataStorageUiInterface;
class ITypedElementDataStorageCompatibilityInterface;

/**
 * A subsystem to provide alternative access to the Typed Elements Data Storage. This can be used in situations where directly accessing
 * the Data Storage from the Typed Elements Registry is not recommended, such as for MASS.
 */
UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDataStorageSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static constexpr bool bRequiresGameThread = true;
	static constexpr bool bIsHotReloadable = false;

	~UTypedElementDataStorageSubsystem() override;

	ITypedElementDataStorageInterface* Get();
	const ITypedElementDataStorageInterface* Get() const;

protected:
	mutable ITypedElementDataStorageInterface* DataStorage{ nullptr };
};

/**
 * A subsystem to provide alternative access to the Typed Elements Data Storage UI. This can be used in situations where directly 
 * accessing the UI from the Typed Elements Registry is not recommended, such as for MASS.
 */
UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDataStorageUiSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static constexpr bool bRequiresGameThread = true;
	static constexpr bool bIsHotReloadable = false;

	~UTypedElementDataStorageUiSubsystem() override;

	ITypedElementDataStorageUiInterface* Get();
	const ITypedElementDataStorageUiInterface* Get() const;

protected:
	mutable ITypedElementDataStorageUiInterface* DataStorageUi{ nullptr };
};

/**
 * A subsystem to provide alternative access to the Typed Elements Data Storage Compatibility. This can be used in situations where directly 
 * accessing the Compatibility extension from the Typed Elements Registry is not recommended, such as for MASS.
 */
UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDataStorageCompatibilitySubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static constexpr bool bRequiresGameThread = true;
	static constexpr bool bIsHotReloadable = false;

	~UTypedElementDataStorageCompatibilitySubsystem() override;

	ITypedElementDataStorageCompatibilityInterface* Get();
	const ITypedElementDataStorageCompatibilityInterface* Get() const;

protected:
	mutable ITypedElementDataStorageCompatibilityInterface* DataStorageCompatibility{ nullptr };
};