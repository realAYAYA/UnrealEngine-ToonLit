// Copyright Epic Games, Inc. All Rights Reserved.

#include "3DElement2String.h"
#include "ElementTools.h"
#include "Element2String.h"
#include "TAssValueName.h"

DISABLE_SDK_WARNINGS_START

#include "ModelElement.hpp"
#include "ModelMeshBody.hpp"
#include "ConvexPolygon.hpp"

#include "Transformation.hpp"
#include "Parameter.hpp"
#include "Light.hpp"

DISABLE_SDK_WARNINGS_END

BEGIN_NAMESPACE_UE_AC

static void AddIfNotZero(utf8_string* IOString, const char* InFmt, int32 InCount)
{
	if (InCount != 0)
	{
		*IOString += Utf8StringFormat(InFmt, InCount);
	}
}

static void AddIfTrue(utf8_string* IOString, const char* InFmt, bool InValue)
{
	if (InValue)
	{
		*IOString += Utf8StringFormat(InFmt, "true");
	}
}

utf8_string F3DElement2String::Element2String(const ModelerAPI::Element& InModelElement)
{
	utf8_string InfoStr;

	if (!InModelElement.IsInvalid())
	{
		InfoStr += Utf8StringFormat("GetType=%d\n", InModelElement.GetType());
		InfoStr += Utf8StringFormat("Guid=%s\n", InModelElement.GetElemGuid().ToUniString().ToUtf8());
		InfoStr += Utf8StringFormat("GenId=%u\n", InModelElement.GetGenId());
		AddIfNotZero(&InfoStr, "TessellatedBodyCount=%d\n", InModelElement.GetTessellatedBodyCount());
		AddIfNotZero(&InfoStr, "MeshBodyCount=%d\n", InModelElement.GetMeshBodyCount());
		AddIfNotZero(&InfoStr, "NurbsBodyCount=%d\n", InModelElement.GetNurbsBodyCount());
		AddIfNotZero(&InfoStr, "PointCloud=%d\n", InModelElement.GetPointCloudCount());
		AddIfNotZero(&InfoStr, "LightCount=%d\n", InModelElement.GetLightCount());
		Box3D Box = InModelElement.GetBounds();
#if AC_VERSION < 24
		InfoStr += Utf8StringFormat("Box={{%lf, %lf, %lf}, {%lf, %lf, %lf}}\n", Box.xMin, Box.yMin, Box.zMin, Box.xMax,
									Box.yMax, Box.zMax);
#else
		InfoStr += Utf8StringFormat("Box={{%lf, %lf, %lf}, {%lf, %lf, %lf}}\n", Box.GetMinX(), Box.GetMinY(),
									Box.GetMinZ(), Box.GetMaxX(), Box.GetMaxY(), Box.GetMaxZ());
#endif
		// GetBaseElemId
		ModelerAPI::Transformation Transform = InModelElement.GetElemLocalToWorldTransformation();
		InfoStr += Utf8StringFormat("LocalToWorldTransformation\n\tStatus=%d\n", Transform.status);
		for (size_t IndexRow = 0; IndexRow < 3; ++IndexRow)
		{
			InfoStr += Utf8StringFormat("\t{%lf, %lf, %lf, %lf}\n", Transform.matrix[IndexRow][0],
										Transform.matrix[IndexRow][1], Transform.matrix[IndexRow][2],
										Transform.matrix[IndexRow][3]);
		}
		GS::Int32 BodyCount = InModelElement.GetMeshBodyCount();
		for (Int32 IndexBody = 1; IndexBody <= BodyCount; ++IndexBody)
		{
			ModelerAPI::MeshBody Body;
			InModelElement.GetMeshBody(IndexBody, &Body);
			InfoStr += Body2String(Body);
		}
	}
	else
	{
		InfoStr += "Element is invalid";
	}

	return InfoStr;
}

utf8_string F3DElement2String::Body2String(const ModelerAPI::MeshBody& InBodyElement)
{
	utf8_string InfoStr;

	AddIfTrue(&InfoStr, "\t\tWireBody=%s\n", InBodyElement.IsWireBody());
	AddIfTrue(&InfoStr, "\t\tIsSurfaceBody=%s\n", InBodyElement.IsSurfaceBody());
	AddIfTrue(&InfoStr, "\t\tIsSolidBody=%s\n", InBodyElement.IsSolidBody());
	AddIfTrue(&InfoStr, "\t\tIsClosed=%s\n", InBodyElement.IsClosed());
	AddIfTrue(&InfoStr, "\t\tIsVisibleIfContour=%s\n", InBodyElement.IsVisibleIfContour());
	AddIfTrue(&InfoStr, "\t\tHasSharpEdge=%s\n", InBodyElement.HasSharpEdge());
	AddIfTrue(&InfoStr, "\t\tAlwaysCastsShadow=%s\n", InBodyElement.AlwaysCastsShadow());
	AddIfTrue(&InfoStr, "\t\tNeverCastsShadow=%s\n", InBodyElement.NeverCastsShadow());
	AddIfTrue(&InfoStr, "\t\tDoesNotReceiveShadow=%s\n", InBodyElement.DoesNotReceiveShadow());
	AddIfTrue(&InfoStr, "\t\tHasColor=%s\n", InBodyElement.HasColor());

	AddIfNotZero(&InfoStr, "\t\tVertexCount=%d\n", InBodyElement.GetVertexCount());
	AddIfNotZero(&InfoStr, "\t\tEdgeCount=%d\n", InBodyElement.GetEdgeCount());
	AddIfNotZero(&InfoStr, "\t\tPolygonCount=%d\n", InBodyElement.GetPolygonCount());
	AddIfNotZero(&InfoStr, "\t\tPolygonVectorCount=%d\n", InBodyElement.GetPolygonVectorCount());

	return InfoStr;
}

// clang-format off
template <>
FAssValueName::SAssValueName TAssEnumName< ModelerAPI::Light::Type >::AssEnumName[] = {
	EnumName(ModelerAPI::Light, UndefinedLight),
	EnumName(ModelerAPI::Light, DirectionLight),
	EnumName(ModelerAPI::Light, SpotLight),
	EnumName(ModelerAPI::Light, PointLight),
	EnumName(ModelerAPI::Light, SunLight),
	EnumName(ModelerAPI::Light, EyeLight),
	EnumName(ModelerAPI::Light, AmbientLight),
	EnumName(ModelerAPI::Light, CameraLight),

	EnumEnd(-1)
};
// clang-format on

utf8_string F3DElement2String::ElementLight2String(const ModelerAPI::Element& InModelElement)
{
	utf8_string InfoStr;

	GS::Int32 LightsCount = InModelElement.GetLightCount();
	if (LightsCount > 0)
	{
		const API_Guid&	  Guid = GSGuid2APIGuid(InModelElement.GetElemGuid());
		ModelerAPI::Light Light;
		for (GS::Int32 LightIndex = 1; LightIndex <= LightsCount; ++LightIndex)
		{
			InModelElement.GetLight(LightIndex, &Light);
			ModelerAPI::Light::Type LightType = Light.GetType();

			API_Guid LightId = CombineGuid(Guid, GuidFromMD5(LightIndex));
			InfoStr += Utf8StringFormat("Light: %d {%s} Type=%s\n", LightIndex, APIGuidToString(LightId).ToUtf8(),
										TAssEnumName< ModelerAPI::Light::Type >::GetName(LightType));
			InfoStr += Utf8StringFormat(
				"\tColor=%s, Position=%s, Direction=%s, UpVector=%s\n", Color2String(Light.GetColor()).c_str(),
				Vertex2String(Light.GetPosition()).c_str(), Vector2String(Light.GetDirection()).c_str(),
				Vector2String(Light.GetUpVector()).c_str());
			InfoStr += Utf8StringFormat("\tRadius=%lf, AngleFalloff=%lf, FalloffAngle1=%lf, FalloffAngle2=%lf, "
										"DistanceFalloff=%lf, MinDistance=%lf, MaxDistance=%lf\n",
										Light.GetRadius(), Light.GetAngleFalloff(), Light.GetFalloffAngle1(),
										Light.GetFalloffAngle2(), Light.GetDistanceFalloff(), Light.GetMinDistance(),
										Light.GetMaxDistance());

			ModelerAPI::ParameterList Parameters;
			Light.GetExtraParameters(&Parameters);
			InfoStr += ParameterList2String(Parameters);
		}
		InfoStr += Utf8StringFormat("\n");
	}

	return InfoStr;
}

utf8_string F3DElement2String::Parameter2String(const ModelerAPI::Parameter& InParameter)
{
	utf8_string	  InfoStr;
	Int32		  dim1 = 0;
	Int32		  dim2 = 0;
	double		  numericValue;
	GS::UniString strValue;

	switch (InParameter.GetType())
	{
		case ModelerAPI::Parameter::NumericType:
			numericValue = InParameter;
			InfoStr +=
				Utf8StringFormat("%s\n\ttype: numeric\n\tvalue: %f\n", InParameter.GetName().ToUtf8(), numericValue);
			break;
		case ModelerAPI::Parameter::StringType:
			InParameter.GetStringValue(strValue);
			InfoStr += Utf8StringFormat("%s\n\ttype: string\n\tvalue: %s\n", InParameter.GetName().ToUtf8(),
										strValue.ToUtf8());
			break;
		case ModelerAPI::Parameter::NumericArrayType:
			InParameter.GetArrayDimensions(&dim1, &dim2);
			InfoStr += Utf8StringFormat("%s\n\ttype: numeric array\n\tdims: %dx%d\n", InParameter.GetName().ToUtf8(),
										dim1, dim2);
			for (Int32 jj = 1; jj <= dim1; ++jj)
			{
				if (dim2 == 0)
				{
					numericValue = InParameter[jj];
					InfoStr += Utf8StringFormat("\t[%d] value: %f\n", jj, numericValue);
				}
				else
				{
					for (Int32 ll = 1; ll <= dim2; ++ll)
					{
						ModelerAPI::ArrayParameter arrayItem;
						InParameter.GetArrayItem(jj, ll, &arrayItem);
						numericValue = InParameter;
						InfoStr += Utf8StringFormat("\t[%d][%d] value: %f\n", jj, ll, numericValue);
					}
				}
			}
			break;
		case ModelerAPI::Parameter::StringArrayType:
			InParameter.GetArrayDimensions(&dim1, &dim2);
			InfoStr += Utf8StringFormat("%s\n\ttype: numeric array\n\tdims: %dx%d\n", InParameter.GetName().ToUtf8(),
										dim1, dim2);
			for (Int32 jj = 1; jj <= dim1; ++jj)
			{
				if (dim2 == 0)
				{
					InParameter[jj].GetStringValue(strValue);
					InfoStr += Utf8StringFormat("\t[%d] value: %s\n", jj, strValue.ToUtf8());
				}
				else
				{
					for (Int32 ll = 1; ll <= dim2; ++ll)
					{
						ModelerAPI::ArrayParameter arrayItem;
						InParameter.GetArrayItem(jj, ll, &arrayItem);
						arrayItem.GetStringValue(strValue);
						InfoStr += Utf8StringFormat("\t[%d][%d] value: %s\n", jj, ll, strValue.ToUtf8());
					}
				}
			}
			break;
		default:
			InfoStr += Utf8StringFormat("%s\n\ttype: undefined\n", InParameter.GetName().ToUtf8());
			break;
	}

	return InfoStr;
}

utf8_string F3DElement2String::ParameterList2String(const ModelerAPI::ParameterList& InParameterList)
{
	utf8_string InfoStr;

	Int32 Count = InParameterList.GetParameterCount();
	for (Int32 Index = 1; Index < Count; ++Index)
	{
		ModelerAPI::Parameter Param;
		if (InParameterList.GetParameter(Index, &Param))
		{
			InfoStr += Utf8StringFormat("Parameter %d: ", Index) + Parameter2String(Param);
		}
	}

	return InfoStr;
}

utf8_string F3DElement2String::Color2String(const ModelerAPI::Color& InColor)
{
	return Utf8StringFormat("(%lf, %lf, %lf)", InColor.red, InColor.green, InColor.blue);
}

utf8_string F3DElement2String::Vertex2String(const ModelerAPI::Vertex& InVertex)
{
	return Utf8StringFormat("(%lf, %lf, %lf)", InVertex.x, InVertex.y, InVertex.z);
}

utf8_string F3DElement2String::Vector2String(const ModelerAPI::Vector& InVector)
{
	return Utf8StringFormat("(%lf, %lf, %lf)", InVector.x, InVector.y, InVector.z);
}

END_NAMESPACE_UE_AC
