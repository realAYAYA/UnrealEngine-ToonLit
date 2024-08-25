// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
	class NodeMeshFragment;
	typedef Ptr<NodeMeshFragment> NodeMeshFragmentPtr;
	typedef Ptr<const NodeMeshFragment> NodeMeshFragmentPtrConst;


	//! This node selects a fragment of a mesh, by selecting some of its layout blocks.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshFragment : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshFragment();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshFragment* pNode, OutputArchive& arch );
		static NodeMeshFragmentPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

        typedef enum {
            FT_LAYOUT_BLOCKS = 0,
            FT_FACE_GROUP_DEPRECATED
        } FRAGMENT_TYPE;

        //! Get the type of mesh fragment to extract.
        FRAGMENT_TYPE GetFragmentType() const;
        void SetFragmentType( FRAGMENT_TYPE type );

		//!
		NodeMeshPtr GetMesh() const;
		void SetMesh( NodeMeshPtr );

		//! Get the index of the layout to use to extract blocks.
        int GetLayoutOrGroup() const;
        void SetLayoutOrGroup( int layoutIndex );

		//! Set the number of layout blocks to extract.
		void SetBlockCount( int );

		//! Get one of the layout block indices
		int GetBlock( int i ) const;
		void SetBlock( int i, int blockIndex );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshFragment();

	private:

		Private* m_pD;

	};


}
