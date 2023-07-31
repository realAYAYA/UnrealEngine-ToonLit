// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ConvertData.h"
#include "MuR/Platform.h"


namespace mu
{
    inline float lerp(float p0, float p1, float alpha)
	{
		return p0 * (1.f - alpha) + p1 * alpha;
	}

    inline float BezierInterpolation(float p0, float p1, float p2, float p3, float alpha)
	{
		const float p01 = lerp(p0, p1, alpha);
		const float p12 = lerp(p1, p2, alpha);
		const float p23 = lerp(p2, p3, alpha);
		const float p012 = lerp(p01, p12, alpha);
		const float p123 = lerp(p12, p23, alpha);
		const float p0123 = lerp(p012, p123, alpha);

		return p0123;
	}

	//---------------------------------------------------------------------------------------------
	//! Reference version
	//---------------------------------------------------------------------------------------------
	inline float EvalCurve(const Curve& curve, float time)
	{
		float interpolated_value = curve.defaultValue;
        const int32_t numKeyFrames = curve.keyFrames.Num();

		if (numKeyFrames < 1)
		{
			return curve.defaultValue;
		}
		else if (numKeyFrames < 2 || (time <= curve.keyFrames[0].time))
		{
			// Use the first value
			interpolated_value = curve.keyFrames[0].value;
		}
		else if (time < curve.keyFrames[numKeyFrames - 1].time)
		{
			int32_t first = 1;
			int32_t last = numKeyFrames - 1;
			int32_t count = last - first;

			while (count > 0)
			{
				int32_t step = count / 2;
				int32_t middle = first + step;

				if (time >= curve.keyFrames[middle].time)
				{
					first = middle + 1;
					count -= step + 1;
				}
				else
				{
					count = step;
				}
			}

			int32_t interpolated_node = first;
			const float difference = curve.keyFrames[interpolated_node].time - curve.keyFrames[interpolated_node - 1].time;

            if (difference > 0.f && curve.keyFrames[interpolated_node - 1].interp_mode != uint8_t(CurveKeyFrame::InterpMode::Constant))
			{
				const float p0 = curve.keyFrames[interpolated_node - 1].value;
				const float p3 = curve.keyFrames[interpolated_node].value;
				const float alpha = (time - curve.keyFrames[interpolated_node - 1].time) / difference;

                if (curve.keyFrames[interpolated_node - 1].interp_mode == uint32_t(CurveKeyFrame::InterpMode::Linear))
				{
					interpolated_value = lerp(p0, p3, alpha);
				}
				else
				{
					const float aux = 1.0f / 3.0f;
					const float p1 = p0 + (curve.keyFrames[interpolated_node - 1].out_tangent * difference * aux);
					const float p2 = p3 - (curve.keyFrames[interpolated_node].in_tangent * difference * aux);

					interpolated_value = BezierInterpolation(p0, p1, p2, p3, alpha);
				}
			}
			else
			{
				interpolated_value = curve.keyFrames[interpolated_node - 1].value;
			}
		}
		else
		{
			// Use the last value
			interpolated_value = curve.keyFrames[numKeyFrames - 1].value;
		}

		return interpolated_value;
	}
}
