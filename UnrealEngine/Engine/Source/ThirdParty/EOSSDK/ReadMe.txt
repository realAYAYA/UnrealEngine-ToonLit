This module contains the EOSSDK headers and binaries.
Other plugins/modules can depend on the EOSSDK by depending on this EOSSDK module.
Several platform extensions of this module exist at Engine\Platforms\<Platform>\Source\ThirdParty\EOSSDK.
Please see also the EOSShared plugin at Engine/Plugins/Online/EOSShared which exposes common functionality, and is responsible for EOSSDK initialization.

Upgrading the SDK can be achieved with the following steps:
	- Delete all subfolders from Engine/Source/ThirdParty/EOSSDK/, leaving just the loose files, e.g. EOSSDK.Build.cs
	- Extract the SDK zip into Engine/Source/ThirdParty/EOSSDK/. You should end up with an Engine/Source/ThirdParty/EOSSDK/SDK folder.
	- For Mac specifically, move libEOSSDK-Mac-Shipping.dylib from Engine/Source/ThirdParty/EOSSDK/SDK/Bin/ to Engine/Binaries/ThirdParty/EOSSDK/Mac/
	- Repeat for any additional platform zips.
		- Android and IOS do not have platform extensions. The Android/IOS SDK zips must be unzipped into Engine/Source/ThirdParty/EOSSDK/ over the top of the base SDK.
		- NDA/console platforms do have platform extensions. The platform specific zips must be unzipped into Engine/Platforms/<Platform>/Source/ThirdParty/EOSSDK/ having first deleted any subfolders there.