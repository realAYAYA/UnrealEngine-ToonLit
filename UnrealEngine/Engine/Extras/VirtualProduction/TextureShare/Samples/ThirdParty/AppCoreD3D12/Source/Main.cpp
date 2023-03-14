//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12HelloTexture.h"
#include "D3D12AppSetup.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	std::string AppName(D3D12AppSetup::AppName);
	std::wstring WAppName(AppName.begin(), AppName.end());

	D3D12HelloTexture sample(D3D12AppSetup::Backbuffer::Size.X, D3D12AppSetup::Backbuffer::Size.Y, WAppName);
	return Win32Application::Run(&sample, hInstance, nCmdShow);
}
