FROM alpine:3.14

COPY . /usr/local/lib
WORKDIR /usr/local/lib

RUN apk update
RUN apk add mosquitto-clients

CMD ["./scripts/publish_messages.sh", "1000"]
