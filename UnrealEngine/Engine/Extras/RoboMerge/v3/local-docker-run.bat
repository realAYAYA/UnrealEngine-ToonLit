@ECHO OFF
set EPIC_BUILD_ROLE_NAME=robomerge-ts-service-testing

set DOCKER_REGISTRY_DOMAIN=
set DOCKER_REGISTRY_NAMESPACE=
set DOCKER_IMAGE_NAME=%DOCKER_REGISTRY_DOMAIN%/%DOCKER_REGISTRY_NAMESPACE%/%EPIC_BUILD_ROLE_NAME%
set DOCKER_VERSION=latest

set ROBO_LOG_LEVEL=info

REM Set P4PORT to IP of Perforce master server to bypass any DNS issues
set P4PORT=perforce:1666
set P4PASSWD=
set BOTS=robomergeQA1
set ROBO_EXTERNAL_URL=https://localhost:4433
set PORTS_ARGS=-p 4433:4433

REM set NODE_ENV=
REM set EPIC_ENV=
REM set EPIC_DEPLOYMENT=
REM set SENTRY_DSN=

@ECHO ON

REM docker pull --platform linux %DOCKER_IMAGE_NAME%:%DOCKER_VERSION%
docker pull %DOCKER_IMAGE_NAME%:%DOCKER_VERSION%

docker stop %EPIC_BUILD_ROLE_NAME%>NUL
docker rm %EPIC_BUILD_ROLE_NAME%>NUL

@REM -v robosettings:/root/.robomerge \

docker run -d --name %EPIC_BUILD_ROLE_NAME% ^
    -e "P4PASSWD=%P4PASSWD%" ^
    -e "P4PORT=%P4PORT%" ^
    -e "BOTNAME=%BOTS%" ^
    -e "ROBO_EXTERNAL_URL=%ROBO_EXTERNAL_URL%" ^
    -e "NODE_ENV=%NODE_ENV%" ^
    -e "EPIC_ENV=%EPIC_ENV%" ^
    -e "EPIC_DEPLOYMENT=%EPIC_DEPLOYMENT%" ^
    -e "SENTRY_DSN=%SENTRY_DSN%" ^
    -e "ROBO_LOG_LEVEL=%ROBO_LOG_LEVEL%" ^
    -v "%CD%\vault:/vault:ro" ^
    -v robomergesavedata:/root/.robomerge ^
    %PORTS_ARGS% ^
    %DOCKER_IMAGE_NAME%:%DOCKER_VERSION% 

pause