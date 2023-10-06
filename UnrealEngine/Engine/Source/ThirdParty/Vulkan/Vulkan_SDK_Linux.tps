<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>Software Name: Vulkan SDK - Linux  </Name>
<!-- Software Name and Version  -->
<!-- Software Name: Software Name: Vulkan SDK - Linux
    Version: 1.0 -->
<!-- Notes: 
We already have a TPS for this version for Windows.  I'm adding Linux support and doing that requires moving the Windows DLL location (see the updated repo location above) and adding Linux versions.  Headers are untouched from what they were before.
-->
<EndUserGroup>Git</EndUserGroup>
  <Location>
  //UE5/Main/Engine/Source/ThirdParty/Vulkan
  //UE5/Main/Engine/Binaries/ThirdParty/Vulkan 
  </Location>
  <Function>Source headers used to link against a Vulkan driver which is dynamically loaded by the user's machine.  There are also debugging related DLLs provided which we ship along with the engine. </Function>
  <Eula>https://vulkan.lunarg.com/software/license/vulkan-1.3.239.0-linux-license-summary.txt</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder></LicenseFolder>
</TpsData>



