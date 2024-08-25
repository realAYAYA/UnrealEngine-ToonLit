@ECHO OFF

set EPIC_BUILD_ROLE_NAME=robomerge-ts-service-testing
set DOCKERFILE=Dockerfile

set DOCKER_REGISTRY_DOMAIN=
set DOCKER_REGISTRY_NAMESPACE=
set DOCKER_IMAGE_NAME=%DOCKER_REGISTRY_DOMAIN%/%DOCKER_REGISTRY_NAMESPACE%/%EPIC_BUILD_ROLE_NAME%

docker version >NUL || echo ERROR: Could not connect to Docker daemon. && goto :end -1

echo Building %DOCKER_IMAGE_NAME% ...
docker build --file %DOCKERFILE% --pull --rm --tag %DOCKER_IMAGE_NAME%:latest . || goto :end -1

echo.
echo.
echo Waiting for user to test and will push...
pause

echo.
echo.
echo Pushing %DOCKER_IMAGE_NAME%:latest to %DOCKER_REGISTRY_DOMAIN%/%DOCKER_REGISTRY_NAMESPACE%
docker push %DOCKER_IMAGE_NAME%:latest

echo Success!
goto :end 0


:end
pause
exit /b %~1
