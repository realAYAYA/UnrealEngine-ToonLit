# Latest Ubuntu LTS release as of November 2022
FROM docker.io/library/ubuntu:22.04 AS builder
WORKDIR /tmp

RUN apt-get update
RUN apt-get -qq install -y curl netcat dos2unix

RUN curl https://www.perforce.com/downloads/perforce/r22.1/bin.linux26x86_64/helix-core-server.tgz --location --silent --output helix-core-server.tgz
RUN tar zxf helix-core-server.tgz && \
    mv ./p4d /usr/local/bin/p4d && \
    mv ./p4 /usr/local/bin/p4 && \
    mkdir -p /app/data

# Start the server and generate changelists
WORKDIR /app
ADD setup.sh /app/setup.sh
RUN dos2unix setup.sh && bash setup.sh

# Copy everything to separate image to minimize image size and number of layers
FROM docker.io/library/ubuntu:22.04
COPY --from=builder /usr/local/bin/p4 /usr/local/bin/p4d /usr/local/bin/
COPY --from=builder /app/data /app/data
COPY --from=builder /trigger-log.sh /trigger-log.sh
ENTRYPOINT ["p4d", "-r", "/app/data", "-p", "1666"]
