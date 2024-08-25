// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "Containers/UnrealString.h"

#include "NodeModifier.generated.h"


/** Despite being an UEnum, this is not always version-serialized (in MutableTools).
* Beware of changing the enum options or order.
*/
UENUM()
enum class EMutableMultipleTagPolicy : uint8
{
	OnlyOneRequired,
	AllRequired
};


namespace mu
{

	// Forward definitions
	class NodeModifier;
	typedef Ptr<NodeModifier> NodeModifierPtr;
	typedef Ptr<const NodeModifier> NodeModifierConst;

	//! This class is the parent of all nodes that output a component.
	//! \ingroup model
	class MUTABLETOOLS_API NodeModifier : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			MeshClipMorphPlane = 0,
			MeshClipWithMesh = 1,
			MeshClipDeform = 2,
			
			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeModifier* pNode, OutputArchive& arch );
		static Ptr<NodeModifier> StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        /** Add a tag to the surface, which will be affected by modifier nodes with the same tag. */
        void AddTag(const FString& TagName);

		/** Set the policy to interprete the tags when there is more than one. */
		void SetMultipleTagPolicy(EMutableMultipleTagPolicy);

		/** Set the stage to apply this modifier in.Default is before normal operations. */
		void SetStage( bool bBeforeNormalOperation );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeModifier() {}

		//!
		EType Type = EType::None;

	};


}
