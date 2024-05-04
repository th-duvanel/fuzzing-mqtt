FROM gcc as build

COPY . ./

RUN make clean && make

FROM ubuntu

WORKDIR /usr/src/app

ENV PORT=1883

COPY --from=build ./build/broker ./

RUN chmod 777 ./broker

EXPOSE $PORT

ENTRYPOINT /usr/src/app/broker ${PORT}
