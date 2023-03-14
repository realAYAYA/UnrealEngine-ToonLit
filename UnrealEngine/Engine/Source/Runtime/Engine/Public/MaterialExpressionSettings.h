// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/TopLevelAssetPath.h"

#if WITH_EDITOR

/**
 * Singleton for the material expression settings and permissions
 */
class ENGINE_API FMaterialExpressionSettings
{
public:
	/** Gets singleton instance */
	static FMaterialExpressionSettings* Get();

	/** Delegate to filter class paths from permissions lists */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnIsClassPathAllowed, const FTopLevelAssetPath& /*InClassPath*/);

	void RegisterIsClassPathAllowedDelegate(const FName OwnerName, FOnIsClassPathAllowed Delegate);
	void UnregisterIsClassPathAllowedDelegate(const FName OwnerName);
	bool IsClassPathAllowed(const FTopLevelAssetPath& InClassPath) const;
	bool HasClassPathFiltering() const;

private:
	FMaterialExpressionSettings() = default;
	~FMaterialExpressionSettings() = default;

	/** Delegates called to determine whether a class type is allowed to be processed in the material translator */
	TMap<FName, FOnIsClassPathAllowed> IsClassPathAllowedDelegates;
};

#endif // WITH_EDITOR
