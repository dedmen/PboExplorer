@echo off

set MYDIR=%~dp0

regsvr32 /u "%MYDIR%PboExplorer.dll"

