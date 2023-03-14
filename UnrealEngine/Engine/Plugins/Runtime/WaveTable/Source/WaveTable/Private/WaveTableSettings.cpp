// Copyright Epic Games, Inc. All Rights Reserved.
#include "WaveTableSettings.h"


TArrayView<const float> FWaveTableSettings::GetEditSourceView() const
{
	int32 SourceNumSamples = SourcePCMData.Num();
	if (SourceNumSamples == 0)
	{
		return { };
	}

	int32 SourceTopOffset = 0;
	if (Top > 0.0f)
	{
		SourceTopOffset = WaveTable::RatioToIndex(Top, SourceNumSamples);
		SourceNumSamples -= SourceTopOffset;
	}

	if (Tail > 0.0f)
	{
		const int32 SourceTailOffset = WaveTable::RatioToIndex(Tail, SourceNumSamples);
		SourceNumSamples -= SourceTailOffset;
	}

	if (SourceTopOffset < SourcePCMData.Num())
	{
		return { SourcePCMData.GetData() + SourceTopOffset, SourceNumSamples };
	}

	return { };
}

namespace WaveTable
{
	int32 GetWaveTableSize(EWaveTableResolution InWaveTableResolution, EWaveTableCurve InCurve, int32 InMaxPCMSize)
	{
		auto GetCurveResolution = [](EWaveTableCurve Curve)
		{
			switch (Curve)
			{
				case EWaveTableCurve::File:
				{
					return EWaveTableResolution::Maximum;
				}
				break;

				case EWaveTableCurve::Linear:
				case EWaveTableCurve::Linear_Inv:
				{
					return EWaveTableResolution::Res_8;
				}
				break;

				case EWaveTableCurve::Exp:
				case EWaveTableCurve::Exp_Inverse:
				case EWaveTableCurve::Log:
				{
					return EWaveTableResolution::Res_256;
				}
				break;

				case EWaveTableCurve::Sin:
				case EWaveTableCurve::SCurve:
				case EWaveTableCurve::Sin_Full:
				{
					return EWaveTableResolution::Res_64;
				}
				break;

				default:
				{
					return EWaveTableResolution::Res_128;
				}
				break;
			};
		};

		switch (InWaveTableResolution)
		{
			case EWaveTableResolution::Maximum:
			{
				return FMath::Max(InMaxPCMSize, 1 << static_cast<int32>(EWaveTableResolution::Res_Max));
			}
			break;

			case EWaveTableResolution::None:
			{
				const EWaveTableResolution CurveRes = GetCurveResolution(InCurve);
				return 1 << static_cast<int32>(CurveRes);
			}
			break;

			default:
			{
				return 1 << static_cast<int32>(InWaveTableResolution);
			}
		};
	}
} // namespace WaveTable
