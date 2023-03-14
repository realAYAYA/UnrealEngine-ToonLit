This module contains the EOSSDK headers and binaries.
Other plugins/modules can depend on the EOSSDK by depending on this EOSSDK module.
Several platform extensions of this module exist at Engine\Platforms\<Platform>\Source\ThirdParty\EOSSDK.
Please see also the EOSShared plugin at Engine/Plugins/Online/EOSShared which exposes common functionality, and is responsible for EOSSDK initialization.

Upgrading the SDK can be achieved with the following steps:
	- Delete all subfolders from Engine/Source/ThirdParty/EOSSDK/, leaving just the EOSSDK.Build.cs etc loose files
	- Extract the SDK zip into Engine/Source/ThirdParty/EOSSDK/
	- Move libEOSSDK-Mac-Shipping.dylib to Engine/Binarires/ThirdParty/EOSSDK/Mac/
	- Repeat for any additional platform zips.
		- Android and IOS must be unzipped into Engine/Source/ThirdParty/EOSSDK/ over the top of the base SDK
		- NDA platforms must be unzipped to their respective platform extensions at Engine/Platforms/<Platform>/Source/ThirdParty/EOSSDK/