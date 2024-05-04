FROM ubuntu:22.04

WORKDIR /home/test-mac0352

COPY . .

RUN apt update && apt upgrade -y

RUN apt install -y git \
    python3 \
    python3-pip \
    python3-scapy

RUN git clone https://gitfront.io/r/luisgar1990/aZXKzGjT1Wzj/mqttgram-h-repo.git
