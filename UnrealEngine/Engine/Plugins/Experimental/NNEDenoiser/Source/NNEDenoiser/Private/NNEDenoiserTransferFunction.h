// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/UnrealMathUtility.h"

namespace UE::NNEDenoiser::Private
{
	constexpr float HDRMax = 65504.f; // maximum HDR value

	// from https://github.com/OpenImageDenoise/oidn
	struct FPU
	{
		static constexpr float a  =  1.41283765e+03f;
		static constexpr float b  =  1.64593172e+00f;
		static constexpr float c  =  4.31384981e-01f;
		static constexpr float d  = -2.94139609e-03f;
		static constexpr float e  =  1.92653254e-01f;
		static constexpr float f  =  6.26026094e-03f;
		static constexpr float g  =  9.98620152e-01f;
		static constexpr float y0 =  1.57945760e-06f;
		static constexpr float y1 =  3.22087631e-02f;
		static constexpr float x0 =  2.23151711e-03f;
		static constexpr float x1 =  3.70974749e-01f;

		static float Forward(float y)
		{
			if (y <= y0)
			return a * y;
			else if (y <= y1)
			return b * FMath::Pow(y, c) + d;
			else
			return e * FMath::Loge(y + f) + g;
		}

		static float Inverse(float x)
		{
			if (x <= x0)
			return x / a;
			else if (x <= x1)
			return FMath::Pow((x - d) / b, 1.f/c);
			else
			return FMath::Exp((x - g) / e) - f;
		}
	};

	class ITransferFunction
	{
		public:
			virtual float Forward(float Y) = 0;
			virtual float Inverse(float X) = 0;
	};

	class FHDRTransferFunction : public ITransferFunction
	{
		public:
			explicit FHDRTransferFunction(float Ymax)
			{
				InvNormScale = FPU::Forward(Ymax);
				NormScale = 1.0f / InvNormScale;
			}

			float Forward(float Y) override
			{
				return FPU::Forward(Y) * NormScale;
			}

			float Inverse(float X) override
			{
				return FPU::Inverse(X * InvNormScale);
			}

			float GetNormScale() const
			{
				return NormScale;
			}

			float GetInvNormScale() const
			{
				return InvNormScale;
			}

		private:
			float NormScale = 1.0f;
			float InvNormScale = 1.0f;
	};

}