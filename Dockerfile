# Can't use buster-slim because it needs librtlsdr-dev 0.5.3
# See https://github.com/dgiardini/rtl-ais/issues/32
FROM debian:stretch-slim

ENV APP=/usr/src/app

WORKDIR $APP

COPY . $APP

RUN apt-get update && apt-get install -y \
git \
rtl-sdr \
librtlsdr-dev \
make \
build-essential \
pkg-config \
libusb-1.0-0-dev \
&& make \ 
&& rm -rf /var/lib/apt/lists/*

CMD $APP/rtl_ais -n

#EXPOSE 10110/udp

