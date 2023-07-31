// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


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
		static NodeModifierPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        //! \name Tags
        //! \{

        //! Add a tag to the surface, which will be affected by modifier nodes with the same tag
        void AddTag(const char* tagName);

        //! Get the number of tags added to the Surface.
        int GetTagCount() const;

        //! Get a tag string from an index (0 to GetTagCount-1)
        const char* GetTag( int ) const;

        //! \}

		//! Set the stage to apply this modifier in. Default is before normal operations.
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
