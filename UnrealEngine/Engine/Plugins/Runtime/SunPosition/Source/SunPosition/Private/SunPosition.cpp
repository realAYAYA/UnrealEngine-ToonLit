// Copyright Epic Games, Inc. All Rights Reserved.

#include "SunPosition.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSunPosition, Log, All);
DEFINE_LOG_CATEGORY(LogSunPosition);

void TestCalculator();

void USunPositionFunctionLibrary::GetSunPosition(float Latitude, float Longitude, float TimeZone, bool bIsDaylightSavingTime, int32 Year, int32 Month, int32 Day, int32 Hours, int32 Minutes, int32 Seconds, FSunPositionData& SunPositionData)
{
#if WITH_EDITOR
//		TestCalculator();
#endif

	if (!FDateTime::Validate(Year, Month, Day, Hours, Minutes, Seconds, 0))
	{
		UE_LOG(LogSunPosition, Warning, TEXT("invalid date"));
		return;
	}

	FDateTime CalcTime(Year, Month, Day, Hours, Minutes, Seconds);

	float TimeOffset = TimeZone;
	if (bIsDaylightSavingTime)
	{
		TimeOffset += 1.0f;
	}

	double LatitudeRad = FMath::DegreesToRadians(Latitude);

	// Get the julian day (number of days since Jan 1st of the year 4713 BC)
	double JulianDay = CalcTime.GetJulianDay();
	double JulianCentury = (JulianDay - 2451545.0) / 36525.0;

	// Get the sun's mean longitude , referred to the mean equinox of julian date
	double GeomMeanLongSunDeg = FMath::Fmod(280.46646 + JulianCentury*(36000.76983 + JulianCentury*0.0003032), 360.0);
	double GeomMeanLongSunRad = FMath::DegreesToRadians(GeomMeanLongSunDeg);

	// Get the sun's mean anomaly
	double GeomMeanAnomSunDeg = 357.52911 + JulianCentury*(35999.05029 - 0.0001537*JulianCentury);
	double GeomMeanAnomSunRad = FMath::DegreesToRadians(GeomMeanAnomSunDeg);

	// Get the earth's orbit eccentricity
	double EccentEarthOrbit = 0.016708634 - JulianCentury*(0.000042037 + 0.0000001267*JulianCentury);
	
	// Get the sun's equation of the center
	double SunEqOfCtr = FMath::Sin(GeomMeanAnomSunRad)*(1.914602 - JulianCentury*(0.004817 + 0.000014*JulianCentury))
					  + FMath::Sin(2.0 * GeomMeanAnomSunRad)*(0.019993 - 0.000101*JulianCentury)
					  + FMath::Sin(3.0 * GeomMeanAnomSunRad)*0.000289;
	
	// Get the sun's true longitude
	double SunTrueLongDeg = GeomMeanLongSunDeg + SunEqOfCtr;

	// Get the sun's true anomaly
	//	double SunTrueAnomDeg = GeomMeanAnomSunDeg + SunEqOfCtr;
	//	double SunTrueAnomRad = FMath::DegreesToRadians(SunTrueAnomDeg);

	// Get the earth's distance from the sun
	//	double SunRadVectorAUs = (1.000001018*(1.0 - EccentEarthOrbit*EccentEarthOrbit)) / (1.0 + EccentEarthOrbit*FMath::Cos(SunTrueAnomRad));
	
	// Get the sun's apparent longitude
	double SunAppLongDeg = SunTrueLongDeg - 0.00569 - 0.00478*FMath::Sin(FMath::DegreesToRadians(125.04 - 1934.136*JulianCentury));
	double SunAppLongRad = FMath::DegreesToRadians(SunAppLongDeg);

	// Get the earth's mean obliquity of the ecliptic
	double MeanObliqEclipticDeg = 23.0 + (26.0 + ((21.448 - JulianCentury*(46.815 + JulianCentury*(0.00059 - JulianCentury*0.001813)))) / 60.0) / 60.0;
	
	// Get the oblique correction
	double ObliqCorrDeg = MeanObliqEclipticDeg + 0.00256*FMath::Cos(FMath::DegreesToRadians(125.04 - 1934.136*JulianCentury));
	double ObliqCorrRad = FMath::DegreesToRadians(ObliqCorrDeg);

	// Get the sun's right ascension
	double SunRtAscenRad = FMath::Atan2(FMath::Cos(ObliqCorrRad)*FMath::Sin(SunAppLongRad), FMath::Cos(SunAppLongRad));
	double SunRtAscenDeg = FMath::RadiansToDegrees(SunRtAscenRad);
	
	// Get the sun's declination
	double SunDeclinRad = FMath::Asin(FMath::Sin(ObliqCorrRad)*FMath::Sin(SunAppLongRad));
	double SunDeclinDeg = FMath::RadiansToDegrees(SunDeclinRad);

	double VarY = FMath::Pow(FMath::Tan(ObliqCorrRad / 2.0), 2.0);
	
	// Get the equation of time
	double EqOfTimeMinutes = 4.0 * FMath::RadiansToDegrees(VarY*FMath::Sin(2.0 * GeomMeanLongSunRad) - 2.0 * EccentEarthOrbit*FMath::Sin(GeomMeanAnomSunRad) + 4.0 * EccentEarthOrbit*VarY*FMath::Sin(GeomMeanAnomSunRad)*FMath::Cos(2.0 * GeomMeanLongSunRad) - 0.5*VarY*VarY*FMath::Sin(4.0 * GeomMeanLongSunRad) - 1.25*EccentEarthOrbit*EccentEarthOrbit*FMath::Sin(2.0 * GeomMeanAnomSunRad));

	// Get the hour angle of the sunrise
	double HASunriseDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Cos(FMath::DegreesToRadians(90.833)) / (FMath::Cos(LatitudeRad)*FMath::Cos(SunDeclinRad)) - FMath::Tan(LatitudeRad)*FMath::Tan(SunDeclinRad)));
	//	double SunlightDurationMinutes = 8.0 * HASunriseDeg;

	// Get the local time of the sun's rise and set
	double SolarNoonLST = (720.0 - 4.0 * Longitude - EqOfTimeMinutes + TimeOffset * 60.0) / 1440.0;
	double SunriseTimeLST = SolarNoonLST - HASunriseDeg * 4.0 / 1440.0;
	double SunsetTimeLST = SolarNoonLST + HASunriseDeg * 4.0 / 1440.0;

	// Get the true solar time
	double TrueSolarTimeMinutes = FMath::Fmod(CalcTime.GetTimeOfDay().GetTotalMinutes() + EqOfTimeMinutes + 4.0 * Longitude - 60.0 * TimeOffset, 1440.0);

	// Get the hour angle of current time
	double HourAngleDeg = TrueSolarTimeMinutes < 0 ? TrueSolarTimeMinutes / 4.0 + 180 : TrueSolarTimeMinutes / 4.0 - 180.0;
	double HourAngleRad = FMath::DegreesToRadians(HourAngleDeg);

	// Get the solar zenith angle
	double SolarZenithAngleRad = FMath::Acos(FMath::Sin(LatitudeRad)*FMath::Sin(SunDeclinRad) + FMath::Cos(LatitudeRad)*FMath::Cos(SunDeclinRad)*FMath::Cos(HourAngleRad));
	double SolarZenithAngleDeg = FMath::RadiansToDegrees(SolarZenithAngleRad);

	// Get the sun elevation
	double SolarElevationAngleDeg = 90.0 - SolarZenithAngleDeg;
	double SolarElevationAngleRad = FMath::DegreesToRadians(SolarElevationAngleDeg);
	double TanOfSolarElevationAngle = FMath::Tan(SolarElevationAngleRad);

	// Get the approximated atmospheric refraction
	double ApproxAtmosphericRefractionDeg = 0.0;
	if (SolarElevationAngleDeg <= 85.0)
	{
		if (SolarElevationAngleDeg > 5.0)
		{
			ApproxAtmosphericRefractionDeg = 58.1 / TanOfSolarElevationAngle - 0.07 / FMath::Pow(TanOfSolarElevationAngle, 3) + 0.000086 / FMath::Pow(TanOfSolarElevationAngle, 5) / 3600.0;
		}
		else
		{
			if (SolarElevationAngleDeg > -0.575)
			{
				ApproxAtmosphericRefractionDeg = 1735.0 + SolarElevationAngleDeg * (-518.2 + SolarElevationAngleDeg * (103.4 + SolarElevationAngleDeg * (-12.79 + SolarElevationAngleDeg * 0.711)));
			}
			else
			{
				ApproxAtmosphericRefractionDeg = -20.772 / TanOfSolarElevationAngle;
			}
		}
		ApproxAtmosphericRefractionDeg /= 3600.0;
	}

	// Get the corrected solar elevation
	double SolarElevationcorrectedforatmrefractionDeg = SolarElevationAngleDeg + ApproxAtmosphericRefractionDeg;

	// Get the solar azimuth 
	double tmp = FMath::RadiansToDegrees(FMath::Acos(((FMath::Sin(LatitudeRad)*FMath::Cos(SolarZenithAngleRad)) - FMath::Sin(SunDeclinRad)) / (FMath::Cos(LatitudeRad)*FMath::Sin(SolarZenithAngleRad))));
	double SolarAzimuthAngleDegcwfromN = HourAngleDeg > 0.0 ? FMath::Fmod(tmp + 180.0, 360.0) : FMath::Fmod(540.0 - tmp, 360.0);


	// offset elevation angle to fit with UE coords system
	SunPositionData.Elevation = 180.0f + SolarElevationAngleDeg;
	SunPositionData.CorrectedElevation = 180.0f + SolarElevationcorrectedforatmrefractionDeg;
	SunPositionData.Azimuth = SolarAzimuthAngleDegcwfromN;
	SunPositionData.SolarNoon = FTimespan::FromDays(SolarNoonLST);
	SunPositionData.SunriseTime = FTimespan::FromDays(SunriseTimeLST);
	SunPositionData.SunsetTime = FTimespan::FromDays(SunsetTimeLST);
}


void TestExpectedValues(float Latitude, float Longitude, float TimeZone, bool bIsDaylightSavingTime, int32 Year, int32 Month, int32 Day, int32 Hours, int32 Minutes, int32 Seconds, double ExpectedAzimuth, double ExpectedElevation, double ExpectedCorrectedElevation)
{
	const float AcceptableError = 0.01f; // in %

	// offset elevation angle to fit with UE coords system
	ExpectedElevation += 180.0;
	ExpectedCorrectedElevation += 180.0;

	FSunPositionData SunPositionData;
	USunPositionFunctionLibrary::GetSunPosition(Latitude, Longitude, TimeZone, bIsDaylightSavingTime, Year, Month, Day, Hours, Minutes, Seconds, SunPositionData);

	// Test elevation values
	float ElevError = FMath::Abs(SunPositionData.Elevation - ExpectedElevation)/ ExpectedElevation * 100.0f;
	if (ElevError > AcceptableError)
	{
		UE_LOG(LogSunPosition, Warning, TEXT("Elevation error above threshold %0.2f%%"), ElevError);
	}

	float CorrElevError = FMath::Abs(SunPositionData.CorrectedElevation - ExpectedCorrectedElevation)/ ExpectedCorrectedElevation * 100.0f;
	if (CorrElevError > AcceptableError)
	{
		UE_LOG(LogSunPosition, Warning, TEXT("Corrected elevation error above threshold %0.2f%%"), CorrElevError);
	}

	// Test azimuth values
	float AzError = FMath::Abs(SunPositionData.Azimuth - ExpectedAzimuth)/ ExpectedAzimuth * 100.0f;
	if (AzError > AcceptableError)
	{
		UE_LOG(LogSunPosition, Warning, TEXT("Azimuth error above threshold %0.2f%%"), AzError);
	}
}

void TestCalculator()
{
	// Test values calculated with the help of NOAA's solar calculator
	// https://www.esrl.noaa.gov/gmd/grad/solcalc/calcdetails.html

	// Test 1 : Sydney, Australia
	{
		TestExpectedValues(-33.0f, -151.0f, 10.0f, false, 2017, 12, 21,  0,  0,  0, 126.557,  -9.975,  -9.942);
		TestExpectedValues(-33.0f, -151.0f, 10.0f, false, 2017, 12, 21,  6, 30,  0,  70.526,  67.682,  67.689);
		TestExpectedValues(-33.0f, -151.0f, 10.0f, false, 2017, 12, 21, 12, 42,  0,	259.318, 28.670,  28.700);

		TestExpectedValues(-33.0f, -151.0f, 10.0f, false, 2017, 4, 30,  0,  0,  0,  93.712, -33.187, -33.178);
		TestExpectedValues(-33.0f, -151.0f, 10.0f, false, 2017, 4, 30,  6, 30,  0,  28.233,  37.537,  37.558);
		TestExpectedValues(-33.0f, -151.0f, 10.0f, false, 2017, 4, 30, 12, 42,  0, 293.350,   7.800,   7.911);
	}

	// Test 2 : Montreal, Canada
	{
		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2017, 12, 21,  0,  0,  0,   6.197, -68.339, -68.337);
		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2017, 12, 21,  6, 30,  0, 113.524,  -9.733,  -9.699);
		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2017, 12, 21, 12, 42,  0, 192.659,  20.556,  20.599);

		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2017, 7, 1,  0,  0,  0,   1.026, -21.904, -21.889);
		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2017, 7, 1,  6, 30,  0,  78.768,  21.932,  21.971);
		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2017, 7, 1, 12, 42,  0, 206.978,  66.127,  66.134);
						   
		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2018, 9, 1, 5,  0,  0,  74.982,   -3.237,  -3.135);
		TestExpectedValues(45.0f, -73.0f, -5.0f, false, 2018, 9, 1, 5, 30,  0,  80.308,    1.937,   2.225);
	}
}

FSunPositionData::FSunPositionData()
	: Elevation(0.0f)
	, CorrectedElevation(0.0f)
	, Azimuth(0.0f)
{

}
