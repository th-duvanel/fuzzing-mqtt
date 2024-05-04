FROM alpine:3.14

COPY . /usr/local/lib
WORKDIR /usr/local/lib

RUN apk update
RUN apk add mosquitto-clients

ENV clients 10

CMD ./scripts/run_mqtt_subscriber.sh $clients
