#ifndef __VWB_SMARTPROJECTOR_DEVELOPMENT_FILE_DECLARATIONS__
#define __VWB_SMARTPROJECTOR_DEVELOPMENT_FILE_DECLARATIONS__
#include <stdint.h>
#include <vector>

/// Type definitions
typedef void* VWB_param;	/// A versatile type to transfer an object pointer or integral number

#ifdef WIN32

typedef wchar_t VWB_wchar;	/// a 16bit unsignd integer

#ifndef MAX_PATH
#define MAX_PATH 260		/// define a maximum file path length, if not done by the OS SDK
#else

#if (MAX_PATH != 260)
#error MAX_PATH define mismatch
#endif

#endif //ndef MAX_PATH

#else

typedef int16_t VWB_wchar;	/// a 16bit unsignd integer
#define MAX_PATH 32768		/// define a maximum file path length, if not done by the OS SDK

#endif

typedef uint8_t VWB_byte;	/// a 8bit unsignd integer
typedef uint16_t VWB_word;	/// a 16bit unsignd integer
typedef int32_t VWB_int;	/// a 32bit signed integer
typedef uint32_t VWB_uint;	/// a 32bit unsigned integer
typedef int64_t VWB_ll;	/// a 64bit signed integer
typedef uint64_t VWB_ull; /// a 64bit unsigned integer

typedef float VWB_float;	/// a single precision floating point number
typedef double VWB_double;	/// a double precision floating point number

#pragma pack( push, 4 )
typedef struct VWB_size { VWB_int cx, cy; } VWB_size; /// a size
typedef struct VWB_rect { VWB_int left, top, right, bottom; } VWB_rect; /// a size

#define VWB_DUMMYDEVICE ((void*)1)
#define VWB_UNDEFINED_GL_TEXTURE ((void*)-1)
/** Error enumerator */
typedef enum VWB_ERROR
{
	VWB_ERROR_NONE = 0,			/// No error, we succeeded
	VWB_ERROR_GENERIC = -1,		/// a generic error, this might be anything, check log file
	VWB_ERROR_PARAMETER = -2,	/// a parameter error, provided parameter are missing or inappropriate
	VWB_ERROR_INI_LOAD = -3,	/// ini could notbe loaded
	VWB_ERROR_BLEND = -4,		/// blend invalid or coud not be loaded to graphic hardware, check log file
	VWB_ERROR_WARP = -5,		/// warp invalid or could not be loaded to graphic hardware, check log file
	VWB_ERROR_SHADER = -6,		/// shader program failed to load, usually because of not supported hardware, check log file
	VWB_ERROR_VWF_LOAD = -7,	/// mappings file broken or version mismatch
	VWB_ERROR_VWF_FILE_NOT_FOUND = -8, /// cannot find mapping file
	VWB_ERROR_NOT_IMPLEMENTED = -9,		/// Not implemented, this function is yet to come
	VWB_ERROR_NETWORK = -10,		/// Network could not be initialized
	VWB_ERROR_FALSE = -16,		/// No error, but nothing has been done
} VWB_ERROR;

typedef enum VWB_STATEMASK
{   
	///< mask to capture and restore pipeline states and settings. All things are set by the warper
	///< consider not restoring things you set each frame anyway
	VWB_STATEMASK_STANDARD = 0, ///< VWB_STATEMASK_VERTEX_BUFFER | VWB_STATEMASK_INPUT_LAYOUT | VWB_STATEMASK_PRIMITIVE_TOPOLOGY | VWB_STATEMASK_RASTERSTATE
	VWB_STATEMASK_NONE = 0x10000000, // don't restore anything
	VWB_STATEMASK_VERTEX_BUFFER = 0x00000001, // vertex buffer slot 0
	VWB_STATEMASK_INPUT_LAYOUT = 0x00000002, // input layout 
	VWB_STATEMASK_PRIMITIVE_TOPOLOGY = 0x00000004, // primitive topology, we use PRIMITIVE_TOPOLOGY_TRIANGLELIST
	VWB_STATEMASK_VERTEX_SHADER = 0x00000008, // vertex shader 
	VWB_STATEMASK_RASTERSTATE = 0x00000020, // raster state / state block
	VWB_STATEMASK_CONSTANT_BUFFER = 0x00000040, // constant buffer or constant registers c0 to c5
	VWB_STATEMASK_PIXEL_SHADER = 0x00000080, // pixel shader
	VWB_STATEMASK_SHADER_RESOURCE = 0x00000100, // shader resource 0 to 2 or texture register t0 to t2
	VWB_STATEMASK_SAMPLER = 0x00001000, // Sampler state resource 0 to 2 in DX10 and later, DX9 sampler state is captured in state block
	VWB_STATEMASK_CLEARBACKBUFFER = 0x00002000, // set to clear backbuffer
	VWB_STATEMASK_ALL = 0x1FFFFFFF, // All
	VWB_STATEMASK_DEFAULT = VWB_STATEMASK_VERTEX_BUFFER | VWB_STATEMASK_INPUT_LAYOUT | VWB_STATEMASK_PRIMITIVE_TOPOLOGY | VWB_STATEMASK_RASTERSTATE,
	VWB_STATEMASK_DEFAULT_D3D12 = 0
} VWB_STATEMASK;

/** The warper
* @remarks All changes you do before calling VWB_Init are considered
*/
struct VWB_Warper
{
	/// this is where this ini file is, this defaults to ""
	char	path[MAX_PATH];

	/// this is the channel name, it defaults to "channel 1"
	char	channel[MAX_PATH];

	/// this is the path to the warp map, a .vwf file, it defaults to "vioso.vwf"
	char	calibFile[12 * MAX_PATH];
	
	/// the calibration index in mapping file, defaults to 0,
	/// you also might set this to negated display number, to search for a certain display:
	/// 
	VWB_int		calibIndex;

	/// set to true to make the world turn and move with view direction and eye position, this is the case if the viewer gets
	/// moved by a motion platform, defaults to false
	bool		bTurnWithView;

	/// set to true to render without blending enabled, defaults to false
	bool		bDoNotBlend;

	/// a path to a external dynamic library to provide the eye parameters, see EyePointProvider.h
	char	eyeProvider[MAX_PATH];
	/// a parameter string, to initialize the eye-point provider
	char	eyeProviderParam[32768];

	/// eye to pivot position coordinate, usually {0,0,0}, defaults to {0,0,0}
	VWB_float   eye[3];

	/// the near plane distance
	/// NOTE: the corresponding .ini-value is "near", defaults to 0.1
	VWB_float	nearDist;

	/// the far plane distance, note: these values are used to create the projection matrix
	/// farDist/nearDist should be as small as possible, to have a good z-buffer resolution
	/// defaults to 200.0
	/// NOTE: the corresponding .ini-value is "far"
	VWB_float	farDist;

	/// set to true to enable bicubic sampling from source texture, defaults to false
	bool		bBicubic;

	/// swivel and negate eye parameters, defaults to 0
	/// bitfield:	0x00000001 change sign of pitch, 0x00000002 use input yaw as pitch, 0x00000004 use input roll as pitch
	///				0x00000010 change sign of yaw, 0x00000020 use input pitch as yaw, 0x00000040 use input roll as yaw
	///				0x00000100 change sign of roll, 0x00000200 use input pitch as roll, 0x00000400 use input yaw as roll
	///				0x00010000 change sign of x movement, 0x00020000 use input y as x, 0x00040000 use input z as x
	///				0x00100000 change sign of y movement, 0x00200000 use input x as y, 0x00400000 use input z as y
	///				0x01000000 change sign of z movement, 0x02000000 use input x as z, 0x04000000 use input y as z
	VWB_int		splice;

	/// the transformation matrix to go from VIOSO coordinates to IG coordinates, defaults to indentity
	/// note VIOSO maps are always right-handed, to use with a left-handed world like DirectX, invert the z!
	VWB_float	trans[16];

	/// set a gamma correction value. This is only useful, if you changed the projector's gamma setting after calibration,
	/// as the gamma is already calculated inside blend map, or to fine-tune, defaults to 1 (no change)
	VWB_float	gamma;

	/// set a moving range. This applies only for 3D mappings and dynamic eye point.
	/// This is a factor applied to the projector mapped MIN(width,height)/2
	/// The view plane is widened to cope with a movement to all sides, defaults to 1
	/// Check borderFit in log: 1 means all points stay on unwidened viewplane, 2 means, we had to double it.
	VWB_float	autoViewC;

	/// set to true to calculate view parameters while creating warper, defaults to false
	/// All further values are calculated/overwritten, if bAutoView is set.
	bool		bAutoView;

	/// [0] = x = pitch, [1] = y = yaw, [2] = z = roll, rotation order is yaw first, then pitch, last roll, defaults to {0,0,0}
	/// positive yaw turns right, positive pitch turns up and positive roll turns clockwise
	VWB_float	dir[3];

	/// the fields of view in degree, [0] = left, [1] = top, [2] = right, [3] = bottom, defaults to {35,30,35,30}
	VWB_float	fov[4];

	/// the screen distance this is where the render plane is, defaults to 1
	VWB_float	screenDist;

	/// set to some value, this is returned by VWB_GetOptimalRes, defaults to {0,0}, set by VWB_AutoView to have a projector pixel to content pixel ratio of 1
	VWB_size	optimalRes;

	/// set to some value, this is returned by VWB_GetOptimalRes, defaults to {0,0,0,0}, set by VWB_AutoView to indicate, only a parttial rect of the whole image is needed
	VWB_rect	optimalRect;

	// set TCP port the plugin is listening to, default is 0 which means network support is switched off
	VWB_word	port;	

	/// set to IPv4 address to listen on a specific one, defaults to "0.0.0.0", which means we listen to all local IPs
	char		addr[MAX_PATH];

	/// set to true to use OpenGL shader version 1.1 with fixed pipeline, defaults to false
	bool		bUseGL110;

	/// set to true if your input texture is only the optimal rect part, defaults to false
	bool		bPartialInput;

	/// Bitfield; only valid in Windows build
	/// 1 rendering of mouse cursor
	/// 2 disable system cursor over window 
	VWB_int		mouseMode;

	/// set to true to flip directX 11 texture v, defaults to false
	bool		bFlipDXVs;

	/// set to true to disable black level offset
	bool        bDoNoBlack;
};
#pragma pack(pop)
// ----------------------------------------------------------------------------------
//                               constant declarations
// ----------------------------------------------------------------------------------

typedef enum FLAG_WARPFILE_HEADER
{
	FLAG_WARPFILE_HEADER_NONE=0x0,										///<   no flags set
	FLAG_WARPFILE_HEADER_DATA_ZIP=0x1,									///<   zip compressed data
	FLAG_WARPFILE_HEADER_DISPLAY_SPLIT=0x2,								///<   data describes a splited display, reserved[0..5] contains splits position ( iRow, iCol, qRow, qCol, org. Display width, org. Display height)
	FLAG_WARPFILE_HEADER_CALIBRATION_BASE_TYP=0x4,						///<   the calibration typ is specified, reserved[6] @see ESPCalibrationBaseTyp for details
	FLAG_WARPFILE_HEADER_BORDER=0x8,										///<   border was set; the content is boxed by 1%
	FLAG_WARPFILE_HEADER_OFFSET=0x10,									///<   desktop monitor offset is valid in reserved[7] reserved[8]
	FLAG_WARPFILE_HEADER_BLACKLEVEL_CORR=0x20,							///<   the blacklevel correction values are set
	FLAG_WARPFILE_HEADER_3D=0x40,										///<   the warp definition contains 3D points instead of uv-mapping
	FLAG_WARPFILE_HEADER_DISPLAYID=0x80,									///<   the displayID of that screen is valid
	FLAG_WARPFILE_HEADER_BLENDV2=0x100,									///<   we are using VWB_BlendRecord2
	FLAG_WARPFILE_HEADER_BLENDV3=0x200,									///<   we are using VWB_BlendRecord3
	FLAG_WARPFILE_HEADER_ALL=											///<   all available flags
	FLAG_WARPFILE_HEADER_OFFSET |
	FLAG_WARPFILE_HEADER_BORDER |
	FLAG_WARPFILE_HEADER_DATA_ZIP | 
	FLAG_WARPFILE_HEADER_DISPLAY_SPLIT | 
	FLAG_WARPFILE_HEADER_CALIBRATION_BASE_TYP | 
	FLAG_WARPFILE_HEADER_BLACKLEVEL_CORR|
	FLAG_WARPFILE_HEADER_3D |
	FLAG_WARPFILE_HEADER_DISPLAYID |
	FLAG_WARPFILE_HEADER_BLENDV2 |
	FLAG_WARPFILE_HEADER_BLENDV3
}FLAG_WARPFILE_HEADER;


typedef enum VWB_TYPE_CALIBBASE
{
	TYP_CALIBBASE_UNSPECIFIC=0,											///<   not specified
	TYP_CALIBBASE_SINGLE_DISPLAY,										///<   warp/blend information based on a single display calibration
	TYP_CALIBBASE_DISPLAY_COMPOUND,										///<   warp/blend information based on a display compound calibration
	TYP_CALIBBASE_SUPER_COMPOUND,										///<   warp/blend information based on a super compound calibration
	TYP_CALIBBASE_UNDEF													///<   undefined
}VWB_TYPE_CALIBBASE;

#pragma pack( push, 4) // 32 bit aligned on all plattforms

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------
//                                                              VWB_WarpSetFileHeader
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------

/* A data block can be identified by its leading magic number.
A datablock can be (so far):
- Vioso warp file  ... mapping file see VWB_WarpFileHeader or VWB_WarpFileHeader2
- windows bitmap  ... blend information, see BITMAPFILEHEADER, BITMAPINFO 
If present, a blend data has to follow immediately after a warp file,
any blend file following a warp file is associated to that monitor. After a blend
there also might be a black (blacklevel correction) image and after that a white (color correction) image.
Other blend data are ignored.
*/

/** VWB_WarpSetFileHeader\n
*  ( Struct SmartProjector Warp Set File Header)\n
*  Container to preface a set of warping/blending informations.
* @author Johannes Mueller
* @author Juergen Krahmann
* @date Apr. 12
* @version 1.0
* @bug No.
* @todo Nothing. */
typedef struct VWB_WarpSetFileHeader
{
public:

	// ----------------------------------------------------------------------------------
	//                               public attributes
	// ----------------------------------------------------------------------------------

	char                                magicNumber[4];							///<   "vwf1"
	VWB_uint                            numBlocks;								///<   number of warp and blend files; the blend files are standard windows bitmaps; the 
	VWB_uint                            offs;									///<   offset of the first data block from the beginning of the file ( = 16 )
	VWB_uint                            reserved;								///<   0;

}VWB_WarpSetFileHeader;

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------
//                                                              VWB_WarpFileHeader
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------

/** VWB_WarpFileHeader\n
*  ( Struct SmartProjector Warp File Header)\n
*  Container to preface warping information.
* @author Johannes Mueller
* @author Juergen Krahmann
* @date Apr. 12
* @version 1.0
* @bug No.
* @todo Nothing. */
typedef struct VWB_WarpFileHeader
{
public:

	// ----------------------------------------------------------------------------------
	//                               public attributes
	// ----------------------------------------------------------------------------------

	char                                magicNumber[4];							///<   "vwf0"
	VWB_uint                            szHdr;									///<   used to communicate the size of this header struct
	VWB_uint                            flags;									///<   additional informations
	///<  @see ESPWarpFileHeaderFlag for details
	VWB_uint                            hMonitor;								///<   set to the HMONITOR of the treated display
	VWB_uint                            size;									///<   actual size of the following data block; the size of the raw data can be calculated from dimensions
	VWB_int                             width;									///<   count of warp records per row
	VWB_int                             height;									///<   count of rows of warp records
	VWB_float                           white[4];								///<   white point of that projector; set to { 1.0f, 1.0f, 1.0f, 1.0f }
	VWB_float                           black[4];								///<   black point of that projector; set to { 0.0f, 0.0f, 0.0f, 1.0f }
	VWB_float                           reserved[16];							///<   used to define additional informations
	///<   in case of split display
	///<   [ 0] => row index
	///<   [ 1] => column index
	///<   [ 2] => row quantum
	///<   [ 3] => column quantum
	///<   [ 4] => original display width
	///<   [ 5] => original display height
	///<   [ 6] => type to define the calibration type the information based on
	///<   [ 7] => original desktop display offset x
	///<   [ 8] => original desktop display offset y
	///<   [ 9] => blacklevel correction factor
	///<   [10] => blacklevel dark value maintain factor
	///<   [11] => blacklevel bright value maintain factor
	///<   [12] => ///<   identifier for a compound display, static cast to int, set if greater than 0, all screend/displays with same compound id should use same content space alas source rect
	char                                name[256];			///<   optional, human readable name for that mapping

}VWB_WarpFileHeader;

/** VWB_WarpFileHeader2\n
*  ( Struct SmartProjector Warp File Header version 2)\n
*  Container to preface warping information.
* magicNumber[4]="vwf0"
* @author Johannes Mueller
* @author Juergen Krahmann
* @date May 13
* @version 1.0
* @bug No.
* @todo Nothing. */
typedef struct VWB_WarpFileHeader2 : public VWB_WarpFileHeader
{
public:

	// ----------------------------------------------------------------------------------
	//                               public attributes
	// ----------------------------------------------------------------------------------

	char								ident[4096];		///<   optional, xml identification for that mapping derived from pdi code from pictureall, only filled if nvapi is appliable
	VWB_ull			                    tmIdent;								///<   !! has to be the last parameter !! used to communicate a time stamp to identify the warp information

}VWB_WarpFileHeader2;

/** VWB_WarpFileHeader3\n
*  ( Struct SmartProjector Warp File Header version 3)\n
*  Container to preface warping information.
* magicNumber[4]="vwf0"
* @author Johannes Mueller
* @date Aug 15
* @version 1.0
* @bug No.
* @todo Nothing. */
typedef struct VWB_WarpFileHeader3 : public VWB_WarpFileHeader2
{
public:

	// ----------------------------------------------------------------------------------
	//                               public attributes
	// ----------------------------------------------------------------------------------

	VWB_float                               vCntDispPx[7];							///<   optional, used to store informations about the content position on handled display
	///<   [0] => minimum covered display pixel column  (l)
	///<   [1] => minimum covered display pixel row     (t)
	///<   [2] => maximum covered display pixel column  (r)
	///<   [3] => maximum covered display pixel row     (b)
	///<   [4] => content to display pixel ratio in x direction
	///<   [5] => content to display pixel ratio in y direction
	///<   [6] => quantum of used display pixel
	VWB_float                               vPartialCnt[9];							
	///<   [0] => minimum relative content position in x direction (l)
	///<   [1] => minimum relative content position in y direction (t)
	///<   [2] => maximum relative content position in x direction (r)
	///<   [3] => maximum relative content position in y direction (b)
	///<   [4] => optional aspect ratio of the content space
	///<   [5] => optional relative content position transform offset in x direction
	///<   [6] => optional relative content position transform offset in y direction
	///<   [7] => optional relative content position transform scale in x direction
	///<   [8] => optional relative content position transform scale in y direction
	char									primName[256];		///<   optional, human readable name for high level calibration the display is assigned to
	VWB_float								vReserved2[16];		///<   used to define additional informations in further versions

	VWB_wchar								displayID[256];		///<   Windows display identifier, use EnumDisplayDevices using EDD_GET_DEVICE_INTERFACE_NAME flag to find it
	char									hostname[256];	///<   network name or IP in dotted decimal

}VWB_WarpFileHeader3;

/** VWB_WarpFileHeader4\n
*  ( Struct SmartProjector Warp File Header version 3)\n
*  Container to preface warping information.
* same as VWB_WarpFileHeader3, but with some reserved(s) unwrapped
* magicNumber[4]="vwf0"
* @author Johannes Mueller
* @author Juergen Krahmann
* @date Nov 16
* @version 1.0
* @bug No.
* @todo Nothing. */
typedef struct VWB_WarpFileHeader4
{
public:

	// ----------------------------------------------------------------------------------
	//                               public attributes
	// ----------------------------------------------------------------------------------

	char                                magicNumber[4];							///<   "vwf0"
	VWB_uint                            szHdr;									///<   used to communicate the size of this header struct
	VWB_uint                            flags;									///<   additional informations
	///<  @see ESPWarpFileHeaderFlag for details
	VWB_uint                            hMonitor;								///<   set to the HMONITOR of the treated display
	VWB_uint                            size;									///<   actual size of the following data block; the size of the raw data can be calculated from dimensions
	VWB_int                             width;									///<   count of warp records per row
	VWB_int                             height;									///<   count of rows of warp records
	VWB_float                           white[4];								///<   white point of that projector; set to { 1.0f, 1.0f, 1.0f, 1.0f }
	VWB_float                           black[4];								///<   black point of that projector; set to { 0.0f, 0.0f, 0.0f, 1.0f }
	VWB_float                           splitRowIndex;							///<   [ 0] => row index
	VWB_float                           splitColumnIndex;						///<   [ 1] => column index
	VWB_float							splitRows;								///<   [ 2] => number of rows
	VWB_float							splitColumns;							///<   [ 3] => number of columns
	VWB_float							splitTotalWidth;						///<   [ 4] => original display width
	VWB_float							splitTotalHeight;						///<   [ 5] => original display height
	VWB_float							typeCalib;								///<   [ 6] => type to define the calibration type the information based on
	VWB_float							offsetX;								///<   [ 7] => original desktop display offset x
	VWB_float							offsetY;								///<   [ 8] => original desktop display offset y
	VWB_float							blackScale;								///<   [ 9] => blacklevel correction texture scale factor
	VWB_float							blackDark;								///<   [10] => blacklevel dark value maintain factor; 
	VWB_float							blackBright;							///<   [11] => blacklevel bright value maintain factor
	VWB_float							compoundID;								///<   [12] => ///<   identifier for a compound display, static cast to int, set if greater than 0, all screend/displays with same compound id should use same content space alas source rect
	VWB_float							vReserved[3];							///<	reserved for future use
	char                                name[256];								///<   optional, human readable name for that mapping
	char								ident[4096];							///<   optional, xml identification for that mapping derived from pdi code from pictureall, only filled if nvapi is appliable
	VWB_ull			                    tmIdent;								///<   used to communicate a time stamp to identify the warp information
	VWB_float                           vCntDispPx[7];							///<   optional, used to store informations about the content position on handled display
	///<   [0] => minimum covered display pixel column  (l)
	///<   [1] => minimum covered display pixel row     (t)
	///<   [2] => maximum covered display pixel column  (r)
	///<   [3] => maximum covered display pixel row     (b)
	///<   [4] => content to display pixel ratio in x direction
	///<   [5] => content to display pixel ratio in y direction
	///<   [6] => quantum of used display pixel
	VWB_float                           vPartialCnt[9];							
	///<   [0] => minimum relative content position in x direction (l)
	///<   [1] => minimum relative content position in y direction (t)
	///<   [2] => maximum relative content position in x direction (r)
	///<   [3] => maximum relative content position in y direction (b)
	///<   [4] => optional aspect ratio of the content space
	///<   [5] => optional relative content position transform offset in x direction
	///<   [6] => optional relative content position transform offset in y direction
	///<   [7] => optional relative content position transform scale in x direction
	///<   [8] => optional relative content position transform scale in y direction
	char                                primName[256];		///<   optional, human readable name for high level calibration the display is assigned to
	VWB_float                           vReserved2[16];							///<   used to define additional informations in further versions
	VWB_wchar							displayID[256];	///<   Windows display identifier, use EnumDisplayDevices using EDD_GET_DEVICE_INTERFACE_NAME flag to find it
	char								hostname[256];	///<   network name or IP in dotted decimal
}VWB_WarpFileHeader4;

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------
//                                                              VWB_WarpRecord
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------

/** VWB_WarpRecord\n
*  ( Struct SmartProjector Warp Record)\n
*  Container to hold informations about warp record.
* @author Johannes Mueller
* @author Juergen Krahmann
* @date Apr. 12
* @version 2.0
* @bug No.
* @todo Nothing. */
typedef struct VWB_WarpRecord
{
public:

	// ----------------------------------------------------------------------------------
	//                               public attributes
	// ----------------------------------------------------------------------------------

	VWB_float                           x;										///<   if FLAG_SP_WARPFILE_HEADER_3D is set the x coordinate, else this is u in case of prewarped content in normalized	coordinates
	VWB_float                           y;										///<   if FLAG_SP_WARPFILE_HEADER_3D is set the y coordinate, else this is v in case of prewarped content in normalized	coordinates
	VWB_float                           z;										///<   if FLAG_SP_WARPFILE_HEADER_3D is set the z coordinate, else 1 indicates valid coordinate if prewarped
	VWB_float                           w;										///<   if FLAG_SP_WARPFILE_HEADER_3D is set 1 indicates valid coordinate 

}VWB_WarpRecord;

/** VWB_BlendRecord\n
*  ( Struct SmartProjector Blend Record)\n
*  Container to hold informations about blending.
* @author Johannes Mueller
* @author Juergen Krahmann
* @date Jun. 13
* @version 1.0
* @bug No.
* @todo Nothing. */
#pragma pack( push, 1 )
typedef struct VWB_BlendRecord {
	VWB_byte                               r;										///< r blend factor
	VWB_byte                               g;										///< g blend factor
	VWB_byte                               b;										///< b blend factor
	VWB_byte                               a;										///< alpha mask; is multipied to all after
} VWB_BlendRecord;
typedef struct VWB_BlendRecord2{
	VWB_word                               r;										///< r blend factor
	VWB_word                               g;										///< g blend factor
	VWB_word                               b;										///< b blend factor
	VWB_word                               a;										///< alpha mask; is multipied to all after
} VWB_BlendRecord2;
typedef struct VWB_BlendRecord3{
	VWB_float                               r;										///< r blend factor
	VWB_float                               g;										///< g blend factor
	VWB_float                               b;										///< b blend factor
	VWB_float                               a;										///< alpha mask; is multipied to all after
} VWB_BlendRecord3;

#ifndef WIN32
#ifndef VIOSOWARPBLEND_DO_DEFINE_BITMAP_INFO
typedef struct BITMAPFILEHEADER{
	VWB_word	bfType;
	VWB_uint	bfSize;
	VWB_word	bfReserved1;
	VWB_word	bfReserved2;
	VWB_uint	bfOffBits;
} BITMAPFILEHEADER;
typedef struct BITMAPINFOHEADER {
	VWB_uint	biSize;
	VWB_int		biWidth;
	VWB_int		biHeight;
	VWB_word	biPlanes;
	VWB_word	biBitCount;
	VWB_uint	biCompression;
	VWB_uint	biSizeImage;
	VWB_int		biXPelsPerMeter;
	VWB_int		biYPelsPerMeter;
	VWB_uint	biClrUsed;
	VWB_uint	biClrImportant;
} BITMAPINFOHEADER;
typedef struct RGBQUAD {
  VWB_byte	rgbBlue;
  VWB_byte	rgbGreen;
  VWB_byte	rgbRed;
  VWB_byte	rgbReserved;
} RGBQUAD;
typedef struct BITMAPINFO {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD          bmiColors[1];
} BITMAPINFO;
#endif
#endif
#pragma pack(pop)

typedef struct VWB_WarpBlendHeader{
	VWB_WarpFileHeader4 header;
	char path[MAX_PATH];
} VWB_WarpBlendHeader;

typedef struct VWB_WarpBlend : VWB_WarpBlendHeader{
	VWB_WarpRecord* pWarp;
	union {
	VWB_BlendRecord* pBlend;
	VWB_BlendRecord2* pBlend2;
	VWB_BlendRecord3* pBlend3;
	};
	VWB_BlendRecord* pBlack;
	VWB_BlendRecord* pWhite;
} VWB_WarpBlend;

typedef std::vector<VWB_WarpBlend*> VWB_WarpBlendSet;
typedef std::vector<VWB_WarpBlendHeader*> VWB_WarpBlendHeaderSet;

typedef struct VWB_WarpBlendVertex
{
	VWB_float pos[3]; // position [0..1]
	VWB_float uv[2]; // the texture coordinate [0..1]
	VWB_float rgb[3]; // blend factor [0..1]
} VWB_WarpBlendVertex;

typedef struct VWB_WarpBlendMesh // a triangle list mesh
{
	VWB_uint nVtx; // the number of vertices
	VWB_WarpBlendVertex* vtx; // the vertices
	VWB_uint nIdx; // number of indices
	VWB_uint* idx;  // the index list
	VWB_size dim;  // the dimension of the calibrated display in pixels
}VWB_WarpBlendMesh;

#ifdef WIN32
#include <Unknwn.h>
typedef struct VWB_D3D12_RENDERINPUT
{
	IUnknown* textureResource; // ID3D12Resource*, if NULL we use rendertarget as source and issue a copy, must be in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE state
	IUnknown* renderTarget; // ID3D12Resource* must be set to add a barrier to command list or to use as copy source, must be in D3D12_RESOURCE_STATE_PRESENT state
	UINT64    rtvHandlePtr; // ptr value from D3D12_CPU_DESCRIPTOR_HANDLE of render target descriptor heap
} VWB_D3D12_RENDERINPUT;

//typedef struct VWB_D3D12Helper
//{
//	typedef UINT64( __stdcall *pfn_getfenceValue )( );
//
//	VWB_uint sig;
//	VWB_uint reserved;
//	void* pDxDevice;
//	void* pDxCommandQueue;
//	const static VWB_uint _sig = 'DX12';
//	VWB_D3D12Helper( void* _pDxDevice, void* _pDxCommandQueue )
//		: sig( _sig )
//		, reserved( 0 )
//		, pDxDevice( _pDxDevice )
//		, pDxCommandQueue( _pDxCommandQueue )
//	{}
//} VWB_D3D12Helper;
#endif //def WIN32
#pragma pack(pop)


#endif//__VWB_SMARTPROJECTOR_DEVELOPMENT_FILE_DECLARATIONS__
