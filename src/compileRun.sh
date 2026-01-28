#!/bin/bash

imageFilePath="$1"
verbose="$2"

if [ -z "$imageFilePath" ]; then
    echo "File path not passed"
    echo "Pass a file path, usage: ./compileRun <file-path>"
    exit
fi

projFile="/home/arda/Documents/Projects/SDLBasedImageViewer/src"
mainFilePath="$projFile/main.cpp"
ppmFilePath="$projFile/PPMViewer.cpp"
parserFilePath="$projFile/Parser.cpp"
g++ "$mainFilePath" "$ppmFilePath" "$parserFilePath" -o main -lSDL2 && ./main "$imageFilePath" "$verbose"
