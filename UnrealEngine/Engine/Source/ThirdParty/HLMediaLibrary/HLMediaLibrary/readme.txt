HoloLens Media Library

Supplimental solution to create dll's that wrap the Window.Media.Playback api's from the Windows runtime.

To create required files for the HLMedia plugin:
  1. Ensure the Microsoft.Windows.CppWinRT NuGet package has been installed.
    a. From the Tools menu, select NuGet Package Manager > Manage NuGet Pacakages for Solution
  2. From Build menu, select Batch Build.
    a. Ensure all targets are selected by pressing Select All button.
    b. Press Build button.

This will create and place all .lib's and .dll's for available platform targets in their respective folder.
  lib's: Engine\Source\ThirdParty\HLMediaLibrary\lib
  dll's: Engine\Binaries\ThirdParty\HLMediaLibrary
