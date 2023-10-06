<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>Detours</Name>
<!-- Software Name and Version  -->
<!-- Detours Version 5.3 -->
    <EndUserGroup>Git</EndUserGroup>
  <Location>
  //UE5/Main/Engine/Source/ThirdParty/Detours
  </Location>
  <Function>Allows intercepting system calls and adding user-specific functionality. In our case, we are using it to virtualize the filesystem and distribute work across the build farm.
  It's fairly complicated, but documented by Microsoft and supported on all versions of Windows.The readme page has more info: https://github.com/microsoft/Detours
  </Function>
  <Eula>https://github.com/microsoft/Detours/blob/main/LICENSE.md</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder></LicenseFolder>
</TpsData>



