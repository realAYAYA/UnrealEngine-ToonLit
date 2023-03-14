// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Windows/AllowWindowsPlatformTypes.h"

#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition
 #	include <d3d11.h>
#	include <d3d10.h> // for ID3D10Multithread
#pragma warning(pop)

#include "Windows/WindowsHWrapper.h"

inline const FString GetComErrorDescription(HRESULT Res)
{
	const uint32 BufSize = 4096;
	WIDECHAR buffer[4096];
	if (::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		Res,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		buffer,
		sizeof(buffer) / sizeof(*buffer),
		nullptr))
	{
		return buffer;
	}
	else
	{
		return TEXT("[cannot find error description]");
	}
}

#define VERIFY_HR(FNcall, Msg, What)	\
res = FNcall;							\
if (FAILED(res))						\
{										\
	PostError(res, Msg, What);			\
	return false;						\
}

#include "Windows/HideWindowsPlatformTypes.h"

// a convenience macro to deal with COM result codes
// is designed to be used in functions returning `bool`: CHECK_HR(COM_function_call());
#define CHECK_HR(COM_call)\
	{\
		HRESULT Res = COM_call;\
		if (FAILED(Res))\
		{\
			UE_LOG(LogElectraPlayer, Error, TEXT("`" #COM_call "` failed: 0x%X - %s"), Res, *GetComErrorDescription(Res));\
			return false;\
		}\
	}

THIRD_PARTY_INCLUDES_START
#include "mfobjects.h"
#include "mfapi.h"
THIRD_PARTY_INCLUDES_END

#include <d3d9.h>
#include <dxva2api.h>
#include <DxErr.h>

#define CHECK_HR_DX9(DX9_call)\
	{\
		HRESULT Res = DX9_call;\
		if (FAILED(Res))\
		{\
			UE_LOG(LogElectraPlayer, Error, TEXT("`" #DX9_call "` failed 0x%X: %s - %s"), Res, DXGetErrorString(Res), DXGetErrorDescription(Res));\
			return false;\
		}\
	}



namespace Electra
{

	struct FDXDeviceInfo
	{
		enum class ED3DVersion
		{
			VersionUnknown,
			Version9Win7,
			Version11Win8,
			Version11XB1,
			Version12Win10
		};

		ED3DVersion								DxVersion;
		TRefCountPtr<ID3D11Device>				DxDevice;
		TRefCountPtr<ID3D11DeviceContext>		DxDeviceContext;
		TRefCountPtr<IMFDXGIDeviceManager>		DxDeviceManager;

		TRefCountPtr<IDirect3D9>				Dx9;
		TRefCountPtr<IDirect3DDevice9>			Dx9Device;
		TRefCountPtr<IDirect3DDeviceManager9>	Dx9DeviceManager;
		TRefCountPtr<ID3D11Device>				RenderingDx11Device;
	
		static TUniquePtr<FDXDeviceInfo>		s_DXDeviceInfo;
	};


	bool IsWindows8Plus();
	bool IsWindows7Plus();

} // namespace Electra

