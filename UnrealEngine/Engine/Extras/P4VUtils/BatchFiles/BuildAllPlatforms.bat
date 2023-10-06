rem Copyright Epic Games, Inc. All Rights Reserved.

setlocal 

pushd "%~dp0.."

rmdir /s /q bin
rmdir /s /q obj

rem Compile with defualt WithRestricted value, which may be true
dotnet publish P4VUtils.csproj /p:IsWindows=true /p:IsOSX=false /p:IsLinux=false
dotnet publish P4VUtils.csproj /p:IsWindows=false /p:IsOSX=true /p:IsLinux=false
dotnet publish P4VUtils.csproj /p:IsWindows=false /p:IsOSX=false /p:IsLinux=true

rem Now compile with WithRestricted of false, which may compile the same as above, or differently
dotnet publish P4VUtils.csproj /p:IsWindows=true /p:IsOSX=false /p:IsLinux=false /p:WithRestricted=false
dotnet publish P4VUtils.csproj /p:IsWindows=false /p:IsOSX=true /p:IsLinux=false /p:WithRestricted=false
dotnet publish P4VUtils.csproj /p:IsWindows=false /p:IsOSX=false /p:IsLinux=true /p:WithRestricted=false
