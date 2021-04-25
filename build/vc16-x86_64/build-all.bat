@echo off
if "%VS160COMNTOOLS%" == "" (
  msg "%username%" "Visual Studio 16 not detected"
  exit 1
)
if not exist x265.sln (
  call make-solutions.bat
)
if exist x265.sln (
  call "%VS160COMNTOOLS%\..\..\VC\vcvarsall.bat"
  MSBuild /property:Configuration="Release" x265.sln
  MSBuild /property:Configuration="Debug" x265.sln
  MSBuild /property:Configuration="RelWithDebInfo" x265.sln
)
