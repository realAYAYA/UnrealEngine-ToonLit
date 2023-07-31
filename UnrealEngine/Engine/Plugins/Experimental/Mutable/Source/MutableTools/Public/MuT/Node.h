// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"

namespace mu { class InputArchive; }
namespace mu { class OutputArchive; }


//! This tag is used to identify files containing serialised Node hierarchies. The tag is not added
//! or checked by the Node serialisation methods, but the involved tools should take care of it.
#define MUTABLE_SOURCE_MODEL_FILETAG	"amsm"


namespace mu
{
	//! \defgroup tools Tools library classes
	//! Tools library classes.

	//! \defgroup model Model nodes
	//! \ingroup tools
	//! This group contains the nodes that can be used to compose models.

	//! \defgroup transform Transform nodes
	//! \ingroup tools
	//! This group contains the nodes that can be used to compose model transformations.


	// Forward declarations
	class Node;
	typedef Ptr<Node> NodePtr;
	typedef Ptr<const Node> NodePtrConst;

	class NodeMap;
	typedef Ptr<NodeMap> NodeMapPtr;
	typedef Ptr<const NodeMap> NodeMapPtrConst;


	//! Information about the type of a node, to provide some means to the tools to deal generically
	//! with nodes.
	//! \ingroup tools
	struct NODE_TYPE
	{
		NODE_TYPE();
		NODE_TYPE( const char* strName, const NODE_TYPE* pParent );

		const char* m_strName;
		const NODE_TYPE* m_pParent;
	};



    //! %Base class for all graphs used in the source data to define models and transforms.
	class MUTABLETOOLS_API Node : public RefCounted
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Colour = 0,
			Component = 1,
			Image = 2,
			Layout = 3,
			LOD = 4,
			Mesh = 5,
			Object = 6,
			PatchImage = 8,
			Scalar = 9,
			PatchMesh = 13,
			Volume_Deprecated = 14,
			Projector = 15,
			Surface = 16,
			Modifier = 18,
			Range = 19,
			String = 20,
			Bool = 21,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const Node* pNode, OutputArchive& arch );
		virtual void SerialiseWrapper(OutputArchive& arch) const = 0;
		static NodePtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Node type hierarchy data.
        virtual const NODE_TYPE* GetType() const;
		static const NODE_TYPE* GetStaticType();

		//! Generic access to graph connections
        virtual int GetInputCount() const = 0;
		virtual Node* GetInputNode( int i ) const = 0;
        virtual void SetInputNode( int i, NodePtr pNode ) = 0;

		//! Set the opaque context returned in messages in the compiler log.
		void SetMessageContext( const void* context );

		//-----------------------------------------------------------------------------------------
        // Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		virtual Private* GetBasePrivate() const = 0;

	protected:

		inline ~Node() {}

		//!
		EType Type = EType::None;

	};


}
