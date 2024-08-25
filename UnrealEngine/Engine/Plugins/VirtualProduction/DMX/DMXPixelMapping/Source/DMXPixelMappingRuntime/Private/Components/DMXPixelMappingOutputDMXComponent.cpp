// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingOutputDMXComponent.h"

#include "Library/DMXEntityFixturePatch.h"


#if WITH_EDITOR
FLinearColor UDMXPixelMappingOutputDMXComponent::GetEditorColor() const
{
	if (bUsePatchColor)
	{
		const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();
		return FixturePatch->EditorColor;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Editor color should not be publicly accessed. However it is ok to access it here.
	return EditorColor;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR
