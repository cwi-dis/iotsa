#!/bin/bash
case "$1" in
on)
    arg=--skip-worktree
    ;;
off)
    arg=--no-skip-worktree
    ;;
*)
    echo Usage: $0 "[ on | off ]"
    echo To ignore changes to the version files, or not ignore them again
    exit 1
    ;;
esac
set -x 
git update-index $arg extras/python/iotsa/version.py src/iotsaVersion.h
