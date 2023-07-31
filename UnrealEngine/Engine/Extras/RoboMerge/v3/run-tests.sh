set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NO_COLOUR='\033[0m'

die() {
   err=$?
   msg=$1
   if [ -n "$msg" ]; then
      echo "$msg" >&2
   fi
   echo "Failed with exit code $err." >&2
   exit $err
}

cleanup() {
    # Cleanup any old containers
    docker stop robomerge_unittests &
    docker stop p4docker &
    docker stop robomerge_functtest &
    docker stop robomerge_functionaltests &
    wait

    docker rm robomerge_unittests &
    docker rm p4docker &
    docker rm robomerge_functtest &
    docker rm robomerge_functionaltests &
    wait

    # Cleanup any old networking
    docker network rm robomerge_functtest_network
}

cleanup

docker network create --driver bridge robomerge_functtest_network


docker build -t helix -f helix.Dockerfile .
docker build -t helix-and-node -f helix-and-node.Dockerfile .

docker build -t p4docker -f Dockerfile.p4docker .
docker build -t robomerge -f Dockerfile .

docker build -t robomerge_functionaltests -f functional_tests/tstests.Dockerfile .


echo "Running unit tests"
docker run -a stderr -h robomerge_unittests --name robomerge_unittests robomerge npm test

UNIT_TESTS_RETURN_CODE=`docker inspect robomerge_unittests --format='{{.State.ExitCode}}'`
if [ "$UNIT_TESTS_RETURN_CODE" != "0" ]; then
    echo Errors encountered during unit tests, check docker logs.
    echo Logs from local Robomerge instance: 
    docker logs robomerge_unittests
    cleanup
    exit 1
fi

echo "Unit tests complete. Return code: $UNIT_TESTS_RETURN_CODE"

red() { while read msg; do printf "$RED$msg$NO_COLOUR\n"; done }
green() { while read msg; do printf "$GREEN$msg$NO_COLOUR\n"; done }

echo
echo "------------------------" | green
echo "Running functional tests" | green
echo "------------------------" | green

docker run -d -p 1666:1666 -h p4docker --name p4docker --network robomerge_functtest_network p4docker
sleep 5
docker run -d -p 8877:8877 -p 8811:8811 -p 25:25 -h robomerge_functtest --name robomerge_functtest --network robomerge_functtest_network \
    -e ROBO_DEV_MODE=true  \
    -e ROBO_DEV_MODE_USER=testuser1  \
    -e P4PORT=p4docker:1666  \
    -e ROBO_BRANCHSPECS_ROOT_PATH=//RoboMergeData/Main  \
    -e ROBO_NO_TLS=true  \
    -e ROBO_EXTERNAL_URL=http://robomerge_functtest:8877  \
    -e BOTNAME=ft1,ft2,ft3,ft4,targets  \
    -e ROBO_LOG_LEVEL=info \
    -e ROBO_SLACK_DOMAIN=http://localhost:8811 \
    robomerge node --trace-warnings dist/robo/watchdog.js

docker run -t --hostname robomerge_functionaltests --name robomerge_functionaltests --network robomerge_functtest_network  \
    -e P4PORT=p4docker:1666 robomerge_functionaltests

FUNCT_TEST_RETURN_CODE=`docker inspect robomerge_functionaltests --format='{{.State.ExitCode}}'`
if [ "$FUNCT_TEST_RETURN_CODE" != "0" ]; then
    # echo "P4Docker Container Logs:"   
    # docker logs --tail 50 p4docker
    echo "RoboMerge TS Container Logs:"

    docker logs --tail 20 robomerge_functtest
    docker logs robomerge_functtest | grep -i "error:" | red

else
    echo "Functional tests complete. Return code: $FUNCT_TEST_RETURN_CODE"

fi

cleanup

