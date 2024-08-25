///
/// Copyright (C) 2018 domeprojection.com GmbH.
/// All rights reserved.
/// Contact: domeprojection.com GmbH (support@domeprojection.com)
///
/// This file is part of the dpLib.
///

#ifndef _DPTYPES_H_
#define _DPTYPES_H_

#include "dpDllspec.h"
#include <stdio.h>
#include <vector>

#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)
// C++11 support
#else
// no c++11 support
#ifndef nullptr
#define nullptr NULL
#endif
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) {if((p)){(p)->Release();(p)=nullptr;}}
#endif

#ifdef __cplusplus
extern "C" {
#endif

	enum dpResult
	{
		dpNoError = 0,
		dpInvalidPointer = 1,
		dpInvalidData = 2,
		dpFileNotFound = 3,
		dpD3DError = 4,
		dpNotImplemented = 5,
        dpOpenGLError = 6,
        dpSystemError = 7,
        dpInvalidContext = 8,
		dpUnknownError
	};

	enum dpImageFileType
	{
		dpBMP = 0,
		dpJPG = 1,
		dpTGA = 2,
		dpPNG = 3,
		dpDDS = 4,
		dpPPM = 5,
		dpDIB = 6,
		dpHDR = 7,
		dpPFM = 8
	};

	enum dpCorrectionType
	{
		dpStaticCorrection = 0,
		dpDynamicCorrection = 1
	};

    enum dpCorrectionPassType
    {
        dpWarpingPass = 0,
        dpBlendingPass = 1,
        dpBlackLevelPass = 2,
        dpSecondaryBlendingPass = 3,
        dpUnknownPass
    };

    enum dpTextureTarget
    {
        dpTextureTarget2D = 0,
        dpTextureTargetRectangle = 1,
        dpTextureTargetUnknown
    };

	typedef struct dpVec3f
	{
		float x;
		float y;
		float z;
		inline dpVec3f() : x(0.0f), y(0.0f), z(0.0f) {};
		inline dpVec3f(float _x, float _y, float _z = 0.0) : x(_x), y(_y), z(_z) {};
		inline dpVec3f(float s) : x(s), y(s), z(s) {};
	} dpVec3f;

	typedef struct dpVec2f
	{
		float x;
		float y;
        inline dpVec2f() : x(0.0f), y(0.0f) {};
		inline dpVec2f(float _x, float _y) : x(_x), y(_y) {};
		inline dpVec2f(float s) : x(s), y(s) {};
	} dpVec2f;

	typedef struct dpMatrix4x4
	{
		float matrix[16];
	} dpMatrix4x4;

	typedef struct dpCamera
	{
		float tanLeft;
		float tanRight;
		float tanBottom;
		float tanTop;
		float cFar;
		float cNear;

		float fov;
		float aspect;
		dpVec2f offset;

		dpVec3f position;
		dpVec3f dir;
		dpVec3f up;
	} dpCamera;

	typedef struct dpTargetRect
	{
		dpVec3f topLeft;
		dpVec3f bottomLeft;
		dpVec3f topRight;
		dpVec3f bottomRight;

		dpVec3f dir;
		dpVec3f up;
	} dpTargetRect;

	typedef struct dpMesh
	{
		dpVec3f* vertices;
		dpVec2f* texcoords;
		unsigned int dimX;
		unsigned int dimY;
		inline dpMesh() : vertices(nullptr), texcoords(nullptr), dimX(0), dimY(0) {}
        inline dpMesh(const dpMesh& mesh)
        {
            dimX = mesh.dimX;
            dimY = mesh.dimY;
            vertices = (dpVec3f*)malloc(dimX * dimY * sizeof(dpVec3f));
            texcoords = (dpVec2f*)malloc(dimX * dimY * sizeof(dpVec2f));
			if(vertices && texcoords)
			{
				memcpy(vertices,  mesh.vertices,  dimX * dimY * sizeof(dpVec3f));
				memcpy(texcoords, mesh.texcoords, dimX * dimY * sizeof(dpVec2f));
			}
        }
		inline ~dpMesh()
		{
			free(vertices);
			free(texcoords);
		}
	} dpMesh;

	typedef struct DPLIB_API dpContext dpContext;

#ifdef __cplusplus
}
#endif

#endif // _DPTYPES_H_
