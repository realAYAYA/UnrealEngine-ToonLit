@echo off
set DOCKER_BUILDKIT=1
docker build --progress=plain --file Dockerfile --output . .