echo off

set dst=..\build\bin\

rem Build EXE
msbuild /property:GenerateFullPaths=true /p:Configuration=Release /p:Platform="x86" /t:build ..\ecs.sln

xcopy /Y ..\Release\jemalloc.dll %dst%
xcopy /Y ..\Release\sample.exe %dst%

rem Data
xcopy /Y *.as %dst%
xcopy /Y *.json %dst%
xcopy /S /Y ..\assets\* %dst%..\assets\*

rem Editor
xcopy /Y editor.cmd %dst%
xcopy /Y ..\webui\dist\* %dst%editor\*