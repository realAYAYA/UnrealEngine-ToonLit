Under `benchmark-suite` you will find a python program and dockerfile to build it into a container that can run benchmarks against UnrealCloudDDC.
We use vegeta to run these benchmarks and the python app generates test data which vegeta can use to run concurrent http tests.

The docker image will automatically run all tests on startup, so we usually use this to just start the docker image somewhere and having that run its tests.
The image expects to get a valid access token passed to it as a argument.
Usage:
```
docker build -t jupiter_benchmark
docker run --network host jupiter_benchmark \
--seed --host <url-to-service> \
--header="Authorization: Bearer <access-token>" \
all
```

While the python script can be executed on any platform vegeta only works for linux, if you have a linux machine you can run these locally otherwise run them via docker.