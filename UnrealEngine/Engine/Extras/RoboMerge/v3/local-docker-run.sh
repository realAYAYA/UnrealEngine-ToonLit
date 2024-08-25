set +x
export EPIC_BUILD_ROLE_NAME=robomerge-ts-service-testing

export DOCKER_REGISTRY_DOMAIN=
export DOCKER_REGISTRY_NAMESPACE=
export DOCKER_IMAGE_NAME=$DOCKER_REGISTRY_DOMAIN/$DOCKER_REGISTRY_NAMESPACE/$EPIC_BUILD_ROLE_NAME
export DOCKER_VERSION=latest

export ROBO_LOG_LEVEL=info

# export NODE_ENV=
# export EPIC_ENV=
# export EPIC_DEPLOYMENT=
# export SENTRY_DSN=

# Set P4PORT to IP of Perforce master server to bypass any DNS issues
export P4PORT=perforce:1666
export P4PASSWD=
export BOTS=robomergeQA1
export ROBO_EXTERNAL_URL=http://localhost:8877

set -x

docker pull $DOCKER_IMAGE_NAME:$DOCKER_VERSION

docker stop $EPIC_BUILD_ROLE_NAME > /dev/null
docker rm $EPIC_BUILD_ROLE_NAME  > /dev/null

docker run -d --name $EPIC_BUILD_ROLE_NAME \
    -e "P4PASSWD=$P4PASSWD" \
    -e "P4PORT=$P4PORT" \
    -e "BOTNAME=$BOTS" \
    -e "ROBO_EXTERNAL_URL=$ROBO_EXTERNAL_URL" \
    -e "NODE_ENV=$NODE_ENV" \
    -e "EPIC_ENV=$EPIC_ENV" \
    -e "EPIC_DEPLOYMENT=$EPIC_DEPLOYMENT" \
    -e "SENTRY_DSN=$SENTRY_DSN" \
    -e "ROBO_LOG_LEVEL=$ROBO_LOG_LEVEL"
    -p 8877:8877 \
    -p 1666:1666 \
    -v robomergesavedata:/root/.robomerge \
    $DOCKER_IMAGE_NAME:$DOCKER_VERSION \
    node dist/robo/watchdog.js 