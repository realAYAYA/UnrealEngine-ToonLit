// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeSurface.h"

namespace mu
{

	// Forward definitions
    class NodeSurfaceVariation;
    typedef Ptr<NodeSurfaceVariation> NodeSurfaceVariationPtr;
    typedef Ptr<const NodeSurfaceVariation> NodeSurfaceVariationPtrConst;

    class NodeModifier;


	//! This node modifies a node of the parent object of the object that this node belongs to.
    //! It allows to extend, cut and morph the parent Surface's meshes.
    //! It also allows to patch the parent Surface's textures.
	//! \ingroup model
    class MUTABLETOOLS_API NodeSurfaceVariation : public NodeSurface
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeSurfaceVariation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeSurfaceVariation* pNode, OutputArchive& arch );
		static NodeSurfaceVariationPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! 
        void AddDefaultSurface(NodeSurface* surface);
        void AddDefaultModifier(NodeModifier* modifier);

		//! Set the number of tags to consider in this variation
		void SetVariationCount( int count );

		//!
		int GetVariationCount() const;

        //!
        enum class VariationType : uint8
        {
            //! The variation selection is controlled by tags defined in other surfaces.
            //! Default value.
            Tag = 0,

            //! The variation selection is controlled by the state the object is in.
            State
        };

        //!
        void SetVariationType(VariationType);

        //! Set the tag or state name that will enable a specific vartiation
		void SetVariationTag(int index, const FString& Tag);

		//! 
        void AddVariationSurface(int index, NodeSurface* surface);
        void AddVariationModifier(int index, NodeModifier* modifier);

		//!}


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeSurfaceVariation();

	private:

		Private* m_pD;

	};



}
