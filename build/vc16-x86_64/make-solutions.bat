@echo off
::
:: run this batch file to create a Visual Studio solution file for this project.
:: See the cmake documentation for other generator targets
::
cmake -G "Visual Studio 16 2019" ..\..\source && cmake-gui ..\..\source
