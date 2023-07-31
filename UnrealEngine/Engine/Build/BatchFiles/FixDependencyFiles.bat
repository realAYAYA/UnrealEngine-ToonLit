@echo off
pushd "%~dp0..\..\"
for /f "tokens=*" %%X in ('dir /b /s /a:-d Content\Editor\Slate\*.PNG') do ren "%%X" "%%~nX.png"
for /f "tokens=*" %%X in ('dir /b /s /a:-d Content\Slate\*.PNG') do ren "%%X" "%%~nX.png"

if not exist Documentation\Source\Shared\Icons goto no_documentation_icons
for /f "tokens=*" %%X in ('dir /b /s /a:-d Documentation\Source\Shared\Icons\*.PNG') do ren "%%X" "%%~nX.png"
:no_documentation_icons

popd