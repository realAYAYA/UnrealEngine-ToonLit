// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"
#include "ParameterList.hpp"

namespace ModelerAPI
{
class Color;
class Vertex;
class Vector;
} // namespace ModelerAPI

BEGIN_NAMESPACE_UE_AC

class F3DElement2String
{
  public:
	static utf8_string Element2String(const ModelerAPI::Element& InModelElement);

	static utf8_string Body2String(const ModelerAPI::MeshBody& InBodyElement);

	static utf8_string ElementLight2String(const ModelerAPI::Element& InModelElement);

	static utf8_string Parameter2String(const ModelerAPI::Parameter& InParameter);

	static utf8_string ParameterList2String(const ModelerAPI::ParameterList& InParameterList);

	static utf8_string Color2String(const ModelerAPI::Color& InColor);

	static utf8_string Vertex2String(const ModelerAPI::Vertex& InVertex);

	static utf8_string Vector2String(const ModelerAPI::Vector& InVector);
};

END_NAMESPACE_UE_AC
