# Can't use buster-slim because it needs librtlsdr-dev 0.5.3
# See https://github.com/dgiardini/rtl-ais/issues/32
FROM debian:stretch-slim
LABEL "name"="rtl-ais" \
  "description"="AIS ship decoding using an RTL-SDR dongle" \
  "author"="Bryan Klofas KF6ZEO"

ENV APP=/usr/src/app

WORKDIR $APP

COPY . $APP

RUN apt-get update && apt-get install -y \
  rtl-sdr \
  librtlsdr-dev \
  libusb-1.0-0-dev \
  make \
  build-essential \
  pkg-config \
  && make \ 
  && apt-get remove -y make build-essential pkg-config \
  && apt-get autoremove -y \
  && rm -rf /var/lib/apt/lists/*

CMD $APP/rtl_ais -n

