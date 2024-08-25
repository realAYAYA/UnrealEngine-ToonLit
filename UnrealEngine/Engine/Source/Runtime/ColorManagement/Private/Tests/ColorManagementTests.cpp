// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "TransferFunctions.h"
#include "ColorSpace.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"
#include "Tests/TestHarnessAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealColorManagementTest, Log, All);

TEST_CASE_NAMED(FTransferFunctionsTest, "System::ColorManagement::TransferFunctions", "[EditorContext][EngineFilter]")
{
	using namespace UE::Color;

	const float TestIncrement = 0.05f;

	// Verify that all transfer functions correctly inverse each other.

	for (uint8 EnumValue = static_cast<uint8>(EEncoding::Linear); EnumValue < static_cast<uint8>(EEncoding::Max); EnumValue++)
	{
		EEncoding EncodingType = static_cast<EEncoding>(EnumValue);

		for (float TestValue = 0.0f; TestValue <= 1.0f; TestValue += TestIncrement)
		{
			float Encoded = UE::Color::Encode(EncodingType, TestValue);
			float Decoded = UE::Color::Decode(EncodingType, Encoded);
			CHECK_MESSAGE(TEXT("Transfer function encode followed by decode must match identity"), FMath::IsNearlyEqual(Decoded, TestValue, KINDA_SMALL_NUMBER));
		}
	}
}

/**
 * Tests if two double matrices (4x4 xyzw) are equal within an optional tolerance
 *
 * @param Mat0 First Matrix
 * @param Mat1 Second Matrix
 * @param Tolerance Error per item allowed for the comparison
 *
 * @return true if equal within tolerance
 */
static bool TestMatricesDoubleEqual(FMatrix44d& Mat0, FMatrix44d& Mat1, double Tolerance = 0.0)
{
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			double Diff = Mat0.M[Row][Column] - Mat1.M[Row][Column];
			if (FMath::Abs(Diff) > Tolerance)
			{
				UE_LOG(LogUnrealColorManagementTest, Log, TEXT("Bad(%.8f) at [%i, %i]"), Diff, Row, Column);
				return false;
			}
		}
	}
	return true;
}

static void TestRgbToXyzConversionMatrices()
{
	// Note: test matrices generated using the python (colour-science) colour library.

	using namespace UE::Color;

	double Tolerance = 0.000001;

	FMatrix44d Mat0 = FColorSpace(EColorSpace::sRGB).GetRgbToXYZ();
	FMatrix44d Mat1 = FMatrix44d(
		{0.412390799266, 0.357584339384, 0.180480788402, 0.0},
		{0.212639005872, 0.715168678768, 0.0721923153607, 0.0},
		{0.0193308187156, 0.119194779795, 0.95053215225, 0.0},
		{0,0,0,1}
	).GetTransposed();
	CHECK_MESSAGE(TEXT("Rec709 RGB2XYZ Matrix Equality"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

	Mat0 = FColorSpace(EColorSpace::Rec2020).GetRgbToXYZ();
	Mat1 = FMatrix44d(
		{0.636958048301, 0.144616903586, 0.168880975164, 0.0},
		{0.262700212011, 0.677998071519, 0.0593017164699, 0.0},
		{4.99410657447e-17, 0.0280726930491, 1.06098505771, 0.0},
		{0,0,0,1}
	).GetTransposed();
	CHECK_MESSAGE(TEXT("Rec2020 RGB2XYZ Matrix Equality"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

	Mat0 = FColorSpace(EColorSpace::ACESAP0).GetRgbToXYZ();
	Mat1 = FMatrix44d(
		{ 0.9525523959, 0.0, 9.36786e-05, 0.0 },
		{ 0.3439664498, 0.7281660966, -0.0721325464, 0.0 },
		{ 0.0, 0.0, 1.0088251844, 0.0 },
		{ 0,0,0,1 }
	).GetTransposed();
	CHECK_MESSAGE(TEXT("AP0 RGB2XYZ Matrix Equality"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

	Mat0 = FColorSpace(EColorSpace::ACESAP1).GetRgbToXYZ();
	Mat1 = FMatrix44d(
		{ 0.662454181109, 0.134004206456, 0.156187687005, 0.0 },
		{ 0.272228716781, 0.674081765811, 0.0536895174079, 0.0 },
		{ -0.00557464949039, 0.00406073352898, 1.01033910031, 0.0 },
		{ 0,0,0,1 }
	).GetTransposed();
	CHECK_MESSAGE(TEXT("AP1 RGB2XYZ Matrix Equality"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

	Mat0 = FColorSpace(EColorSpace::P3DCI).GetRgbToXYZ();
	Mat1 = FMatrix44d(
		{0.445169815565, 0.277134409207, 0.172282669816, 0.0},
		{0.209491677913, 0.721595254161, 0.0689130679262, 0.0},
		{-3.63410131697e-17, 0.047060560054, 0.907355394362, 0.0},
		{0,0,0,1}
	).GetTransposed();
	CHECK_MESSAGE(TEXT("P3DCI RGB2XYZ Matrix Equality"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

	Mat0 = FColorSpace(EColorSpace::P3D65).GetRgbToXYZ();
	Mat1 = FMatrix44d(
		{0.486570948648, 0.265667693169, 0.198217285234, 0.0},
		{0.22897456407, 0.691738521837, 0.0792869140937, 0.0},
		{-3.97207551693e-17, 0.0451133818589, 1.0439443689, 0.0},
		{0,0,0,1}
	).GetTransposed();
	CHECK_MESSAGE(TEXT("P3D65 RGB2XYZ Matrix Equality"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));
}


static void TestColorSpaceTransforms(UE::Color::EChromaticAdaptationMethod Method)
{
	// Note: test matrices generated using the python (colour-science) colour library.

	using namespace UE::Color;

	double Tolerance = UE_SMALL_NUMBER;

	const FColorSpace Src = FColorSpace(EColorSpace::ACESAP1);
	FMatrix44d Mat0, Mat1;

	if (Method == UE::Color::EChromaticAdaptationMethod::None)
	{
		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::sRGB), Method);
		Mat1 = FMatrix44d(
			{ 1.73125381945, -0.604043087283, -0.0801077089571, 0.0 },
			{ -0.131618928589, 1.13484150569, -0.00867943255179, 0.0 },
			{ -0.0245682525938, -0.125750404281, 1.06563695775, 0.0 },
			{0,0,0,1}
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 without chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::Rec2020), Method);
		Mat1 = FMatrix44d(
			{ 1.04179138412, -0.0107415627227, -0.00696187506631, 0.0 },
			{ -0.00168312771738, 1.00036605073, -0.0014082109903, 0.0 },
			{ -0.005209686529, -0.0226414456785, 0.95230241486, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 without chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::ACESAP0), Method);
		Mat1 = FMatrix44d(
			{ 0.695452241359, 0.140678696471, 0.163869062214, 0.0 },
			{ 0.0447945633525, 0.859671118443, 0.0955343182103, 0.0 },
			{ -0.00552588255811, 0.00402521030598, 1.00150067225, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 without chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::ACESAP1), Method);
		Mat1 = FMatrix44d::Identity;
		CHECK_MESSAGE(TEXT("AP1->Rec709 without chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::P3DCI), Method);
		Mat1 = FMatrix44d(
			{ 1.53077277413, -0.322790385146, -0.0736971869439, 0.0 },
			{ -0.0668950445367, 1.03255367118, -0.0105932139741, 0.0 },
			{ -0.0026742897488, -0.0490786970568, 1.11404817691, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 without chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::P3D65), Method);
		Mat1 = FMatrix44d(
			{ 1.40052305923, -0.295324940013, -0.067426473386, 0.0 },
			{ -0.069782360156, 1.07712062473, -0.0110504369624, 0.0 },
			{ -0.0023243874884, -0.0426572735573, 0.968286867584, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 without chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));
	}
	else if (Method == UE::Color::EChromaticAdaptationMethod::Bradford)
	{
		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::sRGB), Method);
		Mat1 = FMatrix44d(
			{ 1.70505099266, -0.621792120657, -0.083258872001, 0.0 },
			{ -0.130256417507, 1.14080473658, -0.0105483190684, 0.0 },
			{ -0.0240033568046, -0.128968976065, 1.15297233287, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 with Bradford chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::Rec2020), Method);
		Mat1 = FMatrix44d(
			{ 1.02582474767, -0.0200531908382, -0.0057715568278, 0.0 },
			{ -0.00223436951998, 1.00458650189, -0.0023521323685, 0.0 },
			{ -0.00501335146809, -0.0252900718108, 1.03030342328, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 with Bradford chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::ACESAP0), Method);
		Mat1 = FMatrix44d(
			{ 0.695452241359, 0.140678696471, 0.163869062214, 0.0 },
			{ 0.0447945633525, 0.859671118443, 0.0955343182103, 0.0 },
			{ -0.00552588255811, 0.00402521030598, 1.00150067225, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 with Bradford chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::ACESAP1), Method);
		Mat1 = FMatrix44d::Identity;
		CHECK_MESSAGE(TEXT("AP1->Rec709 with Bradford chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::P3DCI), Method);
		Mat1 = FMatrix44d(
			{ 1.46412016696, -0.393327041647, -0.0707931253151, 0.0 },
			{ -0.0664765138416, 1.07529152526, -0.00881501141699, 0.0 },
			{ -0.00255286167095, -0.0470296027287, 1.0495824644, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 with Bradford chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));

		Mat0 = FColorSpaceTransform(Src, FColorSpace(EColorSpace::P3D65), Method);
		Mat1 = FMatrix44d(
			{ 1.37921412825, -0.308864144674, -0.0703499835796, 0.0 },
			{ -0.0693348583814, 1.082296746, -0.012961887621, 0.0 },
			{ -0.00215900951357, -0.0454593248373, 1.04761833435, 0.0 },
			{ 0,0,0,1 }
		).GetTransposed();
		CHECK_MESSAGE(TEXT("AP1->Rec709 with Bradford chromatic adaptation"), TestMatricesDoubleEqual(Mat0, Mat1, Tolerance));
	}
}

static void TestAppliedColorTransforms()
{
	using namespace UE::Color;

	// Intentionally misalign the start of FLinearColor member,
	// as a test against 16-byte (128bit) alignment when using 4 float SIMD instructions.
	// 
	// Given that we now load/store from unaligned memory, the above alignment requirement is lifted.
	struct FAlignmentTest
	{
		float Nudge = 0.0f;
		FLinearColor SrcColor = FLinearColor(1.0f, 0.5f, 0.0f);
	};

	FColorSpaceTransform Transform = FColorSpaceTransform(FColorSpace(EColorSpace::sRGB), FColorSpace(EColorSpace::ACESAP1), EChromaticAdaptationMethod::Bradford);

	TUniquePtr<FAlignmentTest> AlignmentTest = MakeUnique<FAlignmentTest>();
	FLinearColor ExpectedResult = FLinearColor(0.78285898f, 0.52837066f, 0.07540048f);
	FLinearColor Result = Transform.Apply(AlignmentTest->SrcColor);

	CHECK_MESSAGE(TEXT("FLinearColor sRGB->AP1 color transform"), Result.Equals(ExpectedResult, UE_SMALL_NUMBER));
}

static void TestLuminance()
{
	using namespace UE::Color;

	// Note: test factors from the python (colour-science) colour library.

	FLinearColor LuminanceFactors = FColorSpace(EColorSpace::sRGB).GetLuminanceFactors();
	CHECK_MESSAGE(TEXT("sRGB luminance factors equality test"), LuminanceFactors.Equals(FLinearColor(0.212639005872, 0.715168678768, 0.0721923153607), UE_SMALL_NUMBER));

	LuminanceFactors = FColorSpace(EColorSpace::ACESAP1).GetLuminanceFactors();
	CHECK_MESSAGE(TEXT("ACESAP1 luminance factors equality test"), LuminanceFactors.Equals(FLinearColor(0.272228716781, 0.674081765811, 0.0536895174079), UE_SMALL_NUMBER));

	LuminanceFactors = FColorSpace(EColorSpace::Rec2020).GetLuminanceFactors();
	CHECK_MESSAGE(TEXT("Rec2020 luminance factors equality test"), LuminanceFactors.Equals(FLinearColor(0.262700212011, 0.677998071519, 0.0593017164699), UE_SMALL_NUMBER));
}

TEST_CASE_NAMED(FColorSpaceTest, "System::ColorManagement::ColorSpace", "[EditorContext][EngineFilter]")
{
	using namespace UE::Color;

	FColorSpace CS = FColorSpace(EColorSpace::sRGB);
	FMatrix44d Mat0 = FColorSpaceTransform(CS, CS, EChromaticAdaptationMethod::None);
	FMatrix44d Mat1 = FMatrix44d::Identity;
	CHECK_MESSAGE(TEXT("Identity color space conversion to itself should match identity"), TestMatricesDoubleEqual(Mat0, Mat1, 0.00000001));

	SECTION("RgbToXyzConversionMatrices")
	TestRgbToXyzConversionMatrices();

	SECTION("ColorSpaceTransforms")
	TestColorSpaceTransforms(EChromaticAdaptationMethod::None);

	SECTION("ColorSpaceTransforms_Bradford")
	TestColorSpaceTransforms(EChromaticAdaptationMethod::Bradford);

	SECTION("AppliedColorTransforms")
	TestAppliedColorTransforms();

	SECTION("Luminance")
	TestLuminance();
}

#endif //WITH_TESTS
