This folder contains a subset of the legacy DirectX components 
that do not conflict with and have not been replaced in the Win10
SDK.  Such that this header\lib location can be included without
pulling in conflicting components when using newer SDKs.

The original DX folder, as-is, will cause DX imports to include incorrect or out of date types.
That folder should be removed once all build configurations are moved over to use this new
segmented layout that makes the conflicting header more clear.

For more information please see: 
https://walbourn.github.io/the-zombie-directx-sdk/
