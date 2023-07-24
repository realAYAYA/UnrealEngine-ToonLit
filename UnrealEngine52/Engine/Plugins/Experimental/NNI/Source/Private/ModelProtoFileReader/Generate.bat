@echo off
setlocal

SET vcpkg_root=%~dp0..\..\..\..\..\..\Source\ThirdParty\vcpkg\Win64\x64-windows-static-md-v142
SET protoc=%vcpkg_root%\protobuf_x64-windows-static-md-v142\tools\protobuf\protoc.exe
SET include=%vcpkg_root%\protobuf_x64-windows-static-md-v142\include

pushd %~dp0
mkdir .\Generated

for /R ".\Schemas" %%f in (*.proto*) do (
	%protoc% -I=%include% --proto_path=%CD%\Schemas --cpp_out=%CD%\Generated %%f
)

for /R ".\Generated" %%f in (*.pb.cc) do (
	type SchemaAutogenHeader.txt >%%f.new
	type SchemaHeader.txt >>%%f.new
	type %%f >>%%f.new
	type SchemaFooter.txt >>%%f.new

	move /y %%f.new %%f >NUL
)

for /R ".\Generated" %%f in (*.pb.h) do (
	type SchemaAutogenHeader.txt >%%f.new
	type %%f >>%%f.new

	move /y %%f.new %%f >NUL
)

popd
