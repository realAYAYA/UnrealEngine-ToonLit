// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeProxyUIDetails.h"

#include "Algo/Count.h"
#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/World.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/PlatformCrt.h"
#include "IDetailPropertyRow.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSubsystem.h"
#include "Layout/Margin.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/Attribute.h"
#include "RuntimeVirtualTextureSetBounds.h"
#include "ScopedTransaction.h"
#include "Templates/Casts.h"
#include "Types/SlateEnums.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "VT/RuntimeVirtualTextureVolume.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

class IPropertyHandle;
class SWidget;
class UObject;
class URuntimeVirtualTexture;

#define LOCTEXT_NAMESPACE "FLandscapeProxyUIDetails"

FLandscapeProxyUIDetails::FLandscapeProxyUIDetails()
{
}

TSharedRef<IDetailCustomization> FLandscapeProxyUIDetails::MakeInstance()
{
	return MakeShareable( new FLandscapeProxyUIDetails);
}

void FLandscapeProxyUIDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);

	auto GenerateTextWidget = [](const FText& InText, bool bInBold = false) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
				.Font(bInBold? IDetailLayoutBuilder::GetDetailFontBold() : IDetailLayoutBuilder::GetDetailFont())
				.Text(InText);
	};

	TArray<TWeakObjectPtr<ALandscapeProxy>> EditingProxies;
	Algo::Transform(EditingObjects, EditingProxies, [](TWeakObjectPtr<UObject> InObject) { return TWeakObjectPtr<ALandscapeProxy>(Cast<ALandscapeProxy>(InObject.Get())); });

	TArray<TWeakObjectPtr<ALandscape>> LandscapeActors;
	for (TWeakObjectPtr<ALandscapeProxy> EditingProxy : EditingProxies)
	{
		if (EditingProxy.IsValid())
		{
			if (ALandscape* LandscapeActor = EditingProxy->GetLandscapeActor())
			{
				LandscapeActors.AddUnique(LandscapeActor);
			}
		}
	}

	if (LandscapeActors.Num() == 1)
	{
		TWeakObjectPtr<ALandscape> LandscapeActor = LandscapeActors[0];
		if (ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo())
		{
			IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("Information", FText::GetEmpty(), ECategoryPriority::Important);

			FText CreateRVTVolumesTooltip = LOCTEXT("Button_CreateVolumes_Tooltip", "Create volumes for the selected Runtime Virtual Textures.");
				
			FText RowDisplayText = LOCTEXT("LandscapeComponentResolution", "Component Resolution (Verts)");
			CategoryBuilder.AddCustomRow(RowDisplayText)
			.RowTag(TEXT("LandscapeComponentResolution"))
			.NameContent()
			[
				GenerateTextWidget(RowDisplayText)
			]
			.ValueContent()
			[
				GenerateTextWidget(FText::Format(LOCTEXT("LandscapeComponentResolutionValue", "{0} x {0}"), LandscapeActor->ComponentSizeQuads+1), true) // Verts
			];

			RowDisplayText = LOCTEXT("LandscapeComponentCount", "Component Count");
			CategoryBuilder.AddCustomRow(RowDisplayText)
			.RowTag(TEXT("LandscapeComponentCount"))
			.NameContent()
			[
				GenerateTextWidget(RowDisplayText)
			]
			.ValueContent()
			[
				GenerateTextWidget(FText::Format(LOCTEXT("LandscapeComponentCountValue", "{0}"), LandscapeActor->LandscapeComponents.Num()), true)
			];

			RowDisplayText = LOCTEXT("LandscapeComponentSubsections", "Component Subsections");
			CategoryBuilder.AddCustomRow(RowDisplayText)
			.RowTag(TEXT("LandscapeComponentSubsections"))
			.NameContent()
			[
				GenerateTextWidget(RowDisplayText)
			]
			.ValueContent()
			[
				GenerateTextWidget(FText::Format(LOCTEXT("LandscapeComponentSubSectionsValue", "{0} x {0}"), LandscapeActor->NumSubsections), true)
			];

			FIntRect Rect = LandscapeActor->GetBoundingRect();
			FIntPoint Size = Rect.Size();
			RowDisplayText = LOCTEXT("LandscapeResolution", "Resolution (Verts)");
			CategoryBuilder.AddCustomRow(RowDisplayText)
			.RowTag(TEXT("LandscapeResolution"))
			.NameContent()
			[
				GenerateTextWidget(RowDisplayText)
			]
			.ValueContent()
			[
				GenerateTextWidget(FText::Format(LOCTEXT("LandscapeResolutionValue", "{0} x {1}"), Size.X+1, Size.Y+1), true)
			];

			int32 LandscapeCount = LandscapeInfo->StreamingProxies.Num() + (LandscapeInfo->LandscapeActor.Get() ? 1 : 0);
			RowDisplayText = LOCTEXT("LandscapeCount", "Landscape Count");
			CategoryBuilder.AddCustomRow(RowDisplayText)
			.RowTag(TEXT("LandscapeCount"))
			.NameContent()
			[
				GenerateTextWidget(RowDisplayText)
			]
			.ValueContent()
			[
				GenerateTextWidget(FText::Format(LOCTEXT("LandscapeCountValue", "{0}"), LandscapeCount), true)
			];

			int32 TotalComponentCount = LandscapeInfo->XYtoComponentMap.Num();
			RowDisplayText = LOCTEXT("TotalLandscapeComponentCount", "Total Component Count");
			CategoryBuilder.AddCustomRow(RowDisplayText)
			.RowTag(TEXT("TotalLandscapeComponentCount"))
			.NameContent()
			[
				GenerateTextWidget(RowDisplayText)
			]
			.ValueContent()
			[
				GenerateTextWidget(FText::Format(LOCTEXT("TotalLandscapeComponentCountValue", "{0}"), TotalComponentCount), true)
			];

			LandscapeInfo->GetLandscapeExtent(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);
			Size = Rect.Size();
			RowDisplayText = LOCTEXT("LandscapeOverallResolution", "Overall Resolution (Verts)");
			CategoryBuilder.AddCustomRow(RowDisplayText)
			.RowTag(TEXT("LandscapeOverallResolution"))
			.NameContent()
			[
				GenerateTextWidget(RowDisplayText)
			]
			.ValueContent()
			[
				GenerateTextWidget(FText::Format(LOCTEXT("LandscapeOverallResolutionValue", "{0} x {1}"), Size.X + 1, Size.Y + 1), true)
			];

			// Apply custom widget for CreateVolume.
			TSharedRef<IPropertyHandle> CreateVolumePropertyHandle = DetailBuilder.GetProperty(TEXT("bSetCreateRuntimeVirtualTextureVolumes"));
			DetailBuilder.EditDefaultProperty(CreateVolumePropertyHandle)->CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("Button_CreateVolumes", "Create Volumes"))
				.ToolTipText(CreateRVTVolumesTooltip)
			]
			.ValueContent()
			.MinDesiredWidth(125.f)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ContentPadding(2)
				.Text(LOCTEXT("Button_CreateVolumes", "Create Volumes"))
				.OnClicked_Lambda([LandscapeActor, this]() { return CreateRuntimeVirtualTextureVolume(LandscapeActor.Get()); })
				.IsEnabled_Lambda([LandscapeActor, this]() { return IsCreateRuntimeVirtualTextureVolumeEnabled(LandscapeActor.Get()); })
				.ToolTipText_Lambda([LandscapeActor, this, CreateRVTVolumesTooltip]()
				{
					return IsCreateRuntimeVirtualTextureVolumeEnabled(LandscapeActor.Get())
						? CreateRVTVolumesTooltip
						: LOCTEXT("Button_CreateVolumes_Tooltip_Invalid", "No Runtime Virtual Textures found in the world.");
				})
			];
		}
	}

	// Add Nanite buttons :
	if (!EditingProxies.IsEmpty())
	{
		auto BuildNaniteData = [EditingProxies](bool bInForceRebuild) -> FReply
		{
			TArray<ALandscapeProxy*> ProxiesToBuild;
			Algo::TransformIf(EditingProxies, ProxiesToBuild, [](const TWeakObjectPtr<ALandscapeProxy>& InProxy) { return InProxy.IsValid(); }, [](const TWeakObjectPtr<ALandscapeProxy>& InProxy) { return InProxy.Get(); });
			if (ProxiesToBuild.IsEmpty())
			{
				return FReply::Unhandled();
			}

			if (UWorld* World = ProxiesToBuild[0]->GetWorld())
			{
				if (ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>())
				{
					LandscapeSubsystem->BuildNanite(MakeArrayView(ProxiesToBuild), bInForceRebuild);
				}
			}

			return FReply::Handled();
		};

		auto GetNumProxiesNeedingRebuild = [EditingProxies](bool bInForceRebuild) -> int32
		{
			TSet<TWeakObjectPtr<ALandscapeProxy>> ProxiesToBuild;
			for (TWeakObjectPtr<ALandscapeProxy> EditingProxy : EditingProxies)
			{
				if (EditingProxy.IsValid())
				{
					ProxiesToBuild.Add(EditingProxy);
					// Build all streaming proxies in the case of a ALandscape :
					if (ALandscape* Landscape = Cast<ALandscape>(EditingProxy.Get()))
					{
						ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
						if (LandscapeInfo != nullptr)
						{
							Algo::Transform(LandscapeInfo->StreamingProxies, ProxiesToBuild, [](const TWeakObjectPtr<ALandscapeStreamingProxy>& InStreamingProxy) { return InStreamingProxy; });
						}
					}
				}
			}

			return Algo::CountIf(ProxiesToBuild, [bInForceRebuild](TWeakObjectPtr<ALandscapeProxy> InProxy) { return (InProxy.IsValid() && (bInForceRebuild || !InProxy->IsNaniteMeshUpToDate())); });
		};

		auto HasAtLeastOneNaniteLandscape = [LandscapeActors]() -> bool
		{
			return Algo::FindByPredicate(LandscapeActors, [](TWeakObjectPtr<ALandscape> InLandscape) { return (InLandscape.IsValid() && InLandscape->IsNaniteEnabled()); }) != nullptr;
		};

		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("Nanite", FText::GetEmpty(), ECategoryPriority::Default);
		TSharedRef<IPropertyHandle> EnableNaniteProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ALandscape, bEnableNanite));
		CategoryBuilder.AddCustomRow(FText::FromString(TEXT("Rebuild Nanite Data")))
		[
			SNew(SHorizontalBox)
			.IsEnabled_Lambda([HasAtLeastOneNaniteLandscape] { return HasAtLeastOneNaniteLandscape(); })
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SButton)
				.Text(LOCTEXT("BuildNaniteData", "Build Data"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText_Lambda([GetNumProxiesNeedingRebuild]()
				{ 
					return FText::Format(LOCTEXT("BuildNaniteDataTooltip", "Builds the Nanite mesh representation from the Landscape data if it's not up to date ({0} {0}|plural(one=actors, other=actors) to build)"), GetNumProxiesNeedingRebuild(/*bInForceRebuild = */false));
				})
				.OnClicked_Lambda([BuildNaniteData]() { return BuildNaniteData(/*bInForceRebuild = */false); })
				.IsEnabled_Lambda([GetNumProxiesNeedingRebuild]() { return (GetNumProxiesNeedingRebuild(/*bInForceRebuild = */false) > 0); })
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SButton)
				.Text(LOCTEXT("RebuildNaniteData", "Rebuild Data"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText_Lambda([GetNumProxiesNeedingRebuild]()
				{ 
					return FText::Format(LOCTEXT("RebuildNaniteDataTooltip", "Rebuilds the Nanite mesh representation from the Landscape data ({0} {0}|plural(one=actors, other=actors) to build)"), GetNumProxiesNeedingRebuild(/*bInForceRebuild = */true));
				})
				.OnClicked_Lambda([BuildNaniteData]() { return BuildNaniteData(/*bInForceRebuild = */true); })
			]
		];
	}
}

static void GetMissingRuntimeVirtualTextureVolumes(ALandscape* InLandscapeActor, TArray<URuntimeVirtualTexture*>& OutVirtualTextures)
{
	UWorld* World = InLandscapeActor != nullptr ? InLandscapeActor->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	TArray<URuntimeVirtualTexture*> FoundVolumes;
	for (TObjectIterator<URuntimeVirtualTextureComponent> It(RF_ClassDefaultObject, false, EInternalObjectFlags::Garbage); It; ++It)
	{
		if (It->GetWorld() == World)
		{
			if (URuntimeVirtualTexture* VirtualTexture = It->GetVirtualTexture())
			{
				FoundVolumes.Add(VirtualTexture);
			}
		}
	}

	for (URuntimeVirtualTexture* VirtualTexture : InLandscapeActor->RuntimeVirtualTextures)
	{
		if (VirtualTexture != nullptr && FoundVolumes.Find(VirtualTexture) == INDEX_NONE)
		{
			OutVirtualTextures.Add(VirtualTexture);
		}
	}
}

bool FLandscapeProxyUIDetails::IsCreateRuntimeVirtualTextureVolumeEnabled(ALandscape* InLandscapeActor) const
{
	if (InLandscapeActor == nullptr)
	{
		return false;
	}

	TArray<URuntimeVirtualTexture*> VirtualTextureVolumesToCreate;
	GetMissingRuntimeVirtualTextureVolumes(InLandscapeActor, VirtualTextureVolumesToCreate);
	return VirtualTextureVolumesToCreate.Num() > 0;
}

FReply FLandscapeProxyUIDetails::CreateRuntimeVirtualTextureVolume(ALandscape* InLandscapeActor)
{
	if (InLandscapeActor == nullptr)
	{
		return FReply::Unhandled();
	}

	TArray<URuntimeVirtualTexture*> VirtualTextureVolumesToCreate;
	GetMissingRuntimeVirtualTextureVolumes(InLandscapeActor, VirtualTextureVolumesToCreate);
	if (VirtualTextureVolumesToCreate.Num() == 0)
	{
		return FReply::Unhandled();
	}

	const FScopedTransaction Transaction(LOCTEXT("Transaction_CreateVolumes", "Create Runtime Virtual Texture Volumes"));
	
	for (URuntimeVirtualTexture* VirtualTexture : VirtualTextureVolumesToCreate)
	{
		ARuntimeVirtualTextureVolume* NewVolume = InLandscapeActor->GetWorld()->SpawnActor<ARuntimeVirtualTextureVolume>();
		NewVolume->VirtualTextureComponent->SetVirtualTexture(VirtualTexture);
		NewVolume->VirtualTextureComponent->SetBoundsAlignActor(InLandscapeActor);
		RuntimeVirtualTexture::SetBounds(NewVolume->VirtualTextureComponent);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
