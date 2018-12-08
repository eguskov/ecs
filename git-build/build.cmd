echo off

set dst=..\git-tools\

rem Build EXE
msbuild /property:GenerateFullPaths=true /p:Configuration=Debug /p:Platform="x86" /t:build ..\ecs.sln

xcopy /Y curl\bin\libcurl.dll %dst%
xcopy /Y openssl\libssl-1_1.dll %dst%
xcopy /Y openssl\libcrypto-1_1.dll %dst%

xcopy /Y ..\Debug\jemallocd.dll %dst%
xcopy /Y ..\Debug\build.exe %dst%

rem Data
xcopy /Y *.md %dst%
xcopy /Y *.as %dst%
xcopy /Y *.json %dst%