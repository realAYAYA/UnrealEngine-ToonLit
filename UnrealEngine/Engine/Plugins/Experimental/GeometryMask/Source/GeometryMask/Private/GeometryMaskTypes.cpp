// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskTypes.h"

#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "GeometryMaskSubsystem.h"
#include "GeometryMaskWorldSubsystem.h"
#include "UObject/UObjectThreadContext.h"

namespace UE::GeometryMask
{
	EGeometryMaskColorChannel VectorToMaskChannel(const FLinearColor& InVector)
	{
		float CurrentMax = std::numeric_limits<float>::min();
		int32 MaxIdx = 0;
		for (int32 ElementIdx = 0; ElementIdx < 4; ++ElementIdx)
		{
			const float CurrentValue = InVector.Component(ElementIdx);			
			if (CurrentValue > CurrentMax)
			{
				CurrentMax = CurrentValue;
				MaxIdx = ElementIdx;
			}
		}
		
		return static_cast<EGeometryMaskColorChannel>(MaxIdx);
	}

	EGeometryMaskColorChannel GetValidMaskChannel(
		EGeometryMaskColorChannel InColorChannel,
		bool bInIncludeAlpha)
	{
		return FMath::Clamp(InColorChannel, EGeometryMaskColorChannel::Red, bInIncludeAlpha ? EGeometryMaskColorChannel::Alpha : EGeometryMaskColorChannel::Blue);
	}

	FStringView ChannelToString(EGeometryMaskColorChannel InColorChannel)
	{
		return MaskChannelEnumToString[FMath::Clamp(InColorChannel, EGeometryMaskColorChannel::Red, EGeometryMaskColorChannel::Num)];
	}
}

uint32 GetTypeHash(const FGeometryMaskCanvasId& InCanvasId)
{
	return HashCombineFast(
		HashCombineFast(
			GetTypeHash(InCanvasId.World),
			GetTypeHash(InCanvasId.Name)),
			InCanvasId.SceneViewIndex);
}

uint32 GetTypeHash(const FGeometryMaskDrawingContext& InUpdateContext)
{
	return HashCombineFast(GetTypeHash(InUpdateContext.World), InUpdateContext.SceneViewIndex);
}

const FGeometryMaskCanvasId& FGeometryMaskCanvasId::None = FGeometryMaskCanvasId(EForceInit::ForceInit);
const FName FGeometryMaskCanvasId::DefaultCanvasName = TEXT("Default");

FGeometryMaskCanvasId::FGeometryMaskCanvasId(const UWorld* InWorld, const FName InName)
	: World(InWorld)
	, Name(InName)
{
}

FGeometryMaskCanvasId::FGeometryMaskCanvasId(EForceInit)
{
	ResetToNone();
}

bool FGeometryMaskCanvasId::IsDefault() const
{
	return Name.IsEqual(DefaultCanvasName);
}

bool FGeometryMaskCanvasId::IsNone() const
{
	return Name.IsNone();
}

void FGeometryMaskCanvasId::ResetToNone()
{
	World = nullptr;
	SceneViewIndex = 0;
	Name = NAME_None;
}

FString FGeometryMaskCanvasId::ToString() const
{
	FString WorldLabel = TEXT("(Transient)");
	FString WorldTypeLabel = TEXT("");
	if (const UWorld* ResolvedWorld = World.ResolveObjectPtr())
	{
		WorldLabel = ResolvedWorld->GetName();
		WorldTypeLabel = LexToString(ResolvedWorld->WorldType);
	}

	return FString::Printf(TEXT("%s(%s).%s"), *WorldLabel, *WorldTypeLabel, *Name.ToString());
}

FGeometryMaskDrawingContext::FGeometryMaskDrawingContext(TObjectKey<UWorld> InWorld, const uint8 InSceneViewIndex)
	: World(MoveTemp(InWorld))
	, SceneViewIndex(InSceneViewIndex)
	, ViewportSize(ForceInit)
	, ViewProjectionMatrix(ForceInit)
{
}

FGeometryMaskDrawingContext::FGeometryMaskDrawingContext(const UWorld* InWorld, const uint8 InSceneViewIndex)
	: World(InWorld)
	, SceneViewIndex(InSceneViewIndex)
	, ViewportSize(ForceInit)
	, ViewProjectionMatrix(ForceInit)
{
}

FGeometryMaskDrawingContext::FGeometryMaskDrawingContext(EForceInit)
	: World(nullptr)
	, ViewportSize(ForceInit)
	, ViewProjectionMatrix(ForceInit)
{
}

bool FGeometryMaskDrawingContext::IsValid() const
{
	return World.ResolveObjectPtr() != nullptr;
}

UGeometryMaskCanvasReferenceComponentBase::~UGeometryMaskCanvasReferenceComponentBase()
{
	Cleanup();
}

UCanvasRenderTarget2D* UGeometryMaskCanvasReferenceComponentBase::GetTexture()
{
	if (const UGeometryMaskCanvas* Canvas = CanvasWeak.Get())
	{
		return Canvas->GetTexture();
	}
	
	return nullptr;
}

void UGeometryMaskCanvasReferenceComponentBase::BeginPlay()
{
	Super::BeginPlay();

	TryResolveCanvas();
}

void UGeometryMaskCanvasReferenceComponentBase::PostLoad()
{
	Super::PostLoad();

	if (!IsTemplate())
	{
		TryResolveCanvas();
	}
}

void UGeometryMaskCanvasReferenceComponentBase::OnRegister()
{
	Super::OnRegister();

	if (!IsTemplate())
	{
		TryResolveCanvas();
	}
}

bool UGeometryMaskCanvasReferenceComponentBase::TryResolveNamedCanvas(FName InCanvasName)
{
	auto BroadcastSetCanvas = [this](const UGeometryMaskCanvas* InCanvas)
	{
		ReceiveSetCanvas(InCanvas);
		OnSetCanvasDelegate.Broadcast(InCanvas);
	};

	// Get current canvas
	UGeometryMaskCanvas* Canvas = CanvasWeak.Get();
	
	if (Canvas)
	{
		// Canvas set, no need to resolve it
		if (Canvas->GetFName() == InCanvasName)
		{
			return true;
		}

		// Canvas is not the correct one
		CanvasWeak.Reset();
	}

	if (UGeometryMaskWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UGeometryMaskWorldSubsystem>())
	{
		Canvas = Subsystem->GetNamedCanvas(InCanvasName);

		CanvasWeak = Canvas;
		
		if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
		{
			if (Canvas)
			{
				BroadcastSetCanvas(Canvas);
			}
		}
	}

	return CanvasWeak.IsValid();
}

bool UGeometryMaskCanvasReferenceComponentBase::Cleanup()
{
	if (!GEngine)
	{
		return false;
	}

	return true;
}
