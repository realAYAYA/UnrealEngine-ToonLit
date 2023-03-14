@Rem Copyright Epic Games, Inc. All Rights Reserved.

@echo off

@Rem Set script location as working directory for commands.
pushd "%~dp0"

:arg_loop_start
SET ARG=%1
if DEFINED ARG (
    if "%ARG%"=="/h" (
        goto print_help
    )
    if "%ARG%"=="/v" (
        SET UEVersion=%2
        SHIFT
    )
    if "%ARG%"=="/b" (
        SET PSInfraTagOrBranch=%2
        SET IsTag=0
        SHIFT
    )
    if "%ARG%"=="/t" (
        SET PSInfraTagOrBranch=%2
        SET IsTag=1
        SHIFT
    )
    SHIFT
    goto arg_loop_start
)

@Rem Name and version of ps-infra that we are downloading
SET PSInfraOrg=EpicGames
SET PSInfraRepo=PixelStreamingInfrastructure

@Rem If a UE version is supplied set the right branch or tag to fetch for that version of UE
if DEFINED UEVersion (
  if "%UEVersion%"=="4.26" (
    SET PSInfraTagOrBranch=UE4.26
    SET IsTag=0
  )
  if "%UEVersion%"=="4.27" (
    SET PSInfraTagOrBranch=UE4.27
    SET IsTag=0
  )
  if "%UEVersion%"=="5.0" (
    SET PSInfraTagOrBranch=UE5.0
    SET IsTag=0
  )
)

@Rem If no arguments select a specific version, fetch the appropriate default
if NOT DEFINED PSInfraTagOrBranch (
    SET PSInfraTagOrBranch=UE5.1
    SET IsTag=0
)

@Rem Whether the named reference is a tag or a branch affects the URL we fetch it on
if %IsTag%==1 (
  SET RefType=tags
) else (
  SET RefType=heads
)

@Rem Look for a SignallingWebServer directory next to this script
if exist SignallingWebServer\ (
  echo SignallingWebServer directory found...skipping install.
) else (
  echo SignallingWebServer directory not found...beginning ps-infra download.

  @Rem Download ps-infra and follow redirects.
  curl -L https://github.com/%PSInfraOrg%/%PSInfraRepo%/archive/refs/%RefType%/%PSInfraTagOrBranch%.zip > ps-infra.zip
  
  @Rem Unarchive the .zip
  tar -xmf ps-infra.zip || echo bad archive, contents: && type ps-infra.zip && exit 0

  @Rem Rename the extracted, versioned, directory
  for /d %%i in ("PixelStreamingInfrastructure-*") do (
    for /d %%j in ("%%i/*") do (
      echo "%%i\%%j"
      move "%%i\%%j" .
    )
    for %%j in ("%%i/*") do (
      echo "%%i\%%j"
      move "%%i\%%j" .
    )

    echo "%%i"
    rmdir /s /q "%%i"
  )

  @Rem Delete the downloaded zip
  del ps-infra.zip
)

exit 0

:print_help
echo.
echo  Tool for fetching PixelStreaming Infrastructure. If no flags are set specifying a version to fetch,
echo  the recommended version will be chosen as a default.
echo.
echo  Usage:
echo    %~n0%~x0 [^/h] [^/v ^<UE version^>] [^/b ^<branch^>] [^/t ^<tag^>]
echo  Where:
echo    /v      Specify a version of Unreal Engine to download the recommended release for
echo    /b      Specify a specific branch for the tool to download from repo
echo    /t      Specify a specific tag for the tool to download from repo
echo    /h      Display this help message
exit 1