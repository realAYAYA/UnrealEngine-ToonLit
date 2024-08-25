// Copyright Epic Games, Inc. All Rights Reserved.

#include "Text3DComponent.h"

#include "Algo/Count.h"
#include "Async/Async.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateTextShaper.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Glyph.h"
#include "Materials/Material.h"
#include "MeshCreator.h"
#include "Misc/ScopeExit.h"
#include "Misc/TransactionObjectEvent.h"
#include "Styling/StyleDefaults.h"
#include "Text3DEngineSubsystem.h"
#include "Text3DPrivate.h"
#include "TextShaper.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "Text3D"

#if WITH_EDITOR
namespace UE::Text3D::Private
{
	/** Get the Group Type according to the material name. */
	static TMap<FName, EText3DGroupType> MaterialToGroup =
	{
		{ TEXT("FrontMaterial"), EText3DGroupType::Front },
		{ TEXT("BackMaterial"), EText3DGroupType::Back },
		{ TEXT("ExtrudeMaterial"), EText3DGroupType::Extrude },
		{ TEXT("BevelMaterial"), EText3DGroupType::Bevel }
	};

	/** Sets the material based on the group name. */
	using FGetter = TMemFunPtrType<true, UText3DComponent, UMaterialInterface*()>::Type;
	static TMap<FName, FGetter> GroupToMaterial =
	{
		{ TEXT("FrontMaterial"), &UText3DComponent::GetFrontMaterial },
		{ TEXT("BackMaterial"), &UText3DComponent::GetBackMaterial },
		{ TEXT("ExtrudeMaterial"), &UText3DComponent::GetExtrudeMaterial },
		{ TEXT("BevelMaterial"), &UText3DComponent::GetBevelMaterial }
	};
}
#endif

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
	TextRoot->SetupAttachment(this);

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

	bFreezeBuild = false;
	ModifyFlags = EText3DModifyFlags::All;

	TextScale = FVector::ZeroVector;

	RefreshTypeface();
}

void UText3DComponent::PostLoad()
{
	// Reset so it's rebuilt (needed if re-using the component!)
	ModifyFlags = EText3DModifyFlags::All;

	Super::PostLoad();
}

void UText3DComponent::BeginDestroy()
{
	ClearTextMesh();

	Super::BeginDestroy();
}

bool UText3DComponent::NeedsMeshRebuild() const
{
	return EnumHasAnyFlags(ModifyFlags,EText3DModifyFlags::Geometry);
}

bool UText3DComponent::NeedsLayoutUpdate() const
{
	return EnumHasAnyFlags(ModifyFlags, EText3DModifyFlags::Layout);
}

void UText3DComponent::MarkForGeometryUpdate()
{
	ModifyFlags |= EText3DModifyFlags::Geometry;
}

void UText3DComponent::MarkForLayoutUpdate()
{
	ModifyFlags |= EText3DModifyFlags::Layout;
}

void UText3DComponent::ClearUpdateFlags()
{
	ModifyFlags = EText3DModifyFlags::None;
}

uint32 UText3DComponent::GetTypeFaceIndex() const
{
	uint32 TypefaceIndex = 0;

	if (Font)
	{
		TArray<FTypefaceEntry>& Fonts = Font->CompositeFont.DefaultTypeface.Fonts;

		for (uint32 Index = 0; Index < static_cast<uint32>(Fonts.Num()); Index++)
		{
			if (Typeface == Fonts[Index].Name)
			{
				TypefaceIndex = Index;
				break;
			}
		}
	}

	return TypefaceIndex;
}

bool UText3DComponent::IsTypefaceAvailable(FName InTypeface) const
{
	for (const FTypefaceEntry& TypefaceElement : GetAvailableTypefaces())
	{
		if (InTypeface == TypefaceElement.Name)
		{
			return true;
		}
	}

	return false;
}

TArray<FTypefaceEntry> UText3DComponent::GetAvailableTypefaces() const
{
	if (Font)
	{
		return Font->CompositeFont.DefaultTypeface.Fonts;
	}

	return {};
}

void UText3DComponent::RefreshTypeface()
{
	if (Font)
	{
		TArray<FTypefaceEntry>& Fonts = Font->CompositeFont.DefaultTypeface.Fonts;
		for (int32 Index = 0; Index < Fonts.Num(); Index++)
		{
			if (Typeface == Fonts[Index].Name)
			{
				// Typeface stays the same
				return;
			}
		}

		if (!Fonts.IsEmpty())
		{
			Typeface = Fonts[0].Name;
		}
		else
		{
			Typeface = TEXT("");
		}
	}
}

void UText3DComponent::OnRegister()
{
	Super::OnRegister();

	if (!TextRoot->IsAttachedTo(this))
	{
		TextRoot->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	}

	RebuildInternal();
}

void UText3DComponent::OnUnregister()
{
	if (IsBeingDestroyed())
	{
		ClearTextMesh();
	}

	Super::OnUnregister();
}

#if WITH_EDITOR
void UText3DComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = PropertyChangedEvent.GetPropertyName();

	static FName BevelTypePropertyName = GET_MEMBER_NAME_CHECKED(UText3DComponent, BevelType);
	static FName BevelSegmentsPropertyName = GET_MEMBER_NAME_CHECKED(UText3DComponent, BevelSegments);
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

		MarkForGeometryUpdate();
	}
	else if (Name == BevelSegmentsPropertyName)
	{
		// Force minimum bevel segments based on the BevelType
		SetBevelSegments(BevelSegments);
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, Font))
	{
		MarkForGeometryUpdate();
		RefreshTypeface();
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, Typeface) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, Text) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, OutlineExpand) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, bOutline) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, Extrude) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, Bevel))
	{
		MarkForGeometryUpdate();
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, HorizontalAlignment) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, VerticalAlignment) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, Kerning) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, LineSpacing) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, WordSpacing) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, bHasMaxWidth) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, MaxWidth) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, bHasMaxHeight) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, MaxHeight) ||
			 Name == GET_MEMBER_NAME_CHECKED(UText3DComponent, bScaleProportionally))
	{
		MarkForLayoutUpdate();
	}
	else if (const EText3DGroupType* MaterialGroup = UE::Text3D::Private::MaterialToGroup.Find(Name))
	{
		UpdateMaterial(*MaterialGroup, Invoke(UE::Text3D::Private::GroupToMaterial[Name], this));
	}

	RebuildInternal();
}

void UText3DComponent::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		ModifyFlags |= EText3DModifyFlags::All;
		RebuildInternal();
	}
}
#endif

bool UText3DComponent::RefreshesOnChange() const
{
	return bRefreshOnChange;
}

void UText3DComponent::SetRefreshOnChange(const bool Value)
{
	if (bRefreshOnChange != Value)
	{
		bRefreshOnChange = Value;
	}
}

const FText& UText3DComponent::GetText() const
{
	return Text;
}

void UText3DComponent::SetText(const FText& Value)
{
	if (!Text.EqualTo(Value))
	{
		Text = Value;
		MarkForGeometryUpdate();
		RebuildInternal();
	}
}

const UFont* UText3DComponent::GetFont() const
{
	return Font;
}

void UText3DComponent::SetFont(UFont* InFont)
{
	if (Font != InFont)
	{
		Font = InFont;
		RefreshTypeface();
		MarkForGeometryUpdate();
		RebuildInternal();
	}
}

bool UText3DComponent::HasOutline() const
{
	return bOutline;
}

void UText3DComponent::SetHasOutline(const bool bValue)
{
	if (bOutline != bValue)
	{
		bOutline = bValue;
		MarkForGeometryUpdate();
		RebuildInternal();
	}
}

float UText3DComponent::GetOutlineExpand() const
{
	return OutlineExpand;
}

void UText3DComponent::SetOutlineExpand(const float Value)
{
	const float NewValue = Value;
	if (!FMath::IsNearlyEqual(OutlineExpand, NewValue))
	{
		OutlineExpand = NewValue;
		MarkForGeometryUpdate();
		RebuildInternal();
	}
}

float UText3DComponent::GetExtrude() const
{
	return Extrude;
}

void UText3DComponent::SetExtrude(const float Value)
{
	const float NewValue = FMath::Max(0.0f, Value);
	if (!FMath::IsNearlyEqual(Extrude, NewValue))
	{
		Extrude = NewValue;
		MarkForGeometryUpdate();
		CheckBevel();
		RebuildInternal();
	}
}

float UText3DComponent::GetBevel() const
{
	return Bevel;
}

void UText3DComponent::SetBevel(const float Value)
{
	const float NewValue = FMath::Clamp(Value, 0.f, MaxBevel());
	if (!FMath::IsNearlyEqual(Bevel, NewValue))
	{
		Bevel = NewValue;
		MarkForGeometryUpdate();
		RebuildInternal();
	}
}

EText3DBevelType UText3DComponent::GetBevelType() const
{
	return BevelType;
}

void UText3DComponent::SetBevelType(const EText3DBevelType Value)
{
	if (BevelType != Value)
	{
		BevelType = Value;
		MarkForGeometryUpdate();
		RebuildInternal();
	}
}

int32 UText3DComponent::GetBevelSegments() const
{
	return BevelSegments;
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
		MarkForGeometryUpdate();
		RebuildInternal();
	}
}

UMaterialInterface* UText3DComponent::GetFrontMaterial() const
{
	return FrontMaterial;
}

void UText3DComponent::SetFrontMaterial(UMaterialInterface* Value)
{
	SetMaterial(EText3DGroupType::Front, Value);
}

UMaterialInterface* UText3DComponent::GetBevelMaterial() const
{
	return BevelMaterial;
}

void UText3DComponent::SetBevelMaterial(UMaterialInterface* Value)
{
	SetMaterial(EText3DGroupType::Bevel, Value);
}

UMaterialInterface* UText3DComponent::GetExtrudeMaterial() const
{
	return ExtrudeMaterial;
}

void UText3DComponent::SetExtrudeMaterial(UMaterialInterface* Value)
{
	SetMaterial(EText3DGroupType::Extrude, Value);
}

UMaterialInterface* UText3DComponent::GetBackMaterial() const
{
	return BackMaterial;
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
			}

			UStaticMeshComponent* StaticMeshComponent = CharacterMeshes[CharacterIndex];
			if (IsValid(StaticMeshComponent))
			{
				StaticMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				StaticMeshComponent->UnregisterComponent();
				StaticMeshComponent->DestroyComponent();
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
		switch (Type)
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

float UText3DComponent::GetKerning() const
{
	return Kerning;
}

void UText3DComponent::SetKerning(const float Value)
{
	if (!FMath::IsNearlyEqual(Kerning, Value))
	{
		Kerning = Value;
		UpdateTransforms();
	}
}

float UText3DComponent::GetLineSpacing() const
{
	return LineSpacing;
}

void UText3DComponent::SetLineSpacing(const float Value)
{
	if (!FMath::IsNearlyEqual(LineSpacing, Value))
	{
		LineSpacing = Value;
		UpdateTransforms();
	}
}

float UText3DComponent::GetWordSpacing() const
{
	return WordSpacing;
}

void UText3DComponent::SetWordSpacing(const float Value)
{
	if (!FMath::IsNearlyEqual(WordSpacing, Value))
	{
		WordSpacing = Value;
		UpdateTransforms();
	}
}

EText3DHorizontalTextAlignment UText3DComponent::GetHorizontalAlignment() const
{
	return HorizontalAlignment;
}

void UText3DComponent::SetHorizontalAlignment(const EText3DHorizontalTextAlignment Value)
{
	if (HorizontalAlignment != Value)
	{
		HorizontalAlignment = Value;
		UpdateTransforms();
	}
}

EText3DVerticalTextAlignment UText3DComponent::GetVerticalAlignment() const
{
	return VerticalAlignment;
}

void UText3DComponent::SetVerticalAlignment(const EText3DVerticalTextAlignment Value)
{
	if (VerticalAlignment != Value)
	{
		VerticalAlignment = Value;
		UpdateTransforms();
	}
}

bool UText3DComponent::HasMaxWidth() const
{
	return bHasMaxWidth;
}

void UText3DComponent::SetHasMaxWidth(const bool Value)
{
	if (bHasMaxWidth != Value)
	{
		bHasMaxWidth = Value;
		UpdateTransforms();
	}
}

float UText3DComponent::GetMaxWidth() const
{
	return MaxWidth;
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

bool UText3DComponent::HasMaxHeight() const
{
	return bHasMaxHeight;
}

void UText3DComponent::SetHasMaxHeight(const bool Value)
{
	if (bHasMaxHeight != Value)
	{
		bHasMaxHeight = Value;
		UpdateTransforms();
	}
}

float UText3DComponent::GetMaxHeight() const
{
	return MaxHeight;
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

bool UText3DComponent::ScalesProportionally() const
{
	return bScaleProportionally;
}

void UText3DComponent::SetScaleProportionally(const bool Value)
{
	if (bScaleProportionally != Value)
	{
		bScaleProportionally = Value;
		UpdateTransforms();
	}
}

bool UText3DComponent::IsFrozen() const
{
	return bFreezeBuild;
}

void UText3DComponent::SetFreeze(const bool bFreeze)
{
	bFreezeBuild = bFreeze;
	if (bFreeze)
	{
		ModifyFlags |= EText3DModifyFlags::Unfreeze;
	}
	else if (EnumHasAnyFlags(ModifyFlags,EText3DModifyFlags::Unfreeze))
	{
		RebuildInternal();
	}
}

bool UText3DComponent::CastsShadow() const
{
	return bCastShadow;
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

void UText3DComponent::SetTypeface(const FName InTypeface)
{
	if (Typeface == InTypeface || !IsTypefaceAvailable(InTypeface))
	{
		return;
	}

	Typeface = InTypeface;

	MarkForGeometryUpdate();
	RebuildInternal();
}

void UText3DComponent::Rebuild()
{
	// Rebuild, ignoring RefreshOnChange
	RebuildInternal(false);
}

void UText3DComponent::RebuildInternal(const bool& bIsAutoUpdate, const bool& bCleanCache)
{
	// If this is an auto update, but the flag is off, ignore this rebuild request
	if (bIsAutoUpdate && bRefreshOnChange == false)
	{
		return;
	}

	if (NeedsMeshRebuild() && !bFreezeBuild)
	{
		BuildTextMesh(bCleanCache);
	}
	else if (NeedsLayoutUpdate())
	{
		UpdateTransforms();
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
			Location.Y += Line.GetAdvance(LineGlyph, Kerning, WordSpacing);
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

	ModifyFlags &= ~EText3DModifyFlags::Layout;
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

	if (TextRoot)
	{
		TArray<USceneComponent*> ChildComponents;
		constexpr bool bIncludeChildDescendants = true;
		TextRoot->GetChildrenComponents(bIncludeChildDescendants, ChildComponents);

		for (USceneComponent* ChildComponent : ChildComponents)
		{
			if (IsValid(ChildComponent))
			{
				ChildComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				ChildComponent->DestroyComponent();	
			}
		}
	}
}

void UText3DComponent::TriggerInternalRebuild(const EText3DModifyFlags InModifyFlags)
{
	if (EnumHasAnyFlags(InModifyFlags, EText3DModifyFlags::Geometry))
	{
		MarkForGeometryUpdate();
	}

	if (EnumHasAnyFlags(InModifyFlags, EText3DModifyFlags::Layout))
	{
		MarkForLayoutUpdate();
	}

	RebuildInternal();
}

void UText3DComponent::BuildTextMesh(const bool& bCleanCache)
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

void UText3DComponent::BuildTextMeshInternal(const bool& bCleanCache)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UText3DComponent::Rebuild);

	ON_SCOPE_EXIT { bIsBuilding = false; };

	if (!IsRegistered())
	{
		return;
	}

	CheckBevel();

	ClearTextMesh();
	if (!Font)
	{
		return;
	}

	UText3DEngineSubsystem* Subsystem = GEngine->GetEngineSubsystem<UText3DEngineSubsystem>();

	const uint32 TypefaceIndex = GetTypeFaceIndex();
	FCachedFontData& CachedFontData = Subsystem->GetCachedFontData(Font, TypefaceIndex);
	const FT_Face Face = CachedFontData.GetFreeTypeFace(TypefaceIndex);
	if (!Face)
	{ 
		UE_LOG(LogText3D, Error, TEXT("Failed to load font data '%s'"), *CachedFontData.GetFontName());
		return;
	}

	FGlyphMeshParameters GlyphMeshParameters{Extrude, Bevel, BevelType, BevelSegments, bOutline, OutlineExpand, TypefaceIndex};
	CachedCounterReferences.Add(CachedFontData.GetCacheCounter(TypefaceIndex));
	CachedCounterReferences.Add(CachedFontData.GetMeshesCacheCounter(GlyphMeshParameters));

	ShapedText->Reset();
	ShapedText->LineHeight = Face->size->metrics.height * FontInverseScale;
	ShapedText->FontAscender = Face->size->metrics.ascender * FontInverseScale;
	ShapedText->FontDescender = Face->size->metrics.descender * FontInverseScale;

	constexpr int32 AdjustedFontSize = 48; // Magic number that makes font scale consistent with previous implementation
	FSlateFontInfo FontInfo(Font, AdjustedFontSize);
	FontInfo.CompositeFont = FStyleDefaults::GetFontInfo().CompositeFont;
	FontInfo.TypefaceFontName = Typeface;

	FTextBlockStyle Style;
	Style.SetFont(FontInfo);

	if (!TextLayout.IsValid())
	{
		TextLayout = MakeShared<FText3DLayout>(Style);
	}

	if (!TextLayoutMarshaller.IsValid())
	{
		TextLayoutMarshaller = FPlainTextLayoutMarshaller::Create();
	}

	FText FormattedText = Text;
	FormatText(FormattedText);
	FTextShaper::Get()->ShapeBidirectionalText(Style, FormattedText.ToString(), TextLayout, TextLayoutMarshaller, ShapedText->Lines);

	const int32 ApproximateGlyphNum = Algo::TransformAccumulate(ShapedText->Lines,
		[](const FShapedGlyphLine& InLine)
		{
			return InLine.GlyphsToRender.Num();
		},
		0);

	TMap<uint32, const FFreeTypeFace*> GlyphIndexToFontFace;
	GlyphIndexToFontFace.Reserve(ApproximateGlyphNum);

	for (int32 LineIndex = 0; LineIndex < ShapedText->Lines.Num(); ++LineIndex)
	{
		for (const FShapedGlyphEntry& GlyphEntry : ShapedText->Lines[LineIndex].GlyphsToRender)
		{
			if (!GlyphEntry.FontFaceData.IsValid())
			{
				// Add as nullptr if not already in map
				if (!GlyphIndexToFontFace.Contains(GlyphEntry.GlyphIndex))
				{
					GlyphIndexToFontFace.Add(GlyphEntry.GlyphIndex, nullptr);
				}

				continue;
			}

			if (const TSharedPtr<FFreeTypeFace> FontFacePtr = GlyphEntry.FontFaceData->FontFace.Pin();
				FontFacePtr.IsValid())
			{
				GlyphIndexToFontFace.FindOrAdd(GlyphEntry.GlyphIndex, FontFacePtr.Get());	
			}
		}
	}

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
		FVector LineLocation = GetLineLocation(LineIndex);

		for (int32 LineGlyph = 0; LineGlyph < ShapedLine.GlyphsToRender.Num(); LineGlyph++)
		{
			FVector GlyphLocation = LineLocation;
			LineLocation.Y += ShapedLine.GetAdvance(LineGlyph, Kerning, WordSpacing);

			const FShapedGlyphEntry& ShapedGlyph = ShapedLine.GlyphsToRender[LineGlyph];
			if (!ShapedGlyph.bIsVisible)
			{
				continue;
			}

			// Count even when mesh is nullptr (allocation still creates components to avoid mesh building in allocation step)
			const int32 GlyphId = GlyphIndex++;

			const FFreeTypeFace* FontFace = GlyphIndexToFontFace[ShapedGlyph.GlyphIndex];
			UStaticMesh* CachedMesh = CachedFontData.GetGlyphMesh(ShapedGlyph.GlyphIndex, GlyphMeshParameters, FontFace);
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

	ClearUpdateFlags();

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

FText UText3DComponent::GetFormattedText() const
{
	FText FormattedText = Text;
	FormatText(FormattedText);

	return FormattedText;
}

TArray<FName> UText3DComponent::GetTypefaceNames() const
{
	TArray<FName> TypefaceNames;

	if (Font)
	{
		for (const FTypefaceEntry& TypeFaceFont : Font->CompositeFont.DefaultTypeface.Fonts)
		{
			TypefaceNames.Add(TypeFaceFont.Name);
		}
	}

	return TypefaceNames;
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
