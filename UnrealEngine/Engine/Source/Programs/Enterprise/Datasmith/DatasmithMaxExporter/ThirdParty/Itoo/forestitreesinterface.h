/**********************************************************************
FILE: itreesinterface.h
DESCRIPTION: Tree interface
CREATED BY: DQG
HISTORY:	18/07/2009- First Version
			14/10/2010- Moved to idependent file
			06/11/2011- CQ - Added fullTM access
			07/12/2011- CQ - Added updateUI
			30/01/2012- CQ - Added forest_getrenderid
			12/10/2012- CQ - Added forest_getrendernode

	Copyright (c) 2012, ITOO Software All Rights Reserved.
**********************************************************************/

#ifndef __TREESINTERFACE__H
#define __TREESINTERFACE__H

// interface ID
#define FOREST_TREE_INTERFACE Interface_ID(0x6afe7c5e, 0x15813e00)

#define GetTreesInterface(obj) ((ITreesInterface*)obj->GetInterface(FOREST_TREE_INTERFACE))

// function IDs
enum { forest_create, forest_delete, forest_edit, forest_count, forest_move, forest_rotate, forest_getposition, forest_getwidth, forest_getheight, forest_getsize,
	forest_getrotation, forest_getspecid, forest_getseed, forest_setposition, forest_setrotation, forest_setwidth, forest_setheight, forest_setsize, forest_setspecid, 
	forest_setseed, forest_update, forest_getfulltm, forest_update_ui, forest_getrenderid, forest_getrendernode};


///////////////////////////////////////////////////////////////////////////////////////////
// MaxScript Interface
///////////////////////////////////////////////////////////////////////////////////////////

class ITreesInterface : public FPMixinInterface {

	BEGIN_FUNCTION_MAP
	VFN_4(forest_create, IForestCreate, TYPE_POINT3, TYPE_FLOAT, TYPE_FLOAT, TYPE_INT);
	VFN_1(forest_delete, IForestDelete, TYPE_INT);
	RO_PROP_FN(forest_count, IForestCount, TYPE_INT);
	VFN_5(forest_edit, IForestEdit, TYPE_INT, TYPE_FLOAT, TYPE_FLOAT, TYPE_INT, TYPE_INT);
	VFN_2(forest_move, IForestMove, TYPE_INT, TYPE_POINT3);
	VFN_2(forest_setposition, IForestMove, TYPE_INT, TYPE_POINT3);
	VFN_2(forest_setrotation, IForestRotate, TYPE_INT, TYPE_POINT3);
	VFN_2(forest_setwidth, IForestSetWidth, TYPE_INT, TYPE_FLOAT);
	VFN_2(forest_setheight, IForestSetHeight, TYPE_INT, TYPE_FLOAT);
	VFN_2(forest_setsize, IForestSetSize, TYPE_INT, TYPE_POINT3);
	VFN_2(forest_setspecid, IForestSetSpecID, TYPE_INT, TYPE_INT);
	VFN_2(forest_setseed, IForestSetSeed, TYPE_INT, TYPE_INT);
	FN_1(forest_getposition, TYPE_POINT3, IForestGetPosition, TYPE_INT);
	FN_1(forest_getwidth, TYPE_FLOAT, IForestGetWidth, TYPE_INT);
	FN_1(forest_getheight, TYPE_FLOAT, IForestGetHeight, TYPE_INT);
	FN_1(forest_getsize, TYPE_POINT3, IForestGetSize, TYPE_INT);
	FN_1(forest_getspecid, TYPE_INT, IForestGetSpecID, TYPE_INT);
	FN_1(forest_getrotation, TYPE_POINT3, IForestGetRotation, TYPE_INT);
	FN_1(forest_getseed, TYPE_INT, IForestGetSeed, TYPE_INT);
	FN_1(forest_getfulltm, TYPE_MATRIX3_BV, IForestGetFullTM, TYPE_INT);
	VFN_0(forest_update, IForestUpdate);
	VFN_0(forest_update_ui, IForestUpdateUI);
	FN_3(forest_getrenderid, TYPE_BOOL, IForestGetRenderID, TYPE_INTPTR, TYPE_FLOAT_BR, TYPE_FLOAT_BR);
	FN_1(forest_getrendernode, TYPE_INODE, IForestGetRenderNode, TYPE_INT);
	END_FUNCTION_MAP

	virtual void IForestCreate(Point3 p, float width, float height, int specid) = 0;
	virtual void IForestDelete(int n) = 0;
	virtual int IForestCount() = 0;
	virtual void IForestEdit(int n, float width, float height, int specid, int seed) = 0;
	virtual void IForestMove(int n, Point3 p) = 0;
	virtual void IForestRotate(int n, Point3 r) = 0;
	virtual void IForestSetWidth(int n, float width) = 0;
	virtual void IForestSetHeight(int n, float height) = 0;
	virtual void IForestSetSize(int n, Point3 s) = 0;
	virtual void IForestSetSpecID(int, int specid) = 0;
	virtual void IForestSetSeed(int, int seed) = 0;
	virtual Point3 *IForestGetPosition(int n) = 0;
	virtual float IForestGetWidth(int n) = 0;
	virtual float IForestGetHeight(int n) = 0;
	virtual Point3 *IForestGetSize(int n) = 0;
	virtual Point3 *IForestGetRotation(int n) = 0;
	virtual int IForestGetSpecID(int n) = 0;
	virtual int IForestGetSeed(int n) = 0;
	virtual Matrix3 IForestGetFullTM(int n) = 0;
	virtual void IForestUpdate() = 0;
	virtual void IForestUpdateUI() = 0;
	virtual BOOL IForestGetRenderID(INT_PTR sc, float &itemid, float &elemid) = 0;
	virtual INode *IForestGetRenderNode(int n) = 0;

	FPInterfaceDesc* GetDesc();	
};


#endif
