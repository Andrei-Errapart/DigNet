@echo off
rem Compile build file generator, if needed.
if  not exist PrintBuildInfo.class javac PrintBuildInfo.java

rem Compile build file.
java PrintBuildInfo %* > src\BuildInfo.java

