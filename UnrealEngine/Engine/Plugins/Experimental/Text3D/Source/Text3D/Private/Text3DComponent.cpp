// Copyright Epic Games, Inc. All Rights Reserved.

#include "Text3DComponent.h"

#include "Glyph.h"
#include "MeshCreator.h"
#include "Text3DEngineSubsystem.h"
#include "Text3DPrivate.h"
#include "TextShaper.h"
#include "Algo/Count.h"
#include "Async/Async.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Misc/ScopeExit.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "Text3D"

struct FText3DShapedText
{
	FText3DShapedText()
	{
		Reset();
	}

	void Reset()
	{
		LineHeight = 0.0f;
		FontAscender = 0.0f;
		FontDescender = 0.0f;
		Lines.Reset();
	}

	float LineHeight;
	float FontAscender;
	float FontDescender;
	TArray<struct FShapedGlyphLine> Lines;
};

using TTextMeshDynamicData = TArray<TUniquePtr<FText3DDynamicData>, TFixedAllocator<static_cast<int32>(EText3DGroupType::TypeCount)>>;

UText3DComponent::UText3DComponent()
	: bIsBuilding(false)
	, ShapedText(new FText3DShapedText())
{
	TextRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TextRoot"));
#if WITH_EDITOR
	TextRoot->SetIsVisualizationComponent(true);
#endif

	if (!IsRunningDedicatedServer())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinder<UFont> Font;
			ConstructorHelpers::FObjectFinder<UMaterial> Material;
			FConstructorStatics()
				: Font(TEXT("/Engine/EngineFonts/Roboto"))
				, Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		Font = ConstructorStatics.Font.Object;
		UMaterial* DefaultMaterial = ConstructorStatics.Material.Object;
		FrontMaterial = DefaultMaterial;
		BevelMaterial = DefaultMaterial;
		ExtrudeMaterial = DefaultMaterial;
		BackMaterial = DefaultMaterial;
	}

	Text = LOCTEXT("DefaultText", "Text");
	bOutline = false;
	OutlineExpand = 0.5f;
	Extrude = 5.0f;
	Bevel = 0.0f;
	BevelType = EText3DBevelType::Convex;
	BevelSegments = 8;

	HorizontalAlignment = EText3DHorizontalTextAlignment::Left;
	VerticalAlignment = EText3DVerticalTextAlignment::FirstLine;
	Kerning = 0.0f;
	LineSpacing = 0.0f;
	WordSpacing = 0.0f;

	bHasMaxWidth = false;
	MaxWidth = 500.f;
	bHasMaxHeight = false;
	MaxHeight = 500.0f;
	bScaleProportionally = true;

	bPendingBuild = false;
	bFreezeBuild = false;

	TextScale = FVector::ZeroVector;
}

void UText3DComponent::OnRegister()
{
	Super::OnRegister();
	TextRoot->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

	// Forces rebuild (if build is in progress)
	bIsBuilding = false;
	BuildTextMesh();
}

void UText3DComponent::OnUnregister()
{
	ClearTextMesh();
	Super::OnUnregister();
}

#if WITH_EDITOR
void UText3DComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	static FName BevelTypePropertyName = GET_MEMBER_NAME_CHECKED(UText3DComponent, BevelType);
	static FName BevelSegmentsPropertyName = GET_MEMBER_NAME_CHECKED(UText3DComponent, BevelSegments);

	const FName Name = PropertyChangedEvent.GetPropertyName();
	if (Name == BevelTypePropertyName)
	{
		switch (BevelType)
		{
		case EText3DBevelType::Linear:
		case EText3DBevelType::OneStep:
		case EText3DBevelType::TwoSteps:
		case EText3DBevelType::Engraved:
		{
			BevelSegments = 1;
			break;
		}
		case EText3DBevelType::Convex:
		case EText3DBevelType::Concave:
		{
			BevelSegments = 8;
			break;
		}
		case EText3DBevelType::HalfCircle:
		{
			BevelSegments = 16;
			break;
		}
		}
	}
	else if (Name == BevelSegmentsPropertyName)
	{
		// Force minimum bevel segments based on the BevelType
		SetBevelSegments(BevelSegments);
	}
}
#endif

void UText3DComponent::SetText(const FText& Value)
{
	if (!Text.EqualTo(Value))
	{
		Text = Value;
		Rebuild();
	}
}

void UText3DComponent::SetFont(UFont* const InFont)
{
	if (Font != InFont)
	{
		Font = InFont;
		Rebuild();
	}
}

void UText3DComponent::SetOutline(const bool bValue)
{
	if (bOutline != bValue)
	{
		bOutline = bValue;		
		Rebuild();
	}
}

void UText3DComponent::SetOutlineExpand(const float Value)
{
	const float NewValue = Value;
	if (!FMath::IsNearlyEqual(OutlineExpand, NewValue))
	{
		OutlineExpand = NewValue;
		Rebuild();
	}
}

void UText3DComponent::SetExtrude(const float Value)
{
	const float NewValue = FMath::Max(0.0f, Value);
	if (!FMath::IsNearlyEqual(Extrude, NewValue))
	{
		Extrude = NewValue;
		CheckBevel();
		Rebuild();
	}
}

void UText3DComponent::SetBevel(const float Value)
{
	const float NewValue = FMath::Clamp(Value, 0.f, MaxBevel());

	if (!FMath::IsNearlyEqual(Bevel, NewValue))
	{
		Bevel = NewValue;
		Rebuild();
	}
}

void UText3DComponent::SetBevelType(const EText3DBevelType Value)
{
	if (BevelType != Value)
	{
		BevelType = Value;
		Rebuild();
	}
}

void UText3DComponent::SetBevelSegments(const int32 Value)
{
	int32 MinBevelSegments = 1;
	if (BevelType == EText3DBevelType::HalfCircle)
	{
		MinBevelSegments = 2;
	}

	const int32 NewValue = FMath::Clamp(Value, MinBevelSegments, 15);
	if (BevelSegments != NewValue)
	{
		BevelSegments = NewValue;
		Rebuild();
	}
}

void UText3DComponent::SetFrontMaterial(UMaterialInterface* Value)
{
	SetMaterial(EText3DGroupType::Front, Value);
}

void UText3DComponent::SetBevelMaterial(UMaterialInterface* Value)
{
	SetMaterial(EText3DGroupType::Bevel, Value);
}

void UText3DComponent::SetExtrudeMaterial(UMaterialInterface* Value)
{
	SetMaterial(EText3DGroupType::Extrude, Value);
}

void UText3DComponent::SetBackMaterial(UMaterialInterface* Value)
{
	SetMaterial(EText3DGroupType::Back, Value);
}

bool UText3DComponent::AllocateGlyphs(int32 Num)
{
	int32 DeltaNum = Num - CharacterMeshes.Num();
	if (DeltaNum == 0)
	{
		return false;
	}
	// Add characters
	if (FMath::Sign(DeltaNum) > 0)
	{
		int32 GlyphId = CharacterMeshes.Num() - 1;
		for(int32 CharacterIndex = 0; CharacterIndex < DeltaNum; ++CharacterIndex)
		{
			GlyphId++;

			const FName CharacterKerningComponentName = MakeUniqueObjectName(this, USceneComponent::StaticClass(), FName(*FString::Printf(TEXT("CharacterKerning%d"), GlyphId)));
			USceneComponent* CharacterKerningComponent = NewObject<USceneComponent>(this, CharacterKerningComponentName, RF_Transient);
			
#if WITH_EDITOR
			CharacterKerningComponent->SetIsVisualizationComponent(true);
#endif
			CharacterKerningComponent->AttachToComponent(TextRoot, FAttachmentTransformRules::KeepRelativeTransform);
			CharacterKerningComponent->RegisterComponent();
			CharacterKernings.Add(CharacterKerningComponent);

			const FName StaticMeshComponentName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("StaticMeshComponent%d"), GlyphId)));
			UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this, StaticMeshComponentName, RF_Transient);
			StaticMeshComponent->RegisterComponent();
			StaticMeshComponent->SetVisibility(GetVisibleFlag());
			StaticMeshComponent->SetHiddenInGame(bHiddenInGame);
			StaticMeshComponent->SetCastShadow(bCastShadow);
			CharacterMeshes.Add(StaticMeshComponent);

			GetOwner()->AddInstanceComponent(StaticMeshComponent);
			StaticMeshComponent->AttachToComponent(CharacterKerningComponent, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
	// Remove characters
	else
	{
		AActor* OwnerActor = GetOwner();
		DeltaNum = FMath::Abs(DeltaNum);
		for(int32 CharacterIndex = CharacterKernings.Num() - 1 - DeltaNum; CharacterIndex < CharacterKernings.Num(); ++CharacterIndex)
		{
			USceneComponent* CharacterKerningComponent = CharacterKernings[CharacterIndex];
			// If called in quick succession, may already be pending destruction
			if (IsValid(CharacterKerningComponent))
			{
				CharacterKerningComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				CharacterKerningComponent->UnregisterComponent();
				CharacterKerningComponent->DestroyComponent();
				OwnerActor->RemoveInstanceComponent(CharacterKerningComponent);
			}

			
			UStaticMeshComponent* StaticMeshComponent = CharacterMeshes[CharacterIndex];
			if (IsValid(StaticMeshComponent))
			{
				StaticMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				StaticMeshComponent->UnregisterComponent();
				StaticMeshComponent->DestroyComponent();
				OwnerActor->RemoveInstanceComponent(StaticMeshComponent);
			}
		}
		
		CharacterKernings.RemoveAt(CharacterKernings.Num() - 1 - DeltaNum, DeltaNum);
		CharacterMeshes.RemoveAt(CharacterMeshes.Num() - 1 - DeltaNum, DeltaNum);
	}

	return true;
}

UMaterialInterface* UText3DComponent::GetMaterial(const EText3DGroupType Type) const
{
	switch (Type)
	{
	case EText3DGroupType::Front:
	{
		return FrontMaterial;
	}

	case EText3DGroupType::Bevel:
	{
		return BevelMaterial;
	}

	case EText3DGroupType::Extrude:
	{
		return ExtrudeMaterial;
	}

	case EText3DGroupType::Back:
	{
		return BackMaterial;
	}
		
	default:
	{
		return nullptr;
	}
	}
}

void UText3DComponent::SetMaterial(const EText3DGroupType Type, UMaterialInterface* Value)
{
	UMaterialInterface* OldMaterial = GetMaterial(Type);
	if (Value != OldMaterial)
	{
		switch(Type)
		{
		case EText3DGroupType::Front:
		{
			FrontMaterial = Value;
			break;
		}

		case EText3DGroupType::Back:
		{
			BackMaterial = Value;
			break;
		}

		case EText3DGroupType::Extrude:
		{
			ExtrudeMaterial = Value;
			break;
		}

		case EText3DGroupType::Bevel:
		{
			BevelMaterial = Value;
			break;
		}

		default:
		{
			return;
		}
		}

		UpdateMaterial(Type, Value);
	}
}

void UText3DComponent::SetKerning(const float Value)
{
	if (!FMath::IsNearlyEqual(Kerning, Value))
	{
		Kerning = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetLineSpacing(const float Value)
{
	if (!FMath::IsNearlyEqual(LineSpacing, Value))
	{
		LineSpacing = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetWordSpacing(const float Value)
{
	if (!FMath::IsNearlyEqual(WordSpacing, Value))
	{
		WordSpacing = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetHorizontalAlignment(const EText3DHorizontalTextAlignment Value)
{
	if (HorizontalAlignment != Value)
	{
		HorizontalAlignment = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetVerticalAlignment(const EText3DVerticalTextAlignment Value)
{
	if (VerticalAlignment != Value)
	{
		VerticalAlignment = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetHasMaxWidth(const bool Value)
{
	if (bHasMaxWidth != Value)
	{
		bHasMaxWidth = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetMaxWidth(const float Value)
{
	const float NewValue = FMath::Max(1.0f, Value);
	if (!FMath::IsNearlyEqual(MaxWidth, NewValue))
	{
		MaxWidth = NewValue;
		UpdateTransforms();
	}
}

void UText3DComponent::SetHasMaxHeight(const bool Value)
{
	if (bHasMaxHeight != Value)
	{
		bHasMaxHeight = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetMaxHeight(const float Value)
{
	const float NewValue = FMath::Max(1.0f, Value);
	if (!FMath::IsNearlyEqual(MaxHeight, NewValue))
	{
		MaxHeight = NewValue;
		UpdateTransforms();
	}
}

void UText3DComponent::SetScaleProportionally(const bool Value)
{
	if (bScaleProportionally != Value)
	{
		bScaleProportionally = Value;
		UpdateTransforms();
	}
}

void UText3DComponent::SetFreeze(const bool bFreeze)
{
	bFreezeBuild = bFreeze;
	if (bFreeze)
	{
		bPendingBuild = false;
	}
	else if (bPendingBuild)
	{
		Rebuild();
	}
}

void UText3DComponent::SetCastShadow(bool NewCastShadow)
{
	if (NewCastShadow != bCastShadow)
	{
		bCastShadow = NewCastShadow;

		for (UStaticMeshComponent* MeshComponent : CharacterMeshes)
		{
			MeshComponent->SetCastShadow(bCastShadow);
		}
		
		MarkRenderStateDirty();
	}
}

int32 UText3DComponent::GetGlyphCount()
{
	return TextRoot->GetNumChildrenComponents();
}

USceneComponent* UText3DComponent::GetGlyphKerningComponent(int32 Index)
{
	if (!CharacterKernings.IsValidIndex(Index))
	{
		return nullptr;
	}

	return CharacterKernings[Index];
}

const TArray<USceneComponent*>& UText3DComponent::GetGlyphKerningComponents()
{
	return CharacterKernings;
}

UStaticMeshComponent* UText3DComponent::GetGlyphMeshComponent(int32 Index)
{
	if (!CharacterKernings.IsValidIndex(Index))
	{
		return nullptr;
	}

	return CharacterMeshes[Index];
}

const TArray<UStaticMeshComponent*>& UText3DComponent::GetGlyphMeshComponents()
{
	return CharacterMeshes;
}

void UText3DComponent::Rebuild(const bool bCleanCache)
{
	bPendingBuild = true;
	if (!bFreezeBuild)
	{
		BuildTextMesh(bCleanCache);
	}
}

void UText3DComponent::CalculateTextWidth()
{
	for (FShapedGlyphLine& ShapedLine : ShapedText->Lines)
	{
		ShapedLine.CalculateWidth(Kerning, WordSpacing);
	}
}

float UText3DComponent::GetTextHeight() const
{
	return ShapedText->Lines.Num() * ShapedText->LineHeight + (ShapedText->Lines.Num() - 1) * LineSpacing;
}

void UText3DComponent::CalculateTextScale()
{
	FVector Scale(1.0f, 1.0f, 1.0f);

	float TextMaxWidth = 0.0f;
	for (const FShapedGlyphLine& ShapedLine : ShapedText->Lines)
	{
		TextMaxWidth = FMath::Max(TextMaxWidth, ShapedLine.Width);
	}

	if (bHasMaxWidth && TextMaxWidth > MaxWidth && TextMaxWidth > 0.0f)
	{
		Scale.Y *= MaxWidth / TextMaxWidth;
		if (bScaleProportionally)
		{
			Scale.Z = Scale.Y;
		}
	}

	const float TotalHeight = GetTextHeight();
	if (bHasMaxHeight && TotalHeight > MaxHeight && TotalHeight > 0.0f)
	{
		Scale.Z *= MaxHeight / TotalHeight;
		if (bScaleProportionally)
		{
			Scale.Y = Scale.Z;
		}
	}

	if (bScaleProportionally)
	{
		Scale.X = Scale.Y;
	}

	TextScale = Scale;
}

FVector UText3DComponent::GetTextScale()
{
	if (TextScale == FVector::ZeroVector)
	{
		CalculateTextScale();
	}

	return TextScale;
}

FVector UText3DComponent::GetLineLocation(int32 LineIndex)
{
	float HorizontalOffset = 0.0f, VerticalOffset = 0.0f;
	if (LineIndex < 0 || LineIndex >= ShapedText->Lines.Num())
	{
		return FVector();
	}

	const FShapedGlyphLine& ShapedLine = ShapedText->Lines[LineIndex];

	if (HorizontalAlignment == EText3DHorizontalTextAlignment::Center)
	{
		HorizontalOffset = -ShapedLine.Width * 0.5f;
	}
	else if (HorizontalAlignment == EText3DHorizontalTextAlignment::Right)
	{
		HorizontalOffset = -ShapedLine.Width;
	}

	const float TotalHeight = GetTextHeight();
	if (VerticalAlignment != EText3DVerticalTextAlignment::FirstLine)
	{
		// First align it to Top
		VerticalOffset -= ShapedText->FontAscender;

		if (VerticalAlignment == EText3DVerticalTextAlignment::Center)
		{
			VerticalOffset += TotalHeight * 0.5f;
		}
		else if (VerticalAlignment == EText3DVerticalTextAlignment::Bottom)
		{
			VerticalOffset += TotalHeight + ShapedText->FontDescender;
		}
	}

	VerticalOffset -= LineIndex * (ShapedText->LineHeight + LineSpacing);

	return FVector(0.0f, HorizontalOffset, VerticalOffset);
}

void UText3DComponent::UpdateTransforms()
{
	CalculateTextWidth();
	CalculateTextScale();
	const FVector Scale = GetTextScale();
	TextRoot->SetRelativeScale3D(Scale);

	int32 GlyphIndex = 0;
	for (int32 LineIndex = 0; LineIndex < ShapedText->Lines.Num(); LineIndex++)
	{
		FShapedGlyphLine& Line = ShapedText->Lines[LineIndex];
		FVector Location = GetLineLocation(LineIndex);

		for (int32 LineGlyph = 0; LineGlyph < Line.GlyphsToRender.Num(); LineGlyph++)
		{
			const FVector CharLocation = Location;
			Location.Y += Line.GetAdvanced(LineGlyph, Kerning, WordSpacing);
			if (!Line.GlyphsToRender[LineGlyph].bIsVisible)
			{
				continue;
			}

			USceneComponent* GlyphKerningComponent = GetGlyphKerningComponent(GlyphIndex);
			if (GlyphKerningComponent)
			{
				GlyphKerningComponent->SetRelativeLocation(CharLocation);
			}

			GlyphIndex++;
		}
	}
}

void UText3DComponent::ClearTextMesh()
{
	CachedCounterReferences.Reset();

	for (UStaticMeshComponent* MeshComponent : CharacterMeshes)
	{
		if (MeshComponent)
		{
			MeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			MeshComponent->SetStaticMesh(nullptr);
			MeshComponent->DestroyComponent();
		}
	}
	CharacterMeshes.Reset();

	for (USceneComponent* KerningComponent : CharacterKernings)
	{
		if (KerningComponent)
		{
			KerningComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			KerningComponent->DestroyComponent();
		}
	}
	CharacterKernings.Reset();

	constexpr bool bIncludeChildDescendants = true;
	TArray<USceneComponent*> ChildComponents;
	TextRoot->GetChildrenComponents(bIncludeChildDescendants, ChildComponents);

	for (USceneComponent* ChildComponent : ChildComponents)
	{
		if (IsValid(ChildComponent))
		{
			ChildComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			ChildComponent->DestroyComponent();	
		}
	}

	if (AActor* OwnerActor = GetOwner())
	{
		constexpr bool bAlsoDestroyComponents = false; // already destroyed!
		OwnerActor->ClearInstanceComponents(bAlsoDestroyComponents);
	}
}

void UText3DComponent::BuildTextMesh(const bool bCleanCache)
{
	// If we're already building, or have a build pending, don't do anything.
	if (bIsBuilding)
	{
		return;
	}
	
	bIsBuilding = true;

	TWeakObjectPtr<UText3DComponent> WeakThis(this);

	// Execution guarded by the above flag
	AsyncTask(ENamedThreads::GameThread, [WeakThis, bCleanCache]()
	{
		if (UText3DComponent* StrongThis = WeakThis.Get())
		{
			if (!UE::IsSavingPackage(StrongThis))
			{
				StrongThis->BuildTextMeshInternal(bCleanCache);	
			}
		}
	});
}

void UText3DComponent::BuildTextMeshInternal(const bool bCleanCache)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("UText3DComponent::Rebuild"));

	ON_SCOPE_EXIT { bIsBuilding = false; };

	if (!IsRegistered())
	{
		return;
	}

	bPendingBuild = false;
	CheckBevel();

	ClearTextMesh();
	if (!Font)
	{
		return;
	}
	
	UText3DEngineSubsystem* Subsystem = GEngine->GetEngineSubsystem<UText3DEngineSubsystem>();
	FCachedFontData& CachedFontData = Subsystem->GetCachedFontData(Font);
	const FT_Face Face = CachedFontData.GetFreeTypeFace();
	if (!Face)
	{ 
		UE_LOG(LogText3D, Error, TEXT("Failed to load font data '%s'"), *CachedFontData.GetFontName());
		return;
	}

	CachedCounterReferences.Add(CachedFontData.GetCacheCounter());
	CachedCounterReferences.Add(CachedFontData.GetMeshesCacheCounter(FGlyphMeshParameters{Extrude, Bevel, BevelType, BevelSegments, bOutline, OutlineExpand}));

	ShapedText->Reset();
	ShapedText->LineHeight = Face->size->metrics.height * FontInverseScale;
	ShapedText->FontAscender = Face->size->metrics.ascender * FontInverseScale;
	ShapedText->FontDescender = Face->size->metrics.descender * FontInverseScale;
	FTextShaper::Get()->ShapeBidirectionalText(Face, Text.ToString(), ShapedText->Lines);
	
	CalculateTextWidth();
	CalculateTextScale();
	TextRoot->SetRelativeScale3D(GetTextScale());

	// Pre-allocate, avoid new'ing!
	AllocateGlyphs(Algo::TransformAccumulate(ShapedText->Lines, [&](const FShapedGlyphLine& ShapedLine)
	{
		return Algo::CountIf(ShapedLine.GlyphsToRender, [&](const FShapedGlyphEntry& ShapedGlyph)
		{
			return ShapedGlyph.bIsVisible;
		});
	},
	0));

	int32 GlyphIndex = 0;
	for (int32 LineIndex = 0; LineIndex < ShapedText->Lines.Num(); LineIndex++)
	{
		const FShapedGlyphLine& ShapedLine = ShapedText->Lines[LineIndex];
		FVector Location = GetLineLocation(LineIndex);

		for (int32 LineGlyph = 0; LineGlyph < ShapedLine.GlyphsToRender.Num(); LineGlyph++)
		{
			FVector GlyphLocation = Location;
			Location.Y += ShapedLine.GetAdvanced(LineGlyph, Kerning, WordSpacing);

			const FShapedGlyphEntry& ShapedGlyph = ShapedLine.GlyphsToRender[LineGlyph];
			if (!ShapedGlyph.bIsVisible)
			{
				continue;
			}

			// Count even when mesh is nullptr (allocation still creates components to avoid mesh building in allocation step)
			const int32 GlyphId = GlyphIndex++;

			UStaticMesh* CachedMesh = CachedFontData.GetGlyphMesh(ShapedGlyph.GlyphIndex, FGlyphMeshParameters{Extrude, Bevel, BevelType, BevelSegments, bOutline, OutlineExpand});
			if (!CachedMesh || FMath::IsNearlyZero(CachedMesh->GetBounds().SphereRadius))
			{
				continue;
			}

			if (CharacterMeshes.IsValidIndex(GlyphId))
			{
				UStaticMeshComponent* StaticMeshComponent = CharacterMeshes[GlyphId];
				StaticMeshComponent->SetStaticMesh(CachedMesh);			
				StaticMeshComponent->SetVisibility(GetVisibleFlag());
				StaticMeshComponent->SetHiddenInGame(bHiddenInGame);
				StaticMeshComponent->SetCastShadow(bCastShadow);
			}
			else
			{
				// @note: This shouldn't occur, but it does under unknown circumstances (UE-164789) so it should be handled 
				UE_LOG(LogText3D, Error, TEXT("CharacterMesh not found at index %d"), GlyphId);
			}

			if (CharacterKernings.IsValidIndex(GlyphId))
			{
				FTransform Transform;
				Transform.SetLocation(GlyphLocation);
				USceneComponent* CharacterKerningComponent = CharacterKernings[GlyphId];
				CharacterKerningComponent->SetRelativeTransform(Transform);
			}
			else
			{
				// @note: This shouldn't occur, but it does under unknown circumstances (UE-164789) so it should be handled
				UE_LOG(LogText3D, Error, TEXT("CharacterKerning not found at index %d"), GlyphId);
			}
		}
	}
	
	for (int32 Index = 0; Index < static_cast<int32>(EText3DGroupType::TypeCount); Index++)
	{
		const EText3DGroupType Type = static_cast<EText3DGroupType>(Index);
		UpdateMaterial(Type, GetMaterial(Type));
	}

	TextGeneratedNativeDelegate.Broadcast();
	TextGeneratedDelegate.Broadcast();

	if (bCleanCache)
	{
		Subsystem->Cleanup();
	}
}

void UText3DComponent::CheckBevel()
{
	if (Bevel > MaxBevel())
	{
		Bevel = MaxBevel();
	}
}

float UText3DComponent::MaxBevel() const
{
	return Extrude / 2.0f;
}

void UText3DComponent::UpdateMaterial(const EText3DGroupType Type, UMaterialInterface* Material)
{
	// Material indices are affected by some options
	const bool bHasBevel = !bOutline && !FMath::IsNearlyZero(Bevel);
	if (!bHasBevel && Type == EText3DGroupType::Bevel)
	{
		return; 
	}
	
	const bool bHasExtrude = !FMath::IsNearlyZero(Extrude);
	if (!bHasExtrude && Type == EText3DGroupType::Extrude)
	{
		return;
	}

	int32 Index = static_cast<int32>(Type);
	Index -= !bHasBevel && Type >= EText3DGroupType::Bevel ? 1 : 0; // if no bevel, and the input is bevel or above (bevel, side/extrude, back), offset -1
	Index -= !bHasExtrude && Type >= EText3DGroupType::Extrude ? 1 : 0; // if no extrude, and the input is side/extrude or above (back), offset -1

	for (UStaticMeshComponent* StaticMeshComponent : CharacterMeshes)
	{
		StaticMeshComponent->SetMaterial(Index, Material);
	}
}

void UText3DComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();
	const bool Visibility = GetVisibleFlag();
	for (UStaticMeshComponent* StaticMeshComponent : CharacterMeshes)
	{
		StaticMeshComponent->SetVisibility(Visibility);
	}
}

void UText3DComponent::OnHiddenInGameChanged()
{
	Super::OnHiddenInGameChanged();
	for (UStaticMeshComponent* StaticMeshComponent : CharacterMeshes)
	{
		StaticMeshComponent->SetHiddenInGame(bHiddenInGame);
	}
}

void UText3DComponent::GetBounds(FVector& Origin, FVector& BoxExtent)
{
	FBox Box(ForceInit);

	for (const UStaticMeshComponent* StaticMeshComponent : CharacterMeshes)
	{
		Box += StaticMeshComponent->Bounds.GetBox();
	}

	Box.GetCenterAndExtents(Origin, BoxExtent);
}

#undef LOCTEXT_NAMESPACE
