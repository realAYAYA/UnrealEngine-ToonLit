// ############################### //
// ## VIOSO Warp and Blend dll  ## //
// ## for DirectX + OpenGL      ## //
// ## author:  Juergen Krahmann ## //
// ## Use with Calibrator >=4.3 ## //
// ## Copyright VIOSO GmbH 2015 ## //
// ############################### //

#ifdef VIOSOWARPBLEND_EXPORTS
	#pragma once
#endif
#include "VWBTypes.h"
#include "VWBDef.h"

/**
@file VIOSO API main include header
@brief
Get updates from 
https://bitbucket.org/VIOSO/vioso_api

This library is meant to be used in image generators to do warping and blending. It takes a .vwf export and a texture buffer
to sample from. If no texture buffer is given, it uses a copy of the current back buffer. It will render to the currently set back buffer.
It provides image based warping, suitable for most cases and, if a 3D map is provided, dynamc eye warping.
NOTE: to build for Windows 7, #define VWB_WIN7_COMPAT

@see README.md for more
*/


	/** creates a new VIOSO Warp & Blend API instance
	* @param [IN_OPT] pDxDevice  a pointer to a DirectX device for Direct3D 9 to 11; for Direct3D 12, you need to specify a pointer to a ID3D12CommandQueue, set to NULL for OpenGL, set to VWB_DUMMYDEVICE, to just hold the data to create a textured mesh.
	* Supported: IDirect3DDevice9,IDirect3DDevice9Ex,ID3D10Device,ID3D10Device1,ID3D11Device,ID3D12CommandQueue (for ID3D12Device initialization)
	* @param [IN_OPT] szConfigFile  path to a .ini file containing settings, if NULL the default values are used
	* @param [IN_OPT] szChannelName a section name to look for in .ini-file.
	* @param [OUT] ppWarper this receives the warper
	* @param [IN_OPT] logLevel, set a log level.
	* @param [IN_OPT] logFile, set a log file.
	* @return VWB_ERROR_NONE on success, otherwise @see VWB_ERROR
	* @remarks Default settings are loaded, then, if a .ini file is given, the settings are updated.
	* If there is a [default] section, values are loaded from there, before individual settings are loaded.
	* After creating, you are able to edit all settings. You may specify a logLevel in .ini, it overwrites given loglevel.
	* You have to destroy a created warper using VWB_Destroy.
	* After successful creation, call VWB_Init, to load .vwf file and initialize the warper. */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_CreateA, ( void* pDxDevice, char const* szConfigFile, char const* szChannelName, VWB_Warper** ppWarper, VWB_int logLevel, char const* szLogFile ) );   
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_CreateW, ( void* pDxDevice, wchar_t const* szConfigFile, wchar_t const* szChannelName, VWB_Warper** ppWarper, VWB_int logLevel, wchar_t const* szLogFile ) );   
#ifdef UNICODE
#define VWB_Create VWB_CreateW
#else //def UNICODE
#define VWB_Create VWB_CreateA
#endif //def UNICODE

	/** destroys a warper
	* @param pWarper	a warper to be destroyed */
	VIOSOWARPBLEND_API( void, VWB_Destroy, ( VWB_Warper* pWarper ));

	/** initializes a warper
	* @param pWarper	a warper 
	* @param extSet  	a warpBlendSet, loaded or created by host application
	* @remarks In case of an OpenGL warper, the later used context needs to be active. There is no multi-threading on side of VIOSO API. */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_Init, ( VWB_Warper* pWarper ));
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_InitExt, ( VWB_Warper* pWarper, VWB_WarpBlendSet* extSet ));

    /** set the new dynamic eye position and direction
	* @param [IN]			pWarper	a valid warper
    * @param [IN,OUT,OPT]	pEye	sets the new eye position, if a eye point provider is present, the value is getted from there, pEye is updated
    * @param [IN,OUT,OPT]	pRot	the new rotation in radian, if a eye point provider is present, the value is getted from there, pRot is updated
    * @param [OUT]			pView	it gets the updated view matrix to translate and rotate into the viewer's perspective
	* @param [OUT]			pProj	it gets the updated projection matrix
	* @param [OUT]			pClip	it gets the updated clip planes: left, top, right, bottom, near, far, where all components are usually positive
	* @param [OUT]			pPos    it gets the updated relative position: x,y,z
	* @param [OUT]			pDir	it gets the updated relative direction: euler angles around x,y,z rotation order is y,x,z
	* @param [IN,OPT]		symmetric set to true, to force symmetric frustum, NOTE: symmetric frusta are worse in quality, as the image plane is no longer parallel to the screen plane
	* @param [IN,OPT]		aspect	set to some ratio, to force clip planes to form a rect with this ratio, if set to 0, optimal aspect ratio is used. NOTE: Non-optimal aspect ratia are worse in quality, as part of the input image is rendered but not used.
	* @return VWB_ERROR_NONE on success, VWB_ERROR_GENERIC otherwise
	* @remarks If EyePointProvider is used, the eye point is set by calling it's getEye function. eye and rot are set to that if not NULL.
	* Else, if eye and rot are not NULL, values taken from here.
	* Else the eye and rot are set to 0-vectors.
	* The internal view and projection matrices are calculated to render. You should set pView and pProj to get these matrices for rendering, if updated.
	* positive rotation means turning right, up and clockwise.
	* Calculate view Matrix from pDir and pPos for left handed direct X
	XMMATRIX R = XMMatrixRotationRollPitchYaw( -pDir[0], pDir[1], pDir[2] );
	XMMATRIX T = XMMatrixTranslation( pPos[0], pPos[1], pPos[2] );
	XMMATRIX V = XMMatrixMultiply( R, T );
	* for right handed openGL
	V = glm::transpose( glm::yawPitchRoll( -dir[1],  dir[0], -dir[2] ) );
	V[0].w = pos[0];
	V[1].w = pos[1];
	V[2].w = pos[2];

	Use pClip this way to get the same matrix like from VWB_getViewProjection in left handed DX:
	P = XMMatrixPerspectiveOffCenterLH( -pClip[0], pClip[2], -pClip[3], pClip[1], pClip[4], pClip[5] );
	* for right handed openGL
	P = glm::frustum( -pClip[0], pClip[2], -pClip[3], pClip[1], pClip[4], pClip[5] );
	*/
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getViewProj, ( VWB_Warper* pWarper, VWB_float* pEye, VWB_float* pRot, VWB_float* pView, VWB_float* pProj ) );
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getViewClip, ( VWB_Warper* pWarper, VWB_float* pEye, VWB_float* pRot, VWB_float* pView, VWB_float* pClip ) );
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getPosDirClip, ( VWB_Warper* pWarper, VWB_float* pEye, VWB_float* pRot, VWB_float* pPos, VWB_float* pDir, VWB_float* pClip, bool symmetric, VWB_float aspect ) );


	/** query the corners of the screen plane
	* @param [IN]			pWarper	a valid warper 
	* @param [OUT]			pTL	it gets the top-left corner
	* @param [OUT]			pTR	it gets the top-right corner
	* @param [OUT]			pBL	it gets the bottom-left corner
	* @param [OUT]			pBR	it gets the bottom-right corner
	* @return VWB_ERROR_NONE on success, VWB_ERROR_GENERIC otherwise 
	NOTE: This is static to a mapping, just call once a session. */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getScreenplane, ( VWB_Warper* pWarper, VWB_float* pTL, VWB_float* pTR, VWB_float* pBL, VWB_float* pBR ) );

	/** set the view and projection matrix directly
	* @param [IN]			pWarper	a valid warper
    * @param [IN]			pView	view matrix to translate and rotate into the viewer's perspective
    * @param [IN]			pProj	projection matrix
    * @return VWB_ERROR_NONE on success, VWB_ERROR_GENERIC otherwise 
	* @remarks
	* The screen is a fixed installation. It does not change with the view direction. 
	* Only position tracking affects transformation, view direction might add rotated IPD/2 for 3D stereo.  */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_setViewProj, ( VWB_Warper* pWarper, VWB_float* pView,  VWB_float* pProj) );

    /** render a warped and blended source texture into the current back buffer
	* @remark for DirectX 9, call inside BeginScene/EndScene statements
	* @param [IN]			pWarper	a valid warper
    * @param [IN,OPT]		pSrc    the source texture, a IDirect3DTexture9*, ID3D10Texture2D*, ID3D11Texture2D*, VWB_D3D12_RENDERINPUT* or a GLint texture index; 
	* if current backbuffer must be read, set to NULL in any DX mode except 12 or to -1 in OpenGL mode
	* in case of directX 12 you need to provide a @see VWB_D3D12_RENDERINPUT as parameter.
	* @param [IN,OPT]		stateMask @see VWB_STATEMASK enumeration, default is 0 to restore usual stuff
	* In D3D12 all flags except VWB_STATEMASK_CLEARBACKBUFFER are ignored.
	* The application is required to set inputs and shader in each term anyway.
	* @return VWB_ERROR_NONE on success, VWB_ERROR_GENERIC otherwise */
    VIOSOWARPBLEND_API( VWB_ERROR, VWB_render, ( VWB_Warper* pWarper, VWB_param src, VWB_uint stateMask ) );  

	/** get info about .vwf, reads all warp headers
	* @param [IN]			path	the file name or a comma separated list of filenames, set to NULL to release data from a previous set
	* @param [OUT]			set		a warp blend header set
	* @param [OUT|OPT]		headers	a pointer to an array of VWB_WarpBlendHeader
	* @param [IN|OUT]		count   the number of array elements provided, if headers is NULL, it is set to the number of headers needed
	* @return VWB_ERROR_NONE on success, VWB_ERROR_PARAM if fname is not set or empty or count is NULL, VWB_ERROR_FALSE if *count is less than the number of headers needed, VWB_ERROR_VWF_FILE_NOT_FOUND if path did not resolve, VWB_ERROR_GENERIC otherwise
	* @remarks The list is emptied and all found headers are appended. You need to call VWB_vwfInfo( NULL, set ) to release memory from within the warper-dll.  */
	#ifdef __cplusplus
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_vwfInfo, ( char const* path, VWB_WarpBlendHeaderSet* set ) );
	#endif
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_vwfInfoC, ( char const* path, VWB_WarpBlendHeader* headers, VWB_uint* count ) );

	/** fills a VWB_WarpBlend from currently loaded data. Warper needs to be initialized as VWB_DUMMYDEVICE.
	* @param [IN]			pWarper	a valid warper
	* @param [OUT]			mesh	the resulting mesh, the mesh will be emptied before filled
	* @return VWB_ERROR_NONE on success, VWB_ERROR_PARAMETER, if parameters are out of range, VWB_ERROR_GENERIC otherwise */
	#ifdef __cplusplus
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getWarpBlend, ( VWB_Warper* pWarper, VWB_WarpBlend const*& wb ) );
	#endif
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getWarpBlendC, ( VWB_Warper* pWarper, VWB_WarpBlend const** wb ) );

	/** fills a float[16] with the currently set internally used matrix for render shader. Warper needs to be initialized as VWB_DUMMYDEVICE.
	* @param [IN]			pWarper	a valid warper
	* @param [OUT]			pMPV	the internal view-projection matrix
	* @return VWB_ERROR_NONE on success, VWB_ERROR_PARAMETER, if parameters are out of range, VWB_ERROR_GENERIC otherwise */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getShaderVPMatrix, ( VWB_Warper* pWarper, VWB_float* pMPV ) );

	/** fills a VWB_WarpBlendMesh from currently loaded data. It uses cols * rows vertices or less depending on how .vwf is filled. Warper needs to be initialized as VWB_DUMMYDEVICE.
	* @param [IN]			pWarper	a valid warper
	* @param [IN]			cols	sets the number of columns
	* @param [IN]			rows	sets the number of rows
	* @param [OUT]			mesh	the resulting mesh, the mesh will be emptied before filled
	* @return VWB_ERROR_NONE on success, VWB_ERROR_PARAMETER, if parameters are out of range, VWB_ERROR_GENERIC otherwise */
	#ifdef __cplusplus
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getWarpBlendMesh, ( VWB_Warper* pWarper, VWB_int cols, VWB_int rows, VWB_WarpBlendMesh& mesh ) );
	#endif
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getWarpBlendMeshC, ( VWB_Warper* pWarper, VWB_int cols, VWB_int rows, VWB_WarpBlendMesh* mesh ) );

	/** destroys a VWB_WarpBlendMesh .
	 * @param [IN]			pWarper	a valid warper
	 * @param [INOUT]		mesh	the mesh to be destroyed
	 * @return VWB_ERROR_NONE on success, VWB_ERROR_PARAMETER, if parameters are out of range, VWB_ERROR_GENERIC otherwise */
	#ifdef __cplusplus
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_destroyWarpBlendMesh, ( VWB_Warper* pWarper, VWB_WarpBlendMesh& mesh ) );
	#endif
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_destroyWarpBlendMeshC, ( VWB_Warper* pWarper, VWB_WarpBlendMesh* mesh ) );

	/* log some string to the API's log file, exposed version; use VWB_logString instead.
	* @param [IN]			level	a level indicator. The string is only written to log file, if this is lower or equal to currently set global log level
	* @param [IN]			str		a null terminated multibyte character string
	* @return VWB_ERROR_NONE on success, VWB_ERROR_PARAMETER otherwise */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB__logString, ( VWB_int level, char const* str ) );

	/* get the version of the API
	* @param[OUT]			major	major version
	* @param[OUT]			minor	minor version
	* @param[OUT]			maintenance	maintenance revision
	* @param[OUT]			build	build number
	* @return VWB_ERROR_NONE on success, VWB_ERROR_PARAMETER, if parameters are out of range */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_getVersion, ( VWB_int* major, VWB_int* minor, VWB_int* maintenance, VWB_int* build ) );

	/* get set a 128bit AES key for encryption and decryption of mappings
	* @param[IN]			key	    a key of 16 bytes (128 bit) provided for encryption/decryption of AES128 algorythm loading and saving vwf files, set to nullptr to disable encryption/decryption
	* @return VWB_ERROR_NONE on success, VWB_ERROR_PARAMETER, if parameters are out of range 
	* @remark Internally we keep only the pointer to your buffer. Make sure to keep the key there as long as it is needed. It will be used during VWB_init VWB_initExt */
	VIOSOWARPBLEND_API( VWB_ERROR, VWB_setCryptoKey, ( uint8_t const* key ) );

	#undef VIOSOWARPBLEND_API