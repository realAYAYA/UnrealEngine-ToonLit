@echo off
setlocal
set FLEX=win_flex.exe
set BISON=win_bison.exe
set SRCDIR=../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib

pushd ..\..\..\..\Restricted\NotForLicensees\Extras\ThirdPartyNotUE\FlexAndBison\

echo Checking out generated files from P4...
p4 edit %THIRD_PARTY_CHANGELIST% %SRCDIR%/glcpp-lex.inl
p4 edit %THIRD_PARTY_CHANGELIST% %SRCDIR%/glcpp-parse.inl
p4 edit %THIRD_PARTY_CHANGELIST% %SRCDIR%/glcpp-parse.h
p4 edit %THIRD_PARTY_CHANGELIST% %SRCDIR%/hlsl_lexer.inl
p4 edit %THIRD_PARTY_CHANGELIST% %SRCDIR%/hlsl_parser.inl
p4 edit %THIRD_PARTY_CHANGELIST% %SRCDIR%/hlsl_parser.h

echo Running Flex and Bison...
%FLEX% --nounistd -o%SRCDIR%/glcpp-lex.inl %SRCDIR%/glcpp-lex.l
%BISON% -v -o "%SRCDIR%/glcpp-parse.inl" --defines=%SRCDIR%/glcpp-parse.h %SRCDIR%/glcpp-parse.y

%FLEX% --nounistd -o%SRCDIR%/hlsl_lexer.inl %SRCDIR%/hlsl_lexer.ll
%BISON% -v -o "%SRCDIR%/hlsl_parser.inl" -p "_mesa_hlsl_" --defines=%SRCDIR%/hlsl_parser.h %SRCDIR%/hlsl_parser.yy

echo Done
pause

popd
endlocal