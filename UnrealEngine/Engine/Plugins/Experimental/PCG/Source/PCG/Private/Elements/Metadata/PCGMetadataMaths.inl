// Copyright Epic Games, Inc. All Rights Reserved.

namespace PCGMetadataMaths
{
	////////////////////
	// Sign
	////////////////////
	template <typename T>
	inline T Sign(const T& Value)
	{
		double Temp(Value);
		return Temp > 0.0 ? static_cast<T>(1) : (Temp < 0.0 ? static_cast<T>(-1) : static_cast<T>(0));
	}

	template<>
	inline FVector2D Sign(const FVector2D& Value)
	{
		return FVector2D(Sign(Value.X), Sign(Value.Y));
	}

	template<>
	inline FVector Sign(const FVector& Value)
	{
		return FVector(Sign(Value.X), Sign(Value.Y), Sign(Value.Z));
	}

	template<>
	inline FVector4 Sign(const FVector4& Value)
	{
		return FVector4(Sign(Value.X), Sign(Value.Y), Sign(Value.Z), Sign(Value.W));
	}

	////////////////////
	// Frac
	////////////////////
	template <typename T>
	inline T Frac(const T& Value)
	{
		return static_cast<T>(FMath::Frac((double)Value));
	}

	template<>
	inline FVector2D Frac(const FVector2D& Value)
	{
		return FVector2D(Frac(Value.X), Frac(Value.Y));
	}

	template<>
	inline FVector Frac(const FVector& Value)
	{
		return FVector(Frac(Value.X), Frac(Value.Y), Frac(Value.Z));
	}

	template<>
	inline FVector4 Frac(const FVector4& Value)
	{
		return FVector4(Frac(Value.X), Frac(Value.Y), Frac(Value.Z), Frac(Value.W));
	}

	////////////////////
	// Truncate
	////////////////////
	template <typename T>
	inline T Truncate(const T& Value)
	{
		return static_cast<T>(FMath::TruncToDouble((double)Value));
	}

	template<>
	inline FVector2D Truncate(const FVector2D& Value)
	{
		return FVector2D(Truncate(Value.X), Truncate(Value.Y));
	}

	template<>
	inline FVector Truncate(const FVector& Value)
	{
		return FVector(Truncate(Value.X), Truncate(Value.Y), Truncate(Value.Z));
	}

	template<>
	inline FVector4 Truncate(const FVector4& Value)
	{
		return FVector4(Truncate(Value.X), Truncate(Value.Y), Truncate(Value.Z), Truncate(Value.W));
	}

	////////////////////
	// Round
	////////////////////
	template <typename T>
	inline T Round(const T& Value)
	{
		return static_cast<T>(FMath::RoundToDouble((double)Value));
	}

	template<>
	inline FVector2D Round(const FVector2D& Value)
	{
		return FVector2D(Round(Value.X), Round(Value.Y));
	}

	template<>
	inline FVector Round(const FVector& Value)
	{
		return FVector(Round(Value.X), Round(Value.Y), Round(Value.Z));
	}

	template<>
	inline FVector4 Round(const FVector4& Value)
	{
		return FVector4(Round(Value.X), Round(Value.Y), Round(Value.Z), Round(Value.W));
	}

	////////////////////
	// Sqrt
	////////////////////
	template <typename T>
	inline T Sqrt(const T& Value)
	{
		return static_cast<T>(FMath::Sqrt((double)Value));
	}

	template<>
	inline FVector2D Sqrt(const FVector2D& Value)
	{
		return FVector2D(Sqrt(Value.X), Sqrt(Value.Y));
	}

	template<>
	inline FVector Sqrt(const FVector& Value)
	{
		return FVector(Sqrt(Value.X), Sqrt(Value.Y), Sqrt(Value.Z));
	}

	template<>
	inline FVector4 Sqrt(const FVector4& Value)
	{
		return FVector4(Sqrt(Value.X), Sqrt(Value.Y), Sqrt(Value.Z), Sqrt(Value.W));
	}

	////////////////////
	// Abs
	////////////////////
	template <typename T>
	inline T Abs(const T& Value)
	{
		return FMath::Abs(Value);
	}

	template<>
	inline FVector2D Abs(const FVector2D& Value)
	{
		return Value.GetAbs();
	}

	template<>
	inline FVector Abs(const FVector& Value)
	{
		return Value.GetAbs();
	}

	template<>
	inline FVector4 Abs(const FVector4& Value)
	{
		return FVector4(FMath::Abs(Value.X), FMath::Abs(Value.Y), FMath::Abs(Value.Z), FMath::Abs(Value.W));
	}

	////////////////////
	// Max
	////////////////////
	template <typename T>
	inline T Max(const T& Value1, const T& Value2)
	{
		return FMath::Max(Value1, Value2);
	}

	template<>
	inline FVector2D Max(const FVector2D& Value1, const FVector2D& Value2)
	{
		return FVector2D::Max(Value1, Value2);
	}

	template<>
	inline FVector Max(const FVector& Value1, const FVector& Value2)
	{
		return FVector::Max(Value1, Value2);
	}

	template<>
	inline FVector4 Max(const FVector4& Value1, const FVector4& Value2)
	{
		return FVector4(FMath::Max(Value1.X, Value2.X), FMath::Max(Value1.Y, Value2.Y), FMath::Max(Value1.Z, Value2.Z), FMath::Max(Value1.W, Value2.W));
	}

	////////////////////
	// Min
	////////////////////
	template <typename T>
	inline T Min(const T& Value1, const T& Value2)
	{
		return FMath::Min(Value1, Value2);
	}

	template<>
	inline FVector2D Min(const FVector2D& Value1, const FVector2D& Value2)
	{
		return FVector2D::Min(Value1, Value2);
	}

	template<>
	inline FVector Min(const FVector& Value1, const FVector& Value2)
	{
		return FVector::Min(Value1, Value2);
	}

	template<>
	inline FVector4 Min(const FVector4& Value1, const FVector4& Value2)
	{
		return FVector4(FMath::Min(Value1.X, Value2.X), FMath::Min(Value1.Y, Value2.Y), FMath::Min(Value1.Z, Value2.Z), FMath::Min(Value1.W, Value2.W));
	}

	////////////////////
	// Pow
	////////////////////
	template <typename T>
	inline T Pow(const T& Value, const T& Power)
	{
		return static_cast<T>(FMath::Pow((double)Value, (double)Power));
	}

	template<>
	inline FVector2D Pow(const FVector2D& Value, const FVector2D& Power)
	{
		return FVector2D(FMath::Pow(Value.X, Power.X), FMath::Pow(Value.Y, Power.Y));
	}

	template<>
	inline FVector Pow(const FVector& Value, const FVector& Power)
	{
		return FVector(FMath::Pow(Value.X, Power.X), FMath::Pow(Value.Y, Power.Y), FMath::Pow(Value.Z, Power.Z));
	}

	template<>
	inline FVector4 Pow(const FVector4& Value, const FVector4& Power)
	{
		return FVector4(FMath::Pow(Value.X, Power.X), FMath::Pow(Value.Y, Power.Y), FMath::Pow(Value.Z, Power.Z), FMath::Pow(Value.W, Power.W));
	}

	////////////////////
	// Clamp
	////////////////////
	template <typename T>
	inline T Clamp(const T& Value, const T& MinValue, const T& MaxValue)
	{
		return FMath::Clamp(Value, MinValue, MaxValue);
	}

	template<>
	inline FVector2D Clamp(const FVector2D& Value, const FVector2D& MinValue, const FVector2D& MaxValue)
	{
		return FVector2D(Clamp(Value.X, MinValue.X, MaxValue.X), Clamp(Value.Y, MinValue.Y, MaxValue.Y));
	}

	template<>
	inline FVector Clamp(const FVector& Value, const FVector& MinValue, const FVector& MaxValue)
	{
		return Value.BoundToBox(MinValue, MaxValue);
	}

	template<>
	inline FVector4 Clamp(const FVector4& Value, const FVector4& MinValue, const FVector4& MaxValue)
	{
		return FVector4(
			Clamp(Value.X, MinValue.X, MaxValue.X),
			Clamp(Value.Y, MinValue.Y, MaxValue.Y),
			Clamp(Value.Z, MinValue.Z, MaxValue.Z),
			Clamp(Value.W, MinValue.W, MaxValue.W));
	}

	////////////////////
	// Lerp
	////////////////////
	template <typename T, typename U>
	inline T Lerp(const T& Value1, const T& Value2, const U& Ratio)
	{
		return static_cast<T>(FMath::Lerp(Value1, Value2, Ratio));
	}

	template<typename U>
	inline FVector2D Lerp(const FVector2D& Value1, const FVector2D& Value2, const U& Ratio)
	{
		return FVector2D(
			Lerp(Value1.X, Value2.X, Ratio),
			Lerp(Value1.Y, Value2.Y, Ratio));
	}

	template<>
	inline FVector2D Lerp(const FVector2D& Value1, const FVector2D& Value2, const FVector2D& Ratio)
	{
		return FVector2D(
			Lerp(Value1.X, Value2.X, Ratio.X),
			Lerp(Value1.Y, Value2.Y, Ratio.Y));
	}

	template<typename U>
	inline FVector Lerp(const FVector& Value1, const FVector& Value2, const U& Ratio)
	{
		return FVector(
			Lerp(Value1.X, Value2.X, Ratio),
			Lerp(Value1.Y, Value2.Y, Ratio),
			Lerp(Value1.Z, Value2.Z, Ratio));
	}

	template<>
	inline FVector Lerp(const FVector& Value1, const FVector& Value2, const FVector& Ratio)
	{
		return FVector(
			Lerp(Value1.X, Value2.X, Ratio.X), 
			Lerp(Value1.Y, Value2.Y, Ratio.Y), 
			Lerp(Value1.Z, Value2.Z, Ratio.Z));
	}

	template<typename U>
	inline FVector4 Lerp(const FVector4& Value1, const FVector4& Value2, const U& Ratio)
	{
		return FVector4(
			Lerp(Value1.X, Value2.X, Ratio),
			Lerp(Value1.Y, Value2.Y, Ratio),
			Lerp(Value1.Z, Value2.Z, Ratio),
			Lerp(Value1.W, Value2.W, Ratio));
	}

	template<>
	inline FVector4 Lerp(const FVector4& Value1, const FVector4& Value2, const FVector4& Ratio)
	{
		return FVector4(
			Lerp(Value1.X, Value2.X, Ratio.X),
			Lerp(Value1.Y, Value2.Y, Ratio.Y),
			Lerp(Value1.Z, Value2.Z, Ratio.Z),
			Lerp(Value1.W, Value2.W, Ratio.W));
	}
}