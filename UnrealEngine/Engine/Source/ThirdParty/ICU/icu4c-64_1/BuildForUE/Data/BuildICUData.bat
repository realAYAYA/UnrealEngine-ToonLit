@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set ROOT_DIR=%CD%

REM Build ICU makedata tool
echo Building ICU makedata tool...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" "%ROOT_DIR%\source\allinone\allinone.sln" /Build Release /Project makedata

REM Build each set of filtered ICU data
cd "%ROOT_DIR%\source\data"
py -B -m buildtool  --mode unix-exec  --src_dir "%ROOT_DIR%\source\data"  --tool_dir "%ROOT_DIR%\bin"  --out_dir "%ROOT_DIR%\icu-data\English\icudt64l"  --tmp_dir "%ROOT_DIR%\icu-data-tmp\English"  --filter_file "%ROOT_DIR%\Filters\English.json"
py -B -m buildtool  --mode unix-exec  --src_dir "%ROOT_DIR%\source\data"  --tool_dir "%ROOT_DIR%\bin"  --out_dir "%ROOT_DIR%\icu-data\EFIGS\icudt64l"  --tmp_dir "%ROOT_DIR%\icu-data-tmp\EFIGS"  --filter_file "%ROOT_DIR%\Filters\EFIGS.json"
py -B -m buildtool  --mode unix-exec  --src_dir "%ROOT_DIR%\source\data"  --tool_dir "%ROOT_DIR%\bin"  --out_dir "%ROOT_DIR%\icu-data\EFIGSCJK\icudt64l"  --tmp_dir "%ROOT_DIR%\icu-data-tmp\EFIGSCJK"  --filter_file "%ROOT_DIR%\Filters\EFIGSCJK.json"
py -B -m buildtool  --mode unix-exec  --src_dir "%ROOT_DIR%\source\data"  --tool_dir "%ROOT_DIR%\bin"  --out_dir "%ROOT_DIR%\icu-data\CJK\icudt64l"  --tmp_dir "%ROOT_DIR%\icu-data-tmp\CJK"  --filter_file "%ROOT_DIR%\Filters\CJK.json"
py -B -m buildtool  --mode unix-exec  --src_dir "%ROOT_DIR%\source\data"  --tool_dir "%ROOT_DIR%\bin"  --out_dir "%ROOT_DIR%\icu-data\All\icudt64l"  --tmp_dir "%ROOT_DIR%\icu-data-tmp\All"  --filter_file "%ROOT_DIR%\Filters\All.json"
cd %ROOT_DIR%
