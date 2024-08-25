// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditorUXSettings.h"
#include "Math/Color.h"

#define LOCTEXT_NAMESPACE "UVEditorUXSettings"

const float FUVEditorUXSettings::UVMeshScalingFactor(1000.0);
const float FUVEditorUXSettings::CameraFarPlaneWorldZ(-10.0);
const float FUVEditorUXSettings::CameraNearPlaneProportionZ(0.8); // Top layer, equivalent to depth bias 80

// 2D Viewport Depth Offsets (Organized by "layers" from the camera's perspective, descending order
// Note: While these are floating point values, they represent percentages and should be separated
// by at least integer amounts, as they serve double duty in certain cases for translucent primitive
// sorting order.
const float FUVEditorUXSettings::ToolLockedPathDepthBias(8.0);
const float FUVEditorUXSettings::ToolExtendPathDepthBias(8.0);
const float FUVEditorUXSettings::SewLineDepthOffset(7.0f);
const float FUVEditorUXSettings::SelectionHoverWireframeDepthBias(6);
const float FUVEditorUXSettings::SelectionHoverTriangleDepthBias(5);
const float FUVEditorUXSettings::SelectionWireframeDepthBias(4.0);
const float FUVEditorUXSettings::SelectionTriangleDepthBias(3.0);
const float FUVEditorUXSettings::WireframeDepthOffset(2.0);
const float FUVEditorUXSettings::UnwrapTriangleDepthOffset(1.0);

const float FUVEditorUXSettings::LivePreviewExistingSeamDepthBias(1.0);

// Note: that this offset can only be applied when we use our own background material
// for a user-supplied texture, and we can't use it for a user-provided material.
// So for consistency this should stay at zero.

const float FUVEditorUXSettings::BackgroundQuadDepthOffset(0.0); // Bottom layer

// 3D Viewport Depth Offsets
const float FUVEditorUXSettings::LivePreviewHighlightDepthOffset(0.5);

// Opacities
const float FUVEditorUXSettings::UnwrapTriangleOpacity(1.0);
const float FUVEditorUXSettings::UnwrapTriangleOpacityWithBackground(0.25);
const float FUVEditorUXSettings::SelectionTriangleOpacity(1.0f);
const float FUVEditorUXSettings::SelectionHoverTriangleOpacity(1.0f);

// Per Asset Shifts
const float FUVEditorUXSettings::UnwrapBoundaryHueShift(30);
const float FUVEditorUXSettings::UnwrapBoundarySaturation(0.50);
const float FUVEditorUXSettings::UnwrapBoundaryValue(0.50);

// Colors
const FColor FUVEditorUXSettings::UnwrapTriangleFillColor(FColor::FromHex("#696871"));
const FColor FUVEditorUXSettings::UnwrapTriangleWireframeColor(FColor::FromHex("#989898"));
const FColor FUVEditorUXSettings::SelectionTriangleFillColor(FColor::FromHex("#8C7A52"));
const FColor FUVEditorUXSettings::SelectionTriangleWireframeColor(FColor::FromHex("#DDA209"));
const FColor FUVEditorUXSettings::SelectionHoverTriangleFillColor(FColor::FromHex("#4E719B"));
const FColor FUVEditorUXSettings::SelectionHoverTriangleWireframeColor(FColor::FromHex("#0E86FF"));
const FColor FUVEditorUXSettings::SewSideLeftColor(FColor::Red);
const FColor FUVEditorUXSettings::SewSideRightColor(FColor::Green);

const FColor FUVEditorUXSettings::ToolLockedCutPathColor(FColor::Green);
const FColor FUVEditorUXSettings::ToolExtendCutPathColor(FColor::Green);
const FColor FUVEditorUXSettings::ToolLockedJoinPathColor(FColor::Turquoise);
const FColor FUVEditorUXSettings::ToolExtendJoinPathColor(FColor::Turquoise);
const FColor FUVEditorUXSettings::ToolCompletionPathColor(FColor::Orange);

const FColor FUVEditorUXSettings::LivePreviewExistingSeamColor(FColor::Green);

const FColor FUVEditorUXSettings::XAxisColor(FColor::Red);
const FColor FUVEditorUXSettings::YAxisColor(FColor::Green);
const FColor FUVEditorUXSettings::GridMajorColor(FColor::FromHex("#888888"));
const FColor FUVEditorUXSettings::GridMinorColor(FColor::FromHex("#777777"));
const FColor FUVEditorUXSettings::RulerXColor(FColor::FromHex("#888888"));
const FColor FUVEditorUXSettings::RulerYColor(FColor::FromHex("#888888"));
const FColor FUVEditorUXSettings::PivotLineColor(FColor::Cyan);

// Thicknesses
const float FUVEditorUXSettings::LivePreviewHighlightThickness(2.0);
const float FUVEditorUXSettings::LivePreviewHighlightPointSize(4);
const float FUVEditorUXSettings::LivePreviewExistingSeamThickness(2.0);
const float FUVEditorUXSettings::SelectionLineThickness(1.5);
const float FUVEditorUXSettings::ToolLockedPathThickness(3.0f);
const float FUVEditorUXSettings::ToolExtendPathThickness(3.0f);
const float FUVEditorUXSettings::SelectionPointThickness(6);
const float FUVEditorUXSettings::SewLineHighlightThickness(3.0f);
const float FUVEditorUXSettings::AxisThickness(2.0);
const float FUVEditorUXSettings::GridMajorThickness(1.0);
const float FUVEditorUXSettings::WireframeThickness(2.0);
const float FUVEditorUXSettings::BoundaryEdgeThickness(2.0);
const float FUVEditorUXSettings::ToolPointSize(6);
const float FUVEditorUXSettings::PivotLineThickness(1.5);

// Grid
const int32 FUVEditorUXSettings::GridSubdivisionsPerLevel(4);
const int32 FUVEditorUXSettings::GridLevels(3);
const int32 FUVEditorUXSettings::RulerSubdivisionLevel(1);

// Pivot Visuals
const int32 FUVEditorUXSettings::PivotCircleNumSides(32);
const float FUVEditorUXSettings::PivotCircleRadius(10.0);

// CVARs

TAutoConsoleVariable<int32> FUVEditorUXSettings::CVarEnablePrototypeUDIMSupport(
	TEXT("modeling.UVEditor.UDIMSupport"),
	1,
	TEXT("Enable experimental UDIM support in the UVEditor"));


FLinearColor FUVEditorUXSettings::GetTriangleColorByTargetIndex(int32 TargetIndex)
{
	double GoldenAngle = 137.50776405;

	FLinearColor BaseColorHSV = FLinearColor::FromSRGBColor(UnwrapTriangleFillColor).LinearRGBToHSV();
	BaseColorHSV.R = static_cast<float>(FMath::Fmod(BaseColorHSV.R + (GoldenAngle / 2.0 * TargetIndex), 360));

	return BaseColorHSV.HSVToLinearRGB();
}

FLinearColor FUVEditorUXSettings::GetWireframeColorByTargetIndex(int32 TargetIndex)
{
	return FLinearColor::FromSRGBColor(UnwrapTriangleWireframeColor);;
}

FLinearColor FUVEditorUXSettings::GetBoundaryColorByTargetIndex(int32 TargetIndex)
{
	FLinearColor BaseColorHSV = GetTriangleColorByTargetIndex(TargetIndex).LinearRGBToHSV();
	FLinearColor BoundaryColorHSV = BaseColorHSV;
	BoundaryColorHSV.R = FMath::Fmod((BoundaryColorHSV.R + UnwrapBoundaryHueShift), 360);
	BoundaryColorHSV.G = UnwrapBoundarySaturation;
	BoundaryColorHSV.B = UnwrapBoundaryValue;
	return BoundaryColorHSV.HSVToLinearRGB();
}

FColor FUVEditorUXSettings::MakeCividisColorFromScalar(float Scalar)
{
	// Color map sourced from:
	// "Optimizing colormaps with consideration for color vision deficiency to enable accurate interpretation of scientific data"
	// https://doi.org/10.1371/journal.pone.0199239

	float RComponents[256] = { 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000,
		0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000,
		0.0000, 0.0000, 0.0055, 0.0236, 0.0416, 0.0576, 0.0710, 0.0827, 0.0932, 0.1030, 0.1120, 0.1204, 0.1283, 0.1359, 0.1431, 0.1500,
		0.1566, 0.1630, 0.1692, 0.1752, 0.1811, 0.1868, 0.1923, 0.1977, 0.2030, 0.2082, 0.2133, 0.2183, 0.2232, 0.2281, 0.2328, 0.2375,
		0.2421, 0.2466, 0.2511, 0.2556, 0.2599, 0.2643, 0.2686, 0.2728, 0.2770, 0.2811, 0.2853, 0.2894, 0.2934, 0.2974, 0.3014, 0.3054,
		0.3093, 0.3132, 0.3170, 0.3209, 0.3247, 0.3285, 0.3323, 0.3361, 0.3398, 0.3435, 0.3472, 0.3509, 0.3546, 0.3582, 0.3619, 0.3655,
		0.3691, 0.3727, 0.3763, 0.3798, 0.3834, 0.3869, 0.3905, 0.3940, 0.3975, 0.4010, 0.4045, 0.4080, 0.4114, 0.4149, 0.4183, 0.4218,
		0.4252, 0.4286, 0.4320, 0.4354, 0.4388, 0.4422, 0.4456, 0.4489, 0.4523, 0.4556, 0.4589, 0.4622, 0.4656, 0.4689, 0.4722, 0.4756,
		0.4790, 0.4825, 0.4861, 0.4897, 0.4934, 0.4971, 0.5008, 0.5045, 0.5083, 0.5121, 0.5158, 0.5196, 0.5234, 0.5272, 0.5310, 0.5349,
		0.5387, 0.5425, 0.5464, 0.5502, 0.5541, 0.5579, 0.5618, 0.5657, 0.5696, 0.5735, 0.5774, 0.5813, 0.5852, 0.5892, 0.5931, 0.5970,
		0.6010, 0.6050, 0.6089, 0.6129, 0.6168, 0.6208, 0.6248, 0.6288, 0.6328, 0.6368, 0.6408, 0.6449, 0.6489, 0.6529, 0.6570, 0.6610,
		0.6651, 0.6691, 0.6732, 0.6773, 0.6813, 0.6854, 0.6895, 0.6936, 0.6977, 0.7018, 0.7060, 0.7101, 0.7142, 0.7184, 0.7225, 0.7267,
		0.7308, 0.7350, 0.7392, 0.7434, 0.7476, 0.7518, 0.7560, 0.7602, 0.7644, 0.7686, 0.7729, 0.7771, 0.7814, 0.7856, 0.7899, 0.7942,
		0.7985, 0.8027, 0.8070, 0.8114, 0.8157, 0.8200, 0.8243, 0.8287, 0.8330, 0.8374, 0.8417, 0.8461, 0.8505, 0.8548, 0.8592, 0.8636,
		0.8681, 0.8725, 0.8769, 0.8813, 0.8858, 0.8902, 0.8947, 0.8992, 0.9037, 0.9082, 0.9127, 0.9172, 0.9217, 0.9262, 0.9308, 0.9353,
		0.9399, 0.9444, 0.9490, 0.9536, 0.9582, 0.9628, 0.9674, 0.9721, 0.9767, 0.9814, 0.9860, 0.9907, 0.9954, 1.0000, 1.0000, 1.0000,
		1.0000, 1.0000, 1.0000};
	float GComponents[256] = { 0.1262, 0.1292, 0.1321, 0.1350, 0.1379, 0.1408, 0.1437, 0.1465, 0.1492, 0.1519, 0.1546, 0.1574, 0.1601,
		0.1629, 0.1657, 0.1685, 0.1714, 0.1743, 0.1773, 0.1798, 0.1817, 0.1834, 0.1852, 0.1872, 0.1901, 0.1930, 0.1958, 0.1987, 0.2015,
		0.2044, 0.2073, 0.2101, 0.2130, 0.2158, 0.2187, 0.2215, 0.2244, 0.2272, 0.2300, 0.2329, 0.2357, 0.2385, 0.2414, 0.2442, 0.2470,
		0.2498, 0.2526, 0.2555, 0.2583, 0.2611, 0.2639, 0.2667, 0.2695, 0.2723, 0.2751, 0.2780, 0.2808, 0.2836, 0.2864, 0.2892, 0.2920,
		0.2948, 0.2976, 0.3004, 0.3032, 0.3060, 0.3088, 0.3116, 0.3144, 0.3172, 0.3200, 0.3228, 0.3256, 0.3284, 0.3312, 0.3340, 0.3368,
		0.3396, 0.3424, 0.3453, 0.3481, 0.3509, 0.3537, 0.3565, 0.3593, 0.3622, 0.3650, 0.3678, 0.3706, 0.3734, 0.3763, 0.3791, 0.3819,
		0.3848, 0.3876, 0.3904, 0.3933, 0.3961, 0.3990, 0.4018, 0.4047, 0.4075, 0.4104, 0.4132, 0.4161, 0.4189, 0.4218, 0.4247, 0.4275,
		0.4304, 0.4333, 0.4362, 0.4390, 0.4419, 0.4448, 0.4477, 0.4506, 0.4535, 0.4564, 0.4593, 0.4622, 0.4651, 0.4680, 0.4709, 0.4738,
		0.4767, 0.4797, 0.4826, 0.4856, 0.4886, 0.4915, 0.4945, 0.4975, 0.5005, 0.5035, 0.5065, 0.5095, 0.5125, 0.5155, 0.5186, 0.5216,
		0.5246, 0.5277, 0.5307, 0.5338, 0.5368, 0.5399, 0.5430, 0.5461, 0.5491, 0.5522, 0.5553, 0.5584, 0.5615, 0.5646, 0.5678, 0.5709,
		0.5740, 0.5772, 0.5803, 0.5835, 0.5866, 0.5898, 0.5929, 0.5961, 0.5993, 0.6025, 0.6057, 0.6089, 0.6121, 0.6153, 0.6185, 0.6217,
		0.6250, 0.6282, 0.6315, 0.6347, 0.6380, 0.6412, 0.6445, 0.6478, 0.6511, 0.6544, 0.6577, 0.6610, 0.6643, 0.6676, 0.6710, 0.6743,
		0.6776, 0.6810, 0.6844, 0.6877, 0.6911, 0.6945, 0.6979, 0.7013, 0.7047, 0.7081, 0.7115, 0.7150, 0.7184, 0.7218, 0.7253, 0.7288,
		0.7322, 0.7357, 0.7392, 0.7427, 0.7462, 0.7497, 0.7532, 0.7568, 0.7603, 0.7639, 0.7674, 0.7710, 0.7745, 0.7781, 0.7817, 0.7853,
		0.7889, 0.7926, 0.7962, 0.7998, 0.8035, 0.8071, 0.8108, 0.8145, 0.8182, 0.8219, 0.8256, 0.8293, 0.8330, 0.8367, 0.8405, 0.8442,
		0.8480, 0.8518, 0.8556, 0.8593, 0.8632, 0.8670, 0.8708, 0.8746, 0.8785, 0.8823, 0.8862, 0.8901, 0.8940, 0.8979, 0.9018, 0.9057,
		0.9094, 0.9131, 0.9169};
	float BComponents[256] = { 0.3015, 0.3077, 0.3142, 0.3205, 0.3269, 0.3334, 0.3400, 0.3467, 0.3537, 0.3606, 0.3676, 0.3746, 0.3817,
		0.3888, 0.3960, 0.4031, 0.4102, 0.4172, 0.4241, 0.4307, 0.4347, 0.4363, 0.4368, 0.4368, 0.4365, 0.4361, 0.4356, 0.4349, 0.4343,
		0.4336, 0.4329, 0.4322, 0.4314, 0.4308, 0.4301, 0.4293, 0.4287, 0.4280, 0.4274, 0.4268, 0.4262, 0.4256, 0.4251, 0.4245, 0.4241,
		0.4236, 0.4232, 0.4228, 0.4224, 0.4220, 0.4217, 0.4214, 0.4212, 0.4209, 0.4207, 0.4205, 0.4204, 0.4203, 0.4202, 0.4201, 0.4200,
		0.4200, 0.4200, 0.4201, 0.4201, 0.4202, 0.4203, 0.4205, 0.4206, 0.4208, 0.4210, 0.4212, 0.4215, 0.4218, 0.4221, 0.4224, 0.4227,
		0.4231, 0.4236, 0.4240, 0.4244, 0.4249, 0.4254, 0.4259, 0.4264, 0.4270, 0.4276, 0.4282, 0.4288, 0.4294, 0.4302, 0.4308, 0.4316,
		0.4322, 0.4331, 0.4338, 0.4346, 0.4355, 0.4364, 0.4372, 0.4381, 0.4390, 0.4400, 0.4409, 0.4419, 0.4430, 0.4440, 0.4450, 0.4462,
		0.4473, 0.4485, 0.4496, 0.4508, 0.4521, 0.4534, 0.4547, 0.4561, 0.4575, 0.4589, 0.4604, 0.4620, 0.4635, 0.4650, 0.4665, 0.4679,
		0.4691, 0.4701, 0.4707, 0.4714, 0.4719, 0.4723, 0.4727, 0.4730, 0.4732, 0.4734, 0.4736, 0.4737, 0.4738, 0.4739, 0.4739, 0.4738,
		0.4739, 0.4738, 0.4736, 0.4735, 0.4733, 0.4732, 0.4729, 0.4727, 0.4723, 0.4720, 0.4717, 0.4714, 0.4709, 0.4705, 0.4701, 0.4696,
		0.4691, 0.4685, 0.4680, 0.4673, 0.4668, 0.4662, 0.4655, 0.4649, 0.4641, 0.4632, 0.4625, 0.4617, 0.4609, 0.4600, 0.4591, 0.4583,
		0.4573, 0.4562, 0.4553, 0.4543, 0.4532, 0.4521, 0.4511, 0.4499, 0.4487, 0.4475, 0.4463, 0.4450, 0.4437, 0.4424, 0.4409, 0.4396,
		0.4382, 0.4368, 0.4352, 0.4338, 0.4322, 0.4307, 0.4290, 0.4273, 0.4258, 0.4241, 0.4223, 0.4205, 0.4188, 0.4168, 0.4150, 0.4129, 
		0.4111, 0.4090, 0.4070, 0.4049, 0.4028, 0.4007, 0.3984, 0.3961, 0.3938, 0.3915, 0.3892, 0.3869, 0.3843, 0.3818, 0.3793, 0.3766,
		0.3739, 0.3712, 0.3684, 0.3657, 0.3627, 0.3599, 0.3569, 0.3538, 0.3507, 0.3474, 0.3442, 0.3409, 0.3374, 0.3340, 0.3306, 0.3268,
		0.3232, 0.3195, 0.3155, 0.3116, 0.3076, 0.3034, 0.2990, 0.2947, 0.2901, 0.2856, 0.2807, 0.2759, 0.2708, 0.2655, 0.2600, 0.2593,
		0.2634, 0.2680, 0.2731};

	const float RedSclr = FMath::Clamp<float>(Scalar, 0.f, 1.f);
	const float GreenSclr = FMath::Clamp<float>(Scalar, 0.f, 1.f);
	const float BlueSclr = FMath::Clamp<float>(Scalar, 0.f, 1.f);
	const uint8 R = (uint8)FMath::TruncToInt(254 * RComponents[(uint8)FMath::TruncToInt(254 * RedSclr)]);
	const uint8 G = (uint8)FMath::TruncToInt(254 * GComponents[(uint8)FMath::TruncToInt(254 * GreenSclr)]);
	const uint8 B = (uint8)FMath::TruncToInt(254 * BComponents[(uint8)FMath::TruncToInt(254 * BlueSclr)]);
	return FColor(R, G, B);
}

FColor FUVEditorUXSettings::MakeTurboColorFromScalar(float Scalar)
{
	// Color map sourced from:
	// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html

	float TurboSRGB[256][3] = { {0.18995,0.07176,0.23217},{0.19483,0.08339,0.26149},{0.19956,0.09498,0.29024},{0.20415,0.10652,0.31844},
		{0.20860,0.11802,0.34607},{0.21291,0.12947,0.37314},{0.21708,0.14087,0.39964},{0.22111,0.15223,0.42558},{0.22500,0.16354,0.45096},
		{0.22875,0.17481,0.47578},{0.23236,0.18603,0.50004},{0.23582,0.19720,0.52373},{0.23915,0.20833,0.54686},{0.24234,0.21941,0.56942},
		{0.24539,0.23044,0.59142},{0.24830,0.24143,0.61286},{0.25107,0.25237,0.63374},{0.25369,0.26327,0.65406},{0.25618,0.27412,0.67381},
		{0.25853,0.28492,0.69300},{0.26074,0.29568,0.71162},{0.26280,0.30639,0.72968},{0.26473,0.31706,0.74718},{0.26652,0.32768,0.76412},
		{0.26816,0.33825,0.78050},{0.26967,0.34878,0.79631},{0.27103,0.35926,0.81156},{0.27226,0.36970,0.82624},{0.27334,0.38008,0.84037},
		{0.27429,0.39043,0.85393},{0.27509,0.40072,0.86692},{0.27576,0.41097,0.87936},{0.27628,0.42118,0.89123},{0.27667,0.43134,0.90254},
		{0.27691,0.44145,0.91328},{0.27701,0.45152,0.92347},{0.27698,0.46153,0.93309},{0.27680,0.47151,0.94214},{0.27648,0.48144,0.95064},
		{0.27603,0.49132,0.95857},{0.27543,0.50115,0.96594},{0.27469,0.51094,0.97275},{0.27381,0.52069,0.97899},{0.27273,0.53040,0.98461},
		{0.27106,0.54015,0.98930},{0.26878,0.54995,0.99303},{0.26592,0.55979,0.99583},{0.26252,0.56967,0.99773},{0.25862,0.57958,0.99876},
		{0.25425,0.58950,0.99896},{0.24946,0.59943,0.99835},{0.24427,0.60937,0.99697},{0.23874,0.61931,0.99485},{0.23288,0.62923,0.99202},
		{0.22676,0.63913,0.98851},{0.22039,0.64901,0.98436},{0.21382,0.65886,0.97959},{0.20708,0.66866,0.97423},{0.20021,0.67842,0.96833},
		{0.19326,0.68812,0.96190},{0.18625,0.69775,0.95498},{0.17923,0.70732,0.94761},{0.17223,0.71680,0.93981},{0.16529,0.72620,0.93161},
		{0.15844,0.73551,0.92305},{0.15173,0.74472,0.91416},{0.14519,0.75381,0.90496},{0.13886,0.76279,0.89550},{0.13278,0.77165,0.88580},
		{0.12698,0.78037,0.87590},{0.12151,0.78896,0.86581},{0.11639,0.79740,0.85559},{0.11167,0.80569,0.84525},{0.10738,0.81381,0.83484},
		{0.10357,0.82177,0.82437},{0.10026,0.82955,0.81389},{0.09750,0.83714,0.80342},{0.09532,0.84455,0.79299},{0.09377,0.85175,0.78264},
		{0.09287,0.85875,0.77240},{0.09267,0.86554,0.76230},{0.09320,0.87211,0.75237},{0.09451,0.87844,0.74265},{0.09662,0.88454,0.73316},
		{0.09958,0.89040,0.72393},{0.10342,0.89600,0.71500},{0.10815,0.90142,0.70599},{0.11374,0.90673,0.69651},{0.12014,0.91193,0.68660},
		{0.12733,0.91701,0.67627},{0.13526,0.92197,0.66556},{0.14391,0.92680,0.65448},{0.15323,0.93151,0.64308},{0.16319,0.93609,0.63137},
		{0.17377,0.94053,0.61938},{0.18491,0.94484,0.60713},{0.19659,0.94901,0.59466},{0.20877,0.95304,0.58199},{0.22142,0.95692,0.56914},
		{0.23449,0.96065,0.55614},{0.24797,0.96423,0.54303},{0.26180,0.96765,0.52981},{0.27597,0.97092,0.51653},{0.29042,0.97403,0.50321},
		{0.30513,0.97697,0.48987},{0.32006,0.97974,0.47654},{0.33517,0.98234,0.46325},{0.35043,0.98477,0.45002},{0.36581,0.98702,0.43688},
		{0.38127,0.98909,0.42386},{0.39678,0.99098,0.41098},{0.41229,0.99268,0.39826},{0.42778,0.99419,0.38575},{0.44321,0.99551,0.37345},
		{0.45854,0.99663,0.36140},{0.47375,0.99755,0.34963},{0.48879,0.99828,0.33816},{0.50362,0.99879,0.32701},{0.51822,0.99910,0.31622},
		{0.53255,0.99919,0.30581},{0.54658,0.99907,0.29581},{0.56026,0.99873,0.28623},{0.57357,0.99817,0.27712},{0.58646,0.99739,0.26849},
		{0.59891,0.99638,0.26038},{0.61088,0.99514,0.25280},{0.62233,0.99366,0.24579},{0.63323,0.99195,0.23937},{0.64362,0.98999,0.23356},
		{0.65394,0.98775,0.22835},{0.66428,0.98524,0.22370},{0.67462,0.98246,0.21960},{0.68494,0.97941,0.21602},{0.69525,0.97610,0.21294},
		{0.70553,0.97255,0.21032},{0.71577,0.96875,0.20815},{0.72596,0.96470,0.20640},{0.73610,0.96043,0.20504},{0.74617,0.95593,0.20406},
		{0.75617,0.95121,0.20343},{0.76608,0.94627,0.20311},{0.77591,0.94113,0.20310},{0.78563,0.93579,0.20336},{0.79524,0.93025,0.20386},
		{0.80473,0.92452,0.20459},{0.81410,0.91861,0.20552},{0.82333,0.91253,0.20663},{0.83241,0.90627,0.20788},{0.84133,0.89986,0.20926},
		{0.85010,0.89328,0.21074},{0.85868,0.88655,0.21230},{0.86709,0.87968,0.21391},{0.87530,0.87267,0.21555},{0.88331,0.86553,0.21719},
		{0.89112,0.85826,0.21880},{0.89870,0.85087,0.22038},{0.90605,0.84337,0.22188},{0.91317,0.83576,0.22328},{0.92004,0.82806,0.22456},
		{0.92666,0.82025,0.22570},{0.93301,0.81236,0.22667},{0.93909,0.80439,0.22744},{0.94489,0.79634,0.22800},{0.95039,0.78823,0.22831},
		{0.95560,0.78005,0.22836},{0.96049,0.77181,0.22811},{0.96507,0.76352,0.22754},{0.96931,0.75519,0.22663},{0.97323,0.74682,0.22536},
		{0.97679,0.73842,0.22369},{0.98000,0.73000,0.22161},{0.98289,0.72140,0.21918},{0.98549,0.71250,0.21650},{0.98781,0.70330,0.21358},
		{0.98986,0.69382,0.21043},{0.99163,0.68408,0.20706},{0.99314,0.67408,0.20348},{0.99438,0.66386,0.19971},{0.99535,0.65341,0.19577},
		{0.99607,0.64277,0.19165},{0.99654,0.63193,0.18738},{0.99675,0.62093,0.18297},{0.99672,0.60977,0.17842},{0.99644,0.59846,0.17376},
		{0.99593,0.58703,0.16899},{0.99517,0.57549,0.16412},{0.99419,0.56386,0.15918},{0.99297,0.55214,0.15417},{0.99153,0.54036,0.14910},
		{0.98987,0.52854,0.14398},{0.98799,0.51667,0.13883},{0.98590,0.50479,0.13367},{0.98360,0.49291,0.12849},{0.98108,0.48104,0.12332},
		{0.97837,0.46920,0.11817},{0.97545,0.45740,0.11305},{0.97234,0.44565,0.10797},{0.96904,0.43399,0.10294},{0.96555,0.42241,0.09798},
		{0.96187,0.41093,0.09310},{0.95801,0.39958,0.08831},{0.95398,0.38836,0.08362},{0.94977,0.37729,0.07905},{0.94538,0.36638,0.07461},
		{0.94084,0.35566,0.07031},{0.93612,0.34513,0.06616},{0.93125,0.33482,0.06218},{0.92623,0.32473,0.05837},{0.92105,0.31489,0.05475},
		{0.91572,0.30530,0.05134},{0.91024,0.29599,0.04814},{0.90463,0.28696,0.04516},{0.89888,0.27824,0.04243},{0.89298,0.26981,0.03993},
		{0.88691,0.26152,0.03753},{0.88066,0.25334,0.03521},{0.87422,0.24526,0.03297},{0.86760,0.23730,0.03082},{0.86079,0.22945,0.02875},
		{0.85380,0.22170,0.02677},{0.84662,0.21407,0.02487},{0.83926,0.20654,0.02305},{0.83172,0.19912,0.02131},{0.82399,0.19182,0.01966},
		{0.81608,0.18462,0.01809},{0.80799,0.17753,0.01660},{0.79971,0.17055,0.01520},{0.79125,0.16368,0.01387},{0.78260,0.15693,0.01264},
		{0.77377,0.15028,0.01148},{0.76476,0.14374,0.01041},{0.75556,0.13731,0.00942},{0.74617,0.13098,0.00851},{0.73661,0.12477,0.00769},
		{0.72686,0.11867,0.00695},{0.71692,0.11268,0.00629},{0.70680,0.10680,0.00571},{0.69650,0.10102,0.00522},{0.68602,0.09536,0.00481},
		{0.67535,0.08980,0.00449},{0.66449,0.08436,0.00424},{0.65345,0.07902,0.00408},{0.64223,0.07380,0.00401},{0.63082,0.06868,0.00401},
		{0.61923,0.06367,0.00410},{0.60746,0.05878,0.00427},{0.59550,0.05399,0.00453},{0.58336,0.04931,0.00486},{0.57103,0.04474,0.00529},
		{0.55852,0.04028,0.00579},{0.54583,0.03593,0.00638},{0.53295,0.03169,0.00705},{0.51989,0.02756,0.00780},{0.50664,0.02354,0.00863},
		{0.49321,0.01963,0.00955},{0.47960,0.01583,0.01055} };

	const float ClampedScalar = FMath::Clamp<float>(Scalar, -0.5f, 0.5f);
	const uint8 RampIndex = (uint8)FMath::TruncToInt(255 * (ClampedScalar + 0.5f));
	const uint8 R = (uint8)FMath::TruncToInt(255 * TurboSRGB[RampIndex][0]);
	const uint8 G = (uint8)FMath::TruncToInt(255 * TurboSRGB[RampIndex][1]);
	const uint8 B = (uint8)FMath::TruncToInt(255 * TurboSRGB[RampIndex][2]);
	FLinearColor LinearColor = FLinearColor::FromSRGBColor(FColor(R, G, B));
	return LinearColor.ToFColor(false);
}

// The mappings here depend on the way we view the unwrap world, set up in UVEditorToolkit.cpp.
// Currently, we look at the world with Z pointing towards the camera, the positive X axis pointing right,
// and the positive Y axis pointing down. This means that we have to flip the V coordinate to map it
// to the up direction.
FVector3d FUVEditorUXSettings::ExternalUVToUnwrapWorldPosition(const FVector2f& UV)
{
	return FVector3d(UV.X * UVMeshScalingFactor, -UV.Y * UVMeshScalingFactor, 0);
}
FVector2f FUVEditorUXSettings::UnwrapWorldPositionToExternalUV(const FVector3d& VertPosition)
{
	return FVector2f(static_cast<float>(VertPosition.X) / UVMeshScalingFactor, -(static_cast<float>(VertPosition.Y) / UVMeshScalingFactor));
}

// Unreal stores its UVs with V subtracted from 1.
FVector2f FUVEditorUXSettings::ExternalUVToInternalUV(const FVector2f& UV)
{
	return FVector2f(UV.X, 1-UV.Y);
}
FVector2f FUVEditorUXSettings::InternalUVToExternalUV(const FVector2f& UV)
{
	return FVector2f(UV.X, 1-UV.Y);
}

// Should be equivalent to converting from unreal's UV to regular (external) and then to unwrap world,
// i.e. ExternalUVToUnwrapWorldPosition(InternalUVToExternalUV(UV))
FVector3d FUVEditorUXSettings::UVToVertPosition(const FVector2f& UV)
{
	return FVector3d(UV.X * UVMeshScalingFactor, (UV.Y - 1) * UVMeshScalingFactor, 0);
}

// Should be equivalent to converting from world to regular (external) UV, and then to unreal's representation,
// i.e. ExternalUVToInternalUV(UnwrapWorldPositionToExternalUV(VertPosition))
FVector2f FUVEditorUXSettings::VertPositionToUV(const FVector3d& VertPosition)
{
	return FVector2f(static_cast<float>(VertPosition.X) / UVMeshScalingFactor, 1.0 + (static_cast<float>(VertPosition.Y) / UVMeshScalingFactor));
};


float FUVEditorUXSettings::LocationSnapValue(int32 LocationSnapMenuIndex)
{
	switch (LocationSnapMenuIndex)
	{
	case 0:
		return 1.0;
	case 1:
		return 0.5;
	case 2:
		return 0.25;
	case 3:
		return 0.125;
	case 4:
		return 0.0625;
	default:
		ensure(false);
		return 0;
	}
}

int32 FUVEditorUXSettings::MaxLocationSnapValue()
{
	return 5;
}

#undef LOCTEXT_NAMESPACE 
