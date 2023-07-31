// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	//
	class NodeLayout;
	typedef Ptr<NodeLayout> NodeLayoutPtr;
	typedef Ptr<const NodeLayout> NodeLayoutPtrConst;

	//
	class NodeLayoutBlocks;
	typedef Ptr<NodeLayoutBlocks> NodeLayoutBlocksPtr;
	typedef Ptr<const NodeLayoutBlocks> NodeLayoutBlocksPtrConst;

	class InputArchive;
	class OutputArchive;

	//! This node is used to define the texture layout for a texture coordinates channel of a
	//! constant mesh.
	//! \ingroup model
	class MUTABLETOOLS_API NodeLayout : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Blocks = 0,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeLayout* pNode, OutputArchive& arch );
		static NodeLayoutPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeLayout() {}

		//!
		EType Type = EType::None;

	};


	//! This node is used to define the texture layout for a texture coordinates channel of a
	//! constant mesh.
	//! The blocks defined here will be set to the channel of the mesh were this node is connected.
	//! \ingroup model
	class MUTABLETOOLS_API NodeLayoutBlocks : public NodeLayout
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeLayoutBlocks();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeLayoutBlocks* pNode, OutputArchive& arch );
		static NodeLayoutBlocksPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Set the size of the grid where the blocks will be defined
		void SetGridSize( int x, int y );

		//! Set the maximum size of the grid where the blocks can be defined
		void SetMaxGridSize(int x, int y);

		//! Get the size of the grid where the blocks will be defined
		void GetGridSize( int* pX, int* pY ) const;

		//! Get the maximum size of the grid where the blocks can be defined
		void GetMaxGridSize(int* pX, int* pY) const;

		//! Set the number of blocks in the layout.
		//! It keeps the current data as much as possible, and the new data is undefined.
		void SetBlockCount( int );

		//! Get the number of blocks in the layout
		int GetBlockCount();

		//! Set a block of the layout.
		//! minx and miny refer to the lowest left corner of the block.
        void SetBlock( int index, int minx, int miny, int sizex, int sizey );

		//! Get a block of the layout.
		//! minx and miny refer to the lowest left corner of the block.
		void GetBlock(int index, int* minx, int* miny, int* sizex, int* sizey);

		//! Set priority of a block.
		void SetBlockPriority(int index, int priority);

		//! Set the texture layout packing strategy 
		void SetLayoutPackingStrategy(EPackStrategy strategy);

		//! Generate the blocks of a layout using the UV of the meshes
		static NodeLayoutBlocksPtr GenerateLayoutBlocks(const MeshPtr pMesh, int layoutIndex, int gridSizeX, int gridSizeY);


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeLayoutBlocks();

	private:

		Private* m_pD;

	};



}
