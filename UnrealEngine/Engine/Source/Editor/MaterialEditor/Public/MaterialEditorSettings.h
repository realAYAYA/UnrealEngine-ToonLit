// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "MaterialEditorSettings.generated.h"

/**
 * Enumerates background for the material editor UI viewport (similar to ETextureEditorBackgrounds)
 */
UENUM()
enum class EBackgroundType : uint8
{
	SolidColor UMETA(DisplayName = "Solid Color"),
	Checkered UMETA(DisplayName = "Checkered")
};

USTRUCT()
struct MATERIALEDITOR_API FCheckerboardSettings
{
	GENERATED_BODY()
	
	// The first color of the checkerboard.
	UPROPERTY(EditAnywhere, Category = "Checkerboard")
	FColor ColorOne;

	// The second color of the checkerboard
	UPROPERTY(EditAnywhere, Category = "Checkerboard")
	FColor ColorTwo;

	// The size of the checkered tiles
	UPROPERTY(EditAnywhere, meta = (ClampMin = "2", ClampMax = "4096"), Category = "Checkerboard")
	int32 Size;

	FCheckerboardSettings()
		: ColorOne(FColor(128, 128, 128))
		, ColorTwo(FColor(64, 64, 64))
		, Size(32)
	{

	}
};

inline bool operator==(const FCheckerboardSettings& Lhs, const FCheckerboardSettings& Rhs)
{
	return Lhs.ColorOne == Rhs.ColorOne
		&& Lhs.ColorTwo == Rhs.ColorTwo
		&& Lhs.Size == Rhs.Size;
}

inline bool operator!=(const FCheckerboardSettings& Lhs, const FCheckerboardSettings& Rhs)
{
	return !operator==(Lhs, Rhs);
}

USTRUCT()
struct MATERIALEDITOR_API FPreviewBackgroundSettings
{
	GENERATED_BODY()
	
	// If true, displays a border around the texture (configured via the material editor)
	UPROPERTY()
	bool bShowBorder;

	// Color to use for the border, if enabled
	UPROPERTY(EditAnywhere, Category = "Background")
	FColor BorderColor;

	// The type of background to show (configured via the material editor)
	UPROPERTY()
	EBackgroundType BackgroundType;

	// The color used as the background of the preview
	UPROPERTY(EditAnywhere, Category = "Background")
	FColor BackgroundColor;

	UPROPERTY(EditAnywhere, Category = "Background")
	FCheckerboardSettings Checkerboard;

	// Note: For now these defaults match the historical material editor behavior, not the texture editor's defaults
	FPreviewBackgroundSettings()
		: bShowBorder(false)
		, BorderColor(FColor::White)
		, BackgroundType(EBackgroundType::SolidColor)
		, BackgroundColor(FColor::Black)
	{

	}

};

UCLASS(config = EditorPerProjectUserSettings)
class MATERIALEDITOR_API UMaterialEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	/**
	Path to user installed Mali shader compiler that can be used by the material editor to compile and extract shader informations for Android platforms.
	Official website address: https://developer.arm.com/products/software-development-tools/graphics-development-tools/mali-offline-compiler/downloads
	*/
	UPROPERTY(config, EditAnywhere, Category = "Offline Shader Compilers", meta = (DisplayName = "Mali Offline Compiler"))
	FFilePath MaliOfflineCompilerPath;

protected:
	// The width (in pixels) of the preview viewport when a material editor is first opened
	UPROPERTY(config, EditAnywhere, meta=(ClampMin=1, ClampMax=4096), Category="User Interface Domain")
	int32 DefaultPreviewWidth = 250;

	// The height (in pixels) of the preview viewport when a material editor is first opened
	UPROPERTY(config, EditAnywhere, meta=(ClampMin=1, ClampMax=4096), Category="User Interface Domain")
	int32 DefaultPreviewHeight = 250;

public:
	// Configures the background shown behind the UI material preview
	UPROPERTY(config, EditAnywhere, Category = "User Interface Domain")
	FPreviewBackgroundSettings PreviewBackground;

	FIntPoint GetPreviewViewportStartingSize() const
	{
		return FIntPoint(DefaultPreviewWidth, DefaultPreviewHeight);
	}

#if WITH_EDITOR
	// Allow listening for changes just to this settings object without having to listen to all UObjects
	FSimpleMulticastDelegate OnPostEditChange;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		OnPostEditChange.Broadcast();
	}
#endif // WITH_EDITOR
};
