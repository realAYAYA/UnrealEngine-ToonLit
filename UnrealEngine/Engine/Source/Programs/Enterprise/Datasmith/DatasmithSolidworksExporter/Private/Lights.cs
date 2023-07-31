// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
	public class FLight
	{
		public enum EType
		{
			Point,
			Spot,
			Directional
		};

		public EType LightType { get; private set; }

		public string LightName { get; private set; }
		public string LightLabel { get; private set; }

		public bool bIsEnabled { get; private set; }

		public float Intensity { get; private set; }
		public FVec3 Color { get; private set; }

		public FVec3 PointLightPosition
		{
			get
			{
				return LightType == EType.Point ? new FVec3(Properties[0], Properties[1], Properties[2]) : null;
			}
		}

		public FVec3 SpotLightPosition
		{
			get
			{
				return LightType == EType.Spot ? new FVec3(Properties[0], Properties[1], Properties[2]) : null;
			}
		}
		public FVec3 SpotLightTarget
		{
			get
			{
				return LightType == EType.Spot ? new FVec3(Properties[3], Properties[4], Properties[5]) : null;
			}
		}
		public float SpotLightConeAngle
		{
			get
			{
				return LightType == EType.Spot ? Properties[6] : 0f;
			}
		}

		public FVec3 DirLightDirection
		{
			get
			{
				return LightType == EType.Directional ? new FVec3(Properties[0], Properties[1], Properties[2]) : null;
			}
		}

		private FLight(EType InType, string InName, string InLabel, bool bInIsEnabled, double InIntensisty, int InColorref)
		{
			LightType = InType;
			LightName = InName;
			LightLabel = InLabel;
			bIsEnabled = bInIsEnabled;
			Intensity = (float)InIntensisty;

			int R = InColorref & 0xff;
			int G = (InColorref >> 8) & 0xff;
			int B = (InColorref >> 16) & 0xff;
			int A = (InColorref >> 24) & 0xff;

			Color = new FVec3(R/255f, G/255f, B/255f);
		}

		// Specific properties
		private float[] Properties = new float[7];
	
		static public FLight MakeSpotLight(string InName, string InLabel, bool bInIsEnabled, FVec3 InPosition, FVec3 InTarget, double InConeAngle, double InIntensity, int InColor)
		{
			FLight SpotLight = new FLight(EType.Spot, InName, InLabel, bInIsEnabled, InIntensity, InColor);

			// Position
			SpotLight.Properties[0] = InPosition.X;
			SpotLight.Properties[1] = InPosition.Y;
			SpotLight.Properties[2] = InPosition.Z;
			// Target
			SpotLight.Properties[3] = InTarget.X;
			SpotLight.Properties[4] = InTarget.Y;
			SpotLight.Properties[5] = InTarget.Z;
			// Angle
			SpotLight.Properties[6] = (float)InConeAngle;

			return SpotLight;
		}

		static public FLight MakePointLight(string InName, string InLabel, bool bInIsEnabled, FVec3 InPosition, double InIntensity, int InColor)
		{
			FLight PointLight = new FLight(EType.Point, InName, InLabel, bInIsEnabled, InIntensity, InColor);

			// Position
			PointLight.Properties[0] = InPosition.X;
			PointLight.Properties[1] = InPosition.Y;
			PointLight.Properties[2] = InPosition.Z;

			return PointLight;
		}

		static public FLight MakeDirLight(string InName, string InLabel, bool bInIsEnabled, FVec3 InDirection, double InIntensity, int InColor)
		{
			FLight DirLight = new FLight(EType.Directional, InName, InLabel, bInIsEnabled, InIntensity, InColor);

			// Position
			DirLight.Properties[0] = InDirection.X;
			DirLight.Properties[1] = InDirection.Y;
			DirLight.Properties[2] = InDirection.Z;

			return DirLight;
		}
	};

	public class FLightExporter
	{
		private const int SPOT_LIGHT = 2;
		private const int POINT_LIGHT = 3;
		private const int DIR_LIGHT = 4;

		// Helper struct to pack/unpack integers to/from doubles.
		// (some Solidworks API methods returns integers pack into doubles)
		[StructLayout(LayoutKind.Explicit)]
		private struct FDoubleIntConv
		{
			// An 8-byte double contains 2 4-byte ints.
			[FieldOffset(0)] private int Int;
			[FieldOffset(0)] private double Double;

			public static void Unpack(double InValue, out int OutInt)
			{
				FDoubleIntConv CV = new FDoubleIntConv();
				CV.Double = InValue;
				OutInt = CV.Int;
			}
		}

		public static List<FLight> ExportLights(ModelDoc2 InDoc)
		{
			List<FLight> SceneLights = new List<FLight>();

			for (int LightIndex = 0; LightIndex < InDoc.GetLightSourceCount(); ++LightIndex)
			{
				string LightName = InDoc.GetLightSourceName(LightIndex);
				string LightLabel = InDoc.LightSourceUserName[LightIndex];
				double[] Props = InDoc.LightSourcePropertyValues[LightIndex] as double[];

				int LightType;
				FDoubleIntConv.Unpack(Props[0], out LightType);

				double AmbientIntensity = 0.0;
				double DiffuseIntensity = 0.0;
				double SpecularIntensity = 0.0;
				int Color = 0;
				bool bIsEnabled = true;
				bool bIsFixed = true;

				FLight Light = null;

				switch (LightType)
				{
					case SPOT_LIGHT:
					{
						double PosX = 0.0, PosY = 0.0, PosZ = 0.0;
						double DirX = 0.0, DirY = 0.0, DirZ = 0.0;
						double ConeAngle = 0.0;

						if (InDoc.GetSpotlightProperties(LightName, ref AmbientIntensity, ref DiffuseIntensity, ref SpecularIntensity, ref Color, ref bIsEnabled, ref bIsFixed, ref PosX, ref PosY, ref PosZ, ref DirX, ref DirY, ref DirZ, ref ConeAngle))
						{
							Light = FLight.MakeSpotLight(LightName, LightLabel, bIsEnabled, new FVec3(PosX, PosY, PosZ), new FVec3(DirX, DirY, DirZ), ConeAngle, DiffuseIntensity, Color);
							double[] Props2 = InDoc.LightSourcePropertyValues[LightIndex] as double[];
						}
					}
					break;
					case POINT_LIGHT:
					{
						double PosX = 0.0, PosY = 0.0, PosZ = 0.0;

						if (InDoc.GetPointLightProperties(LightName, ref AmbientIntensity, ref DiffuseIntensity, ref SpecularIntensity, ref Color, ref bIsEnabled, ref bIsFixed, ref PosX, ref PosY, ref PosZ))
						{
							Light = FLight.MakePointLight(LightName, LightLabel, bIsEnabled, new FVec3(PosX, PosY, PosZ), DiffuseIntensity, Color);
						}
					}
					break;
					case DIR_LIGHT:
					{
						if (LightName != "Directional-1") // Don't export three default directional lights(they all have same internal name)
						{
							double TargetX = 0.0, TargetY = 0.0, TargetZ = 0.0;
							if (InDoc.GetDirectionLightProperties(LightName, ref AmbientIntensity, ref DiffuseIntensity,
								    ref SpecularIntensity, ref Color, ref bIsEnabled, ref bIsFixed, ref TargetX,
								    ref TargetY, ref TargetZ))
							{
								Light = FLight.MakeDirLight(LightName, LightLabel, bIsEnabled,
									new FVec3(TargetX, TargetY, TargetZ), DiffuseIntensity, Color);
							}
						}					}
					break;
				}

				if (Light != null)
				{
					SceneLights.Add(Light);
				}
			}

			return SceneLights;
		}
	}
}