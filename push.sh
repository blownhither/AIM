#!/bin/bash
set -x

git add .
git commit -m "$1"
git push os

