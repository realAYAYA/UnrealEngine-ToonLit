# Windows AutoSDK

Visual Studio does not define any system-wide environment variables, so the Windows AutoSDK build environment is configured automatically by UnrealBuildTool based on the presence of the appropriately named directories. 

The following components can be used with the AutoSDK system:

## Visual C++

Visual C++ toolchain files can be copied from the Tools/MSVC folder under the Visual Studio installation directory. For example:

    C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Tools\MSVC\14.13.26128\...
	
Would be copied to:

    [AutoSDK Root]\HostWin64\Win64\VS2017\14.13.26128\...

Note that using each machine using a copy of the toolchain via AutoSDKs should have an appropriate license.

## Windows SDK

Multiple versions of the Windows SDK can be distributed via AutoSDK in the same merged directory hierarchy as multiple manual Windows SDK installations on a single machine. Typically:

    C:\Program Files (x86)\Windows Kits\10\...

Would be copied to:

    [AutoSDK Root]\HostWin64\Win64\Windows Kits\10\...

The NETFXSDK directory may also be distributed in the same manner.

    [AutoSDK Root]\HostWin64\Win64\Windows Kits\NETFXSDK\...

## DIA SDK

The DIA SDK is distributed with Visual Studio, and is typically installed to the DIA SDK folder under the Visual Studio installation directory. For example:

    C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\DIA SDK\...

Would be copied to:

    [AutoSDK Root]\HostWin64\Win64\DIA SDK\...

## LLVM/Clang

Multiple Clang versions can be distributed with AutoSDK, copied from the LLVM installation directory. For example:

	C:\Program Files\LLVM\...
	
Would be copied to:

    [AutoSDK Root]\HostWin64\Win64\LLVM\8.00\...
