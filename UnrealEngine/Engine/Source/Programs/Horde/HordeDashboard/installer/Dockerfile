FROM node:20.11.0 as build-env
ARG VersionInfo
ENV REACT_APP_VERSION_INFO=${VersionInfo}
WORKDIR /app

COPY Programs/Horde/HordeDashboard ./Dashboard
RUN cd ./Dashboard && yarn install
RUN cd ./Dashboard && yarn run build

