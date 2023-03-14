// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingScreenComponent.h"

#include "DMXPixelMappingRuntimeCommon.h"
#include "DMXPixelMappingTypes.h"
#include "DMXPixelMappingUtils.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXPortManager.h"

#if WITH_EDITOR
#include "DMXPixelMappingComponentWidget.h"
#include "SDMXPixelMappingScreenComponentBox.h"
#endif // WITH_EDITOR

#include "Engine/Texture.h"


DECLARE_CYCLE_STAT(TEXT("Send Screen"), STAT_DMXPixelMaping_SendScreen, STATGROUP_DMXPIXELMAPPING);

#define LOCTEXT_NAMESPACE "DMXPixelMappingScreenComponent"

const FVector2D UDMXPixelMappingScreenComponent::MinGridSize = FVector2D(1.f);

UDMXPixelMappingScreenComponent::UDMXPixelMappingScreenComponent()
	: bSendToAllOutputPorts(true)
{
	SetSize(FVector2D(500.f, 500.f)); 
	
	NumXCells = 10;
	NumYCells = 10;

	PixelFormat = EDMXCellFormat::PF_RGB;
	bIgnoreAlphaChannel = true;

	LocalUniverse = 1;
	StartAddress = 1;
	PixelIntensity = 1;
	AlphaIntensity = 1;
	Distribution = EDMXPixelMappingDistribution::TopLeftToRight;
}

#if WITH_EDITOR
void UDMXPixelMappingScreenComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	// Call the parent at the first place
	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);
	
	if (PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, OutputPortReferences))
	{
		// Rebuild the set of ports
		OutputPorts.Reset();
		for (const FDMXOutputPortReference& OutputPortReference : OutputPortReferences)
		{
			const FDMXOutputPortSharedRef* OutputPortPtr = FDMXPortManager::Get().GetOutputPorts().FindByPredicate([&OutputPortReference](const FDMXOutputPortSharedRef& OutputPort) {
				return OutputPort->GetPortGuid() == OutputPortReference.GetPortGuid();
				});

			if (OutputPortPtr)
			{
				OutputPorts.Add(*OutputPortPtr);
			}
		}
	}
	

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, NumXCells) ||
		PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, NumYCells) ||
		PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, LocalUniverse) ||
		PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, StartAddress) ||
		PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, Distribution) ||
		PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, PixelFormat) ||
		PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, bShowAddresses) ||
		PropertyChangedChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingScreenComponent, bShowUniverse))
	{
		if (ScreenComponentBox.IsValid())
		{
			FDMXPixelMappingScreenComponentGridParams GridParams;
			GridParams.bShowAddresses = bShowAddresses;
			GridParams.bShowUniverse = bShowUniverse;
			GridParams.Distribution = Distribution;
			GridParams.NumXCells = NumXCells;
			GridParams.NumYCells = NumYCells;
			GridParams.PixelFormat = PixelFormat;
			GridParams.LocalUniverse = LocalUniverse;
			GridParams.StartAddress = StartAddress;

			ScreenComponentBox->RebuildGrid(GridParams);
		}
	}
	if (PropertyChangedChainEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if (PropertyChangedChainEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetPositionXPropertyName() ||
			PropertyChangedChainEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetPositionYPropertyName())
		{
			if (ComponentWidget_DEPRECATED.IsValid())
			{
				ComponentWidget_DEPRECATED->SetPosition(GetPosition());
			}
		}
		else if (PropertyChangedChainEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetSizeXPropertyName() ||
			PropertyChangedChainEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetSizeYPropertyName())
		{
			if (ComponentWidget_DEPRECATED.IsValid())
			{
				ComponentWidget_DEPRECATED->SetSize(GetPosition());
			}
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR

#if WITH_EDITOR
const FText UDMXPixelMappingScreenComponent::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}
#endif // WITH_EDITOR

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedRef<FDMXPixelMappingComponentWidget> UDMXPixelMappingScreenComponent::BuildSlot(TSharedRef<SConstraintCanvas> InCanvas)
{
	if (!ComponentWidget_DEPRECATED.IsValid())
	{
		ScreenComponentBox = 
			SNew(SDMXPixelMappingScreenComponentBox)
			.NumXCells(NumXCells)
			.NumYCells(NumYCells)
			.Distribution(Distribution)
			.PixelFormat(PixelFormat)
			.LocalUniverse(LocalUniverse)
			.StartAddress(StartAddress)
			.bShowAddresses(bShowAddresses)
			.bShowUniverse(bShowUniverse);

		ComponentWidget_DEPRECATED = MakeShared<FDMXPixelMappingComponentWidget>(ScreenComponentBox, nullptr);

		ComponentWidget_DEPRECATED->AddToCanvas(InCanvas, ZOrder);
		ComponentWidget_DEPRECATED->SetPosition(GetPosition());
		ComponentWidget_DEPRECATED->SetSize(GetSize());
		ComponentWidget_DEPRECATED->SetColor(GetEditorColor());
		ComponentWidget_DEPRECATED->SetLabelText(FText::FromString(GetUserFriendlyName()));
	}
	
	return ComponentWidget_DEPRECATED.ToSharedRef();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR

const FName& UDMXPixelMappingScreenComponent::GetNamePrefix()
{
	static FName NamePrefix = TEXT("DMX Screen");
	return NamePrefix;
}

void UDMXPixelMappingScreenComponent::AddColorToSendBuffer(const FColor& InColor, TArray<uint8>& OutDMXSendBuffer)
{
	if (PixelFormat == EDMXCellFormat::PF_R)
	{
		OutDMXSendBuffer.Add(InColor.R);
	}
	else if (PixelFormat == EDMXCellFormat::PF_G)
	{
		OutDMXSendBuffer.Add(InColor.G);
	}
	else if (PixelFormat == EDMXCellFormat::PF_B)
	{
		OutDMXSendBuffer.Add(InColor.B);
	}
	else if (PixelFormat == EDMXCellFormat::PF_RG)
	{
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.G);
	}
	else if (PixelFormat == EDMXCellFormat::PF_RB)
	{
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.B);
	}
	else if (PixelFormat == EDMXCellFormat::PF_GB)
	{
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.B);
	}
	else if (PixelFormat == EDMXCellFormat::PF_GR)
	{
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.R);
	}
	else if (PixelFormat == EDMXCellFormat::PF_BR)
	{
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(InColor.R);
	}
	else if (PixelFormat == EDMXCellFormat::PF_BG)
	{
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(InColor.G);
	}
	else if (PixelFormat == EDMXCellFormat::PF_RGB)
	{
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.B);
	}
	else if (PixelFormat == EDMXCellFormat::PF_BRG)
	{
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.G);
	}
	else if (PixelFormat == EDMXCellFormat::PF_GRB)
	{
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.B);
	}
	else if (PixelFormat == EDMXCellFormat::PF_GBR)
	{
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(InColor.R);
	}
	else if (PixelFormat == EDMXCellFormat::PF_RGBA)
	{
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(bIgnoreAlphaChannel ? 0 : InColor.A);
	}
	else if (PixelFormat == EDMXCellFormat::PF_GBRA)
	{
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(bIgnoreAlphaChannel ? 0 : InColor.A);
	}
	else if (PixelFormat == EDMXCellFormat::PF_BRGA)
	{
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(bIgnoreAlphaChannel ? 0 : InColor.A);
	}
	else if (PixelFormat == EDMXCellFormat::PF_GRBA)
	{
		OutDMXSendBuffer.Add(InColor.G);
		OutDMXSendBuffer.Add(InColor.R);
		OutDMXSendBuffer.Add(InColor.B);
		OutDMXSendBuffer.Add(bIgnoreAlphaChannel ? 0 : InColor.A);
	}
}

UDMXPixelMappingRendererComponent* UDMXPixelMappingScreenComponent::GetRendererComponent() const
{
	return Cast<UDMXPixelMappingRendererComponent>(GetParent());
}

void UDMXPixelMappingScreenComponent::ResetDMX()
{
	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
	if (ensure(RendererComponent))
	{
	RendererComponent->ResetColorDownsampleBufferPixels(PixelDownsamplePositionRange.Key, PixelDownsamplePositionRange.Value);
	SendDMX();
}
}

void UDMXPixelMappingScreenComponent::SendDMX()
{
	SCOPE_CYCLE_COUNTER(STAT_DMXPixelMaping_SendScreen);

	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
	if (!ensure(RendererComponent))
	{
		return;
	}

	if (LocalUniverse < 0)
	{
		UE_LOG(LogDMXPixelMappingRuntime, Warning, TEXT("LocalUniverse < 0"));
		return;
	}

	// Helper to send to the correct ports, depending on bSendToAllOutputPorts
	auto SendDMXToPorts = [this](int32 InUniverseID, const TMap<int32, uint8>& InChannelToValueMap)
	{
		if (bSendToAllOutputPorts)
		{
			for (const FDMXOutputPortSharedRef& OutputPort : FDMXPortManager::Get().GetOutputPorts())
			{
				OutputPort->SendDMX(InUniverseID, InChannelToValueMap);
			}
		}
		else
		{
			for (const FDMXOutputPortSharedRef& OutputPort : OutputPorts)
			{
				OutputPort->SendDMX(InUniverseID, InChannelToValueMap);
			}
		}
	};

	TArray<FLinearColor> UnsortedList; 
	if (RendererComponent->GetDownsampleBufferPixels(PixelDownsamplePositionRange.Key, PixelDownsamplePositionRange.Value, UnsortedList))
	{
		TArray<FLinearColor> SortedList;
		SortedList.Reserve(UnsortedList.Num());
		FDMXPixelMappingUtils::TextureDistributionSort<FLinearColor>(Distribution, NumXCells, NumYCells, UnsortedList, SortedList);

		// Sending only if there enough space at least for one pixel
		if (!FDMXPixelMappingUtils::CanFitCellIntoChannels(PixelFormat, StartAddress))
		{
			return;
		}

		// Prepare Universes for send
		TArray<uint8> SendBuffer;
		for (const FLinearColor& LinearColor : SortedList)
		{
			constexpr bool bUseSRGB = true;
			FColor Color = LinearColor.ToFColor(bUseSRGB);
		
			const float MaxValue = 255.f;
			Color.R = static_cast<uint8>(FMath::Min(Color.R * PixelIntensity, MaxValue));
			Color.G = static_cast<uint8>(FMath::Min(Color.G * PixelIntensity, MaxValue));
			Color.B = static_cast<uint8>(FMath::Min(Color.B * PixelIntensity, MaxValue));
			Color.A = static_cast<uint8>(FMath::Min(Color.A * AlphaIntensity, MaxValue));;
			AddColorToSendBuffer(Color, SendBuffer);
		}

		// Start sending
		const uint32 UniverseMaxChannels = FDMXPixelMappingUtils::GetUniverseMaxChannels(PixelFormat, StartAddress);
		uint32 SendDMXIndex = StartAddress;
		int32 UniverseToSend = LocalUniverse;
		const int32 SendBufferNum = SendBuffer.Num();
		TMap<int32, uint8> ChannelToValueMap;
		for (int32 FragmentMapIndex = 0; FragmentMapIndex < SendBufferNum; FragmentMapIndex++)
		{
			// ready to send here
			if (SendDMXIndex > UniverseMaxChannels)
			{
				SendDMXToPorts(UniverseToSend, ChannelToValueMap);

				// Now reset
				ChannelToValueMap.Empty();
				SendDMXIndex = StartAddress;
				UniverseToSend++;
			}

			// should be channels from 1...UniverseMaxChannels
			ChannelToValueMap.Add(SendDMXIndex, SendBuffer[FragmentMapIndex]);
			if (!ensureMsgf(SendDMXIndex < UniverseMaxChannels || SendDMXIndex > 0, TEXT("Pixel Mapping Screen Component trying to send out of universe range.")))
			{
				break;
			}

			// send dmx if next iteration is the last one
			if ((SendBufferNum > FragmentMapIndex + 1) == false)
			{
				SendDMXToPorts(UniverseToSend, ChannelToValueMap);
				break;
			}

			SendDMXIndex++;
		}
	}
}

void UDMXPixelMappingScreenComponent::QueueDownsample()
{
	// Queue pixels into the downsample rendering
	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
	if (!ensure(RendererComponent))
	{
		return;
	}

	UTexture* InputTexture = RendererComponent->GetRendererInputTexture();
	if (!ensure(InputTexture))
	{
		return;
	}

	const uint32 TextureSizeX = InputTexture->GetResource()->GetSizeX();
	const uint32 TextureSizeY = InputTexture->GetResource()->GetSizeY();
	check(TextureSizeX > 0 && TextureSizeY > 0);
	
	constexpr bool bStaticCalculateUV = true;
	const FVector2D SizePixel(GetSize().X / NumXCells, GetSize().Y / NumYCells);
	const FVector2D UVSize(SizePixel.X / TextureSizeX, SizePixel.Y / TextureSizeY);
	const FVector2D UVCellSize = UVSize / 2.f;
	const int32 PixelNum = NumXCells * NumYCells;
	const FVector4 PixelFactor(1.f, 1.f, 1.f, 1.f);
	const FIntVector4 InvertPixel(0);

	// Start of downsample index
	PixelDownsamplePositionRange.Key = RendererComponent->GetDownsamplePixelNum();

	int32 IterationCount = 0;
	ForEachPixel([&](const int32 InXYIndex, const int32 XIndex, const int32 YIndex)
        {
            const FIntPoint PixelPosition = RendererComponent->GetPixelPosition(InXYIndex + PixelDownsamplePositionRange.Key);
            const FVector2D UV = FVector2D((GetPosition().X + SizePixel.X * XIndex) / TextureSizeX, (GetPosition().Y + SizePixel.Y * YIndex) / TextureSizeY);

            FDMXPixelMappingDownsamplePixelParam DownsamplePixelParam
            {
                PixelFactor,
                InvertPixel,
                PixelPosition,
                UV,
                UVSize,
                UVCellSize,
                CellBlendingQuality,
                bStaticCalculateUV
            };

            RendererComponent->AddPixelToDownsampleSet(MoveTemp(DownsamplePixelParam));

            IterationCount = InXYIndex;
        });

	// End of downsample index
	PixelDownsamplePositionRange.Value = PixelDownsamplePositionRange.Key + IterationCount;
}

void UDMXPixelMappingScreenComponent::RenderWithInputAndSendDMX()
{
	if (UDMXPixelMappingRendererComponent* RendererComponent = GetFirstParentByClass<UDMXPixelMappingRendererComponent>(this))
	{
		RendererComponent->RendererInputTexture();
	}

	RenderAndSendDMX();
}

bool UDMXPixelMappingScreenComponent::CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const
{
	return Component && Component->IsA<UDMXPixelMappingRendererComponent>();
}

const FVector2D UDMXPixelMappingScreenComponent::GetScreenPixelSize() const
{
	return FVector2D(GetSize().X / NumXCells, GetSize().Y / NumYCells);
}

void UDMXPixelMappingScreenComponent::ForEachPixel(ForEachPixelCallback InCallback)
{
	int32 IndexXY = 0;
	for (int32 NumYIndex = 0; NumYIndex < NumYCells; ++NumYIndex)
	{
	for (int32 NumXIndex = 0; NumXIndex < NumXCells; ++NumXIndex)
	{
			InCallback(IndexXY, NumXIndex, NumYIndex);
			IndexXY++;
		}
	}
}

void UDMXPixelMappingScreenComponent::SetPosition(const FVector2D& NewPosition)
{
	Super::SetPosition(NewPosition);

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetPosition(GetPosition());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

void UDMXPixelMappingScreenComponent::SetSize(const FVector2D& NewSize)
{
	Super::SetSize(NewSize);

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetSize(GetSize());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

#undef LOCTEXT_NAMESPACE
