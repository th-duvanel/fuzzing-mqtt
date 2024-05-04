@echo off

set opts=-FC -GR- -EHa- -nologo -O2
set code=%cd%
pushd ..\build
cl %opts% %code%\main.c -Femqtt_broker.exe
popd
