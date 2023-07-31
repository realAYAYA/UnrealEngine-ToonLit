// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Math/Color.h"
#include "MathUtil.h"

namespace UE
{
namespace Geometry
{


/**
 * Color constants in Linear color space.
 * For a given Color name, Color3f<T> defines templated construction of arbitrary float-vector type, 
 * and Color3b() constructs an FColor
 */
namespace LinearColors
{
	/** Apply an sRGB to Linear color transformation on a given color vector. */
	template <typename VectorType>
	void SRGBToLinear(VectorType& Color)
	{
		// Sourced from MPCDIUtils.ush
		auto SRGBToLinearFloat = [](const float Color) -> float
		{
			return (Color <= 0.04045f) ? Color / 12.92f : FMathf::Pow((Color + 0.055f) / 1.055f, 2.4f);
		};
		
		Color.X = SRGBToLinearFloat(Color.X);
		Color.Y = SRGBToLinearFloat(Color.Y);
		Color.Z = SRGBToLinearFloat(Color.Z);
	}

	/** Apply a Linear to sRGB color transformation on a given color vector. */
	template <typename VectorType>
	void LinearToSRGB(VectorType& Color)
	{
		// Sourced from MPCDIUtils.ush
		auto LinearToSRGBFloat = [](const float Color) -> float
		{
			return (Color <= 0.0031308f) ? Color * 12.92f : 1.055f * FMathf::Pow(Color, 1.0f / 2.4f) - 0.055f;
		};

		Color.X = LinearToSRGBFloat(Color.X);
		Color.Y = LinearToSRGBFloat(Color.Y);
		Color.Z = LinearToSRGBFloat(Color.Z);
	}
	
	template <typename VectorType>
	VectorType MakeColor3f(float R, float G, float B)
	{
		return VectorType(R, G, B);
	}

	template <typename VectorType>
	VectorType MakeColor4f(float R, float G, float B, float A)
	{
		return VectorType(R, G, B, A);
	}

#define ColorConstant(ColorName, R, G, B) \
	inline FLinearColor ColorName##3f() { return MakeColor3f<FLinearColor>(R, G, B); } \
	inline FColor ColorName##3b() { return MakeColor3f<FLinearColor>(R, G, B).ToFColor(false); }

	ColorConstant(Black, 0.0f, 0.0f, 0.0f);
	ColorConstant(White, 1.0f, 1.0f, 1.0f);
	ColorConstant(Gray, 0.5f, 0.5f, 0.5f);

	ColorConstant(Red, 1.0f, 0.0f, 0.0f);
	ColorConstant(Green, 0.0f, 1.0f, 0.0f);
	ColorConstant(Blue, 0.0f, 0.0f, 1.0f);
	ColorConstant(Yellow, 1.0f, 1.0f, 0.0f);
	ColorConstant(Cyan, 0.0f, 1.0f, 1.0f);
	ColorConstant(Magenta, 1.0f, 0.0f, 1.0f);

	ColorConstant(VideoBlack, 0.062745f, 0.062745f, 0.062745f);
	ColorConstant(VideoWhite, 0.9215686f, 0.9215686f, 0.9215686f);
	ColorConstant(VideoRed, 0.9215686f, 0.062745f, 0.062745f);
	ColorConstant(VideoGreen, 0.062745f, 0.9215686f, 0.062745f);
	ColorConstant(VideoBlue, 0.062745f, 0.062745f, 0.9215686f);

	// X11 Colors - https://www.w3.org/TR/css-color-3/#html4
	// python code to parse text file:
	// 
	//f1 = f.readlines()
	//	for x in f1 :
	//tokens = x.split()
	//	colors = tokens[2].split(',')
	//	r = float(colors[0]) / 255.0;
	//g = float(colors[1]) / 255.0;
	//b = float(colors[2]) / 255.0;
	//print "ColorConstant(" + tokens[0].capitalize() + ", " + str(r) + "f, " + str(g) + "f, " + str(b) + "f);"


	ColorConstant(AliceBlue, 0.941176470588f, 0.972549019608f, 1.0f);
	ColorConstant(AntiqueWhite, 0.980392156863f, 0.921568627451f, 0.843137254902f);
	ColorConstant(Aqua, 0.0f, 1.0f, 1.0f);
	ColorConstant(Aquamarine, 0.498039215686f, 1.0f, 0.83137254902f);
	ColorConstant(Azure, 0.941176470588f, 1.0f, 1.0f);
	ColorConstant(Beige, 0.960784313725f, 0.960784313725f, 0.862745098039f);
	ColorConstant(Bisque, 1.0f, 0.894117647059f, 0.76862745098f);
	ColorConstant(BlanchedAlmond, 1.0f, 0.921568627451f, 0.803921568627f);
	ColorConstant(BlueViolet, 0.541176470588f, 0.16862745098f, 0.886274509804f);
	ColorConstant(Brown, 0.647058823529f, 0.164705882353f, 0.164705882353f);
	ColorConstant(Burlywood, 0.870588235294f, 0.721568627451f, 0.529411764706f);
	ColorConstant(CadetBlue, 0.372549019608f, 0.619607843137f, 0.627450980392f);
	ColorConstant(Chartreuse, 0.498039215686f, 1.0f, 0.0f);
	ColorConstant(Chocolate, 0.823529411765f, 0.411764705882f, 0.117647058824f);
	ColorConstant(Coral, 1.0f, 0.498039215686f, 0.313725490196f);
	ColorConstant(CornflowerBlue, 0.392156862745f, 0.58431372549f, 0.929411764706f);
	ColorConstant(Cornsilk, 1.0f, 0.972549019608f, 0.862745098039f);
	ColorConstant(Crimson, 0.862745098039f, 0.078431372549f, 0.235294117647f);
	ColorConstant(DarkBlue, 0.0f, 0.0f, 0.545098039216f);
	ColorConstant(DarkCyan, 0.0f, 0.545098039216f, 0.545098039216f);
	ColorConstant(DarkGoldenrod, 0.721568627451f, 0.525490196078f, 0.043137254902f);
	ColorConstant(DarkGray, 0.662745098039f, 0.662745098039f, 0.662745098039f);
	ColorConstant(DarkGreen, 0.0f, 0.392156862745f, 0.0f);
	ColorConstant(DarkKhaki, 0.741176470588f, 0.717647058824f, 0.419607843137f);
	ColorConstant(DarkMagenta, 0.545098039216f, 0.0f, 0.545098039216f);
	ColorConstant(DarkOliveGreen, 0.333333333333f, 0.419607843137f, 0.18431372549f);
	ColorConstant(DarkOrange, 1.0f, 0.549019607843f, 0.0f);
	ColorConstant(DarkOrchid, 0.6f, 0.196078431373f, 0.8f);
	ColorConstant(DarkRed, 0.545098039216f, 0.0f, 0.0f);
	ColorConstant(DarkSalmon, 0.913725490196f, 0.588235294118f, 0.478431372549f);
	ColorConstant(DarkSeagreen, 0.560784313725f, 0.737254901961f, 0.560784313725f);
	ColorConstant(DarkSlateBlue, 0.282352941176f, 0.239215686275f, 0.545098039216f);
	ColorConstant(DarkSlateGray, 0.18431372549f, 0.309803921569f, 0.309803921569f);
	ColorConstant(DarkTurquoise, 0.0f, 0.807843137255f, 0.819607843137f);
	ColorConstant(DarkViolet, 0.580392156863f, 0.0f, 0.827450980392f);
	ColorConstant(DeepPink, 1.0f, 0.078431372549f, 0.576470588235f);
	ColorConstant(DeepSkyBlue, 0.0f, 0.749019607843f, 1.0f);
	ColorConstant(DimGray, 0.411764705882f, 0.411764705882f, 0.411764705882f);
	ColorConstant(DodgerBlue, 0.117647058824f, 0.564705882353f, 1.0f);
	ColorConstant(Firebrick, 0.698039215686f, 0.133333333333f, 0.133333333333f);
	ColorConstant(FloralWhite, 1.0f, 0.980392156863f, 0.941176470588f);
	ColorConstant(ForestGreen, 0.133333333333f, 0.545098039216f, 0.133333333333f);
	ColorConstant(Fuchsia, 1.0f, 0.0f, 1.0f);
	ColorConstant(Gainsboro, 0.862745098039f, 0.862745098039f, 0.862745098039f);
	ColorConstant(GhostWhite, 0.972549019608f, 0.972549019608f, 1.0f);
	ColorConstant(Gold, 1.0f, 0.843137254902f, 0.0f);
	ColorConstant(Goldenrod, 0.854901960784f, 0.647058823529f, 0.125490196078f);
	ColorConstant(GreenYellow, 0.678431372549f, 1.0f, 0.18431372549f);
	ColorConstant(HoneyDew, 0.941176470588f, 1.0f, 0.941176470588f);
	ColorConstant(HotPink, 1.0f, 0.411764705882f, 0.705882352941f);
	ColorConstant(IndianRed, 0.803921568627f, 0.360784313725f, 0.360784313725f);
	ColorConstant(Indigo, 0.294117647059f, 0.0f, 0.509803921569f);
	ColorConstant(Ivory, 1.0f, 1.0f, 0.941176470588f);
	ColorConstant(Khaki, 0.941176470588f, 0.901960784314f, 0.549019607843f);
	ColorConstant(Lavender, 0.901960784314f, 0.901960784314f, 0.980392156863f);
	ColorConstant(LavenderBlush, 1.0f, 0.941176470588f, 0.960784313725f);
	ColorConstant(LawnGreen, 0.486274509804f, 0.988235294118f, 0.0f);
	ColorConstant(LemonChiffon, 1.0f, 0.980392156863f, 0.803921568627f);
	ColorConstant(LightBlue, 0.678431372549f, 0.847058823529f, 0.901960784314f);
	ColorConstant(LightCoral, 0.941176470588f, 0.501960784314f, 0.501960784314f);
	ColorConstant(LightCyan, 0.878431372549f, 1.0f, 1.0f);
	ColorConstant(LightGoldenrod, 0.980392156863f, 0.980392156863f, 0.823529411765f);
	ColorConstant(LightGray, 0.827450980392f, 0.827450980392f, 0.827450980392f);
	ColorConstant(LightGreen, 0.564705882353f, 0.933333333333f, 0.564705882353f);
	ColorConstant(LightPink, 1.0f, 0.713725490196f, 0.756862745098f);
	ColorConstant(LightSalmon, 1.0f, 0.627450980392f, 0.478431372549f);
	ColorConstant(LightSeaGreen, 0.125490196078f, 0.698039215686f, 0.666666666667f);
	ColorConstant(LightSkyBlue, 0.529411764706f, 0.807843137255f, 0.980392156863f);
	ColorConstant(LightSlateGray, 0.466666666667f, 0.533333333333f, 0.6f);
	ColorConstant(LightSteelBlue, 0.690196078431f, 0.76862745098f, 0.870588235294f);
	ColorConstant(LightYellow, 1.0f, 1.0f, 0.878431372549f);
	ColorConstant(Lime, 0.0f, 1.0f, 0.0f);
	ColorConstant(LimeGreen, 0.196078431373f, 0.803921568627f, 0.196078431373f);
	ColorConstant(Linen, 0.980392156863f, 0.941176470588f, 0.901960784314f);
	ColorConstant(Maroon, 0.501960784314f, 0.0f, 0.0f);
	ColorConstant(MediumAquamarine, 0.4f, 0.803921568627f, 0.666666666667f);
	ColorConstant(MediumBlue, 0.0f, 0.0f, 0.803921568627f);
	ColorConstant(MediumOrchid, 0.729411764706f, 0.333333333333f, 0.827450980392f);
	ColorConstant(MediumPurple, 0.576470588235f, 0.439215686275f, 0.858823529412f);
	ColorConstant(MediumSeaGreen, 0.235294117647f, 0.701960784314f, 0.443137254902f);
	ColorConstant(MediumSlateBlue, 0.482352941176f, 0.407843137255f, 0.933333333333f);
	ColorConstant(MediumSpringGreen, 0.0f, 0.980392156863f, 0.603921568627f);
	ColorConstant(MediumTurquoise, 0.282352941176f, 0.819607843137f, 0.8f);
	ColorConstant(MediumVioletRed, 0.780392156863f, 0.0823529411765f, 0.521568627451f);
	ColorConstant(MidnightBlue, 0.0980392156863f, 0.0980392156863f, 0.439215686275f);
	ColorConstant(MintCream, 0.960784313725f, 1.0f, 0.980392156863f);
	ColorConstant(MistyRose, 1.0f, 0.894117647059f, 0.882352941176f);
	ColorConstant(Moccasin, 1.0f, 0.894117647059f, 0.709803921569f);
	ColorConstant(NavajoWhite, 1.0f, 0.870588235294f, 0.678431372549f);
	ColorConstant(Navy, 0.0f, 0.0f, 0.501960784314f);
	ColorConstant(OldLace, 0.992156862745f, 0.960784313725f, 0.901960784314f);
	ColorConstant(Olive, 0.501960784314f, 0.501960784314f, 0.0f);
	ColorConstant(OliveDrab, 0.419607843137f, 0.556862745098f, 0.137254901961f);
	ColorConstant(Orange, 1.0f, 0.647058823529f, 0.0f);
	ColorConstant(OrangeRed, 1.0f, 0.270588235294f, 0.0f);
	ColorConstant(Orchid, 0.854901960784f, 0.439215686275f, 0.839215686275f);
	ColorConstant(PaleGoldenrod, 0.933333333333f, 0.909803921569f, 0.666666666667f);
	ColorConstant(PaleGreen, 0.596078431373f, 0.98431372549f, 0.596078431373f);
	ColorConstant(PaleTurquoise, 0.686274509804f, 0.933333333333f, 0.933333333333f);
	ColorConstant(PaleVioletRed, 0.858823529412f, 0.439215686275f, 0.576470588235f);
	ColorConstant(PapayaWhip, 1.0f, 0.937254901961f, 0.835294117647f);
	ColorConstant(PeachPuff, 1.0f, 0.854901960784f, 0.725490196078f);
	ColorConstant(Peru, 0.803921568627f, 0.521568627451f, 0.247058823529f);
	ColorConstant(Pink, 1.0f, 0.752941176471f, 0.796078431373f);
	ColorConstant(Plum, 0.866666666667f, 0.627450980392f, 0.866666666667f);
	ColorConstant(PowderBlue, 0.690196078431f, 0.878431372549f, 0.901960784314f);
	ColorConstant(Purple, 0.501960784314f, 0.0f, 0.501960784314f);
	ColorConstant(RosyBrown, 0.737254901961f, 0.560784313725f, 0.560784313725f);
	ColorConstant(RoyalBlue, 0.254901960784f, 0.411764705882f, 0.882352941176f);
	ColorConstant(SaddleBrown, 0.545098039216f, 0.270588235294f, 0.0745098039216f);
	ColorConstant(Salmon, 0.980392156863f, 0.501960784314f, 0.447058823529f);
	ColorConstant(SandyBrown, 0.956862745098f, 0.643137254902f, 0.376470588235f);
	ColorConstant(SeaGreen, 0.180392156863f, 0.545098039216f, 0.341176470588f);
	ColorConstant(Seashell, 1.0f, 0.960784313725f, 0.933333333333f);
	ColorConstant(Sienna, 0.627450980392f, 0.321568627451f, 0.176470588235f);
	ColorConstant(Silver, 0.752941176471f, 0.752941176471f, 0.752941176471f);
	ColorConstant(SkyBlue, 0.529411764706f, 0.807843137255f, 0.921568627451f);
	ColorConstant(SlateBlue, 0.41568627451f, 0.352941176471f, 0.803921568627f);
	ColorConstant(SlateGray, 0.439215686275f, 0.501960784314f, 0.564705882353f);
	ColorConstant(Snow, 1.0f, 0.980392156863f, 0.980392156863f);
	ColorConstant(SpringGreen, 0.0f, 1.0f, 0.498039215686f);
	ColorConstant(SteelBlue, 0.274509803922f, 0.509803921569f, 0.705882352941f);
	ColorConstant(Tan, 0.823529411765f, 0.705882352941f, 0.549019607843f);
	ColorConstant(Teal, 0.0f, 0.501960784314f, 0.501960784314f);
	ColorConstant(Thistle, 0.847058823529f, 0.749019607843f, 0.847058823529f);
	ColorConstant(Tomato, 1.0f, 0.388235294118f, 0.278431372549f);
	ColorConstant(Turquoise, 0.250980392157f, 0.878431372549f, 0.81568627451f);
	ColorConstant(Violet, 0.933333333333f, 0.509803921569f, 0.933333333333f);
	ColorConstant(Wheat, 0.960784313725f, 0.870588235294f, 0.701960784314f);
	ColorConstant(WhiteSmoke, 0.960784313725f, 0.960784313725f, 0.960784313725f);
	ColorConstant(YellowGreen, 0.603921568627f, 0.803921568627f, 0.196078431373f);






	/**
	 * Select a Color from a fixed color palette based on given Index
	 * @param Index arbitrary integer
	 * @return Color selected from a fixed set, or White if Index < 0
	 */
	template <typename VectorType>
	VectorType SelectColor(int32 Index)
	{
		static const FLinearColor ColorMap[] = {
			SpringGreen3f(), Plum3f(), Khaki3f(),
			PaleGreen3f(), LightSteelBlue3f(), Aquamarine3f(),
			Salmon3f(), Goldenrod3f(), LightSeaGreen3f(),
			IndianRed3f(), DarkSalmon3f(), Coral3f(),
			Burlywood3f(), GreenYellow3f(), Lavender3f(),
			MediumAquamarine3f(), Thistle3f(), Wheat3f(),
			LightSkyBlue3f(), LightPink3f(), MediumSpringGreen3f()
		};
		return VectorType( (Index <= 0) ? White3f() : ColorMap[(Index-1) % (7*3)] );
	}


	/**
	 * Select a FColor from a fixed color palette based on given Index
	 * @param Index arbitrary integer
	 * @return Color selected from a fixed set, or White if Index < 0
	 */
	GEOMETRYCORE_API FColor SelectFColor(int32 Index);

}


} // end namespace UE::Geometry
} // end namespace UE