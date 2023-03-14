// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairAttributes.h"

namespace HairAttribute
{
	const FName Vertex::Color("groom_color");
	const FName Vertex::Roughness("groom_roughness");
	const FName Vertex::Position("position");
	const FName Vertex::Width("groom_width");

	const FName Strand::Color("groom_color");
	const FName Strand::Roughness("groom_roughness");
	const FName Strand::GroupID("groom_group_id");
	const FName Strand::Guide("groom_guide");
	const FName Strand::ID("groom_id");
	const FName Strand::RootUV("groom_root_uv");
	const FName Strand::VertexCount("vertexcount");
	const FName Strand::Width("groom_width");
	const FName Strand::ClosestGuides("groom_closest_guides");
	const FName Strand::GuideWeights("groom_guide_weights");
	const FName Strand::BasisType("groom_basis_type");
	const FName Strand::CurveType("groom_curve_type");
	const FName Strand::Knots("groom_knots");
	const FName Strand::GroupName("groom_group_name");
	const FName Strand::GroupCardsID("groom_group_cards_id");

	const FName Groom::Color("groom_color");
	const FName Groom::Roughness("groom_roughness");
	const FName Groom::Width("groom_width");
	const FName Groom::MajorVersion("groom_version_major");
	const FName Groom::MinorVersion("groom_version_minor");
	const FName Groom::Tool("groom_tool");
	const FName Groom::Properties("groom_properties");
}
