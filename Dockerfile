FROM debian:stretch-slim

ENV VER=${VER:-master} \
    REPO=https://github.com/dgiardini/rtl-ais.git \
    APP=/usr/src/app \
    GIT_SSL_NO_VERIFY=true

WORKDIR $APP

RUN apt-get update && apt-get install -y \
git \
rtl-sdr \
librtlsdr-dev \
make \
build-essential \
pkg-config \
libusb-1.0-0-dev \
&&  git clone -b $VER $REPO $APP \
&&  make

CMD $APP/rtl_ais -n

#EXPOSE 10110/udp

