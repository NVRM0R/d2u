FROM debian:trixie
RUN apt-get update
RUN apt-get install -y build-essential \
                        meson wget git \
                        pkg-config cmake \
                        libsystemd-dev \
                        python3 python3-pip \
                        libboost-all-dev
WORKDIR /tmp
COPY requirements.txt /tmp/requirements.txt
RUN pip3 install --no-cache-dir -r /tmp/requirements.txt --break-system-packages

WORKDIR /app