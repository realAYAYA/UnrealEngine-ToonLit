// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNodeComment.h"

namespace Metasound
{
	namespace Editor
	{
		class SMetasoundGraphNodeComment : public SGraphNodeComment
		{
		public:
			// SNodePanel::SNode interface
			virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
			// End of SNodePanel::SNode interface
		};
	} // namespace Editor
} // namespace Metasound
