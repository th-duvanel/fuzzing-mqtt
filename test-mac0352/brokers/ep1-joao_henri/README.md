# MAC0352 - Redes de Computadores e Sistemas Distribuídos - EP1

## Author:

- 11796378 - João Henri Carrenho Rocha - [joao.henri@usp.br](joao.henri@usp.br) 

## Dependencies:

You'll need to have Docker installed and the bridge network configured to run the tests.

## Folders:

- **docs**: has the slides and the description of the EP;
- **infra**: has the Dockerfiles used to run the tests;
- **scripts**: has the scripts executed by the Docker containers in the tests;
- **src**: has the EP's source code;
- **test-outputs**: has some charts and previous outputs collected by **scripts/monitor_cpu_and_network_usage.sh**;

## Running:

To run the broker in localhost at port 1883, simply:
```
make run
```

To run the broker in a Docker container at port 1883:
```
make run-broker
```

To kill this Docker container:
```
make kill-broker
```

## Tests:

These tests run Docker containers and tag images. This way, to delete everything, you need to:
1) Kill the running container (docker kill)
2) Remove the named image (docker rm)

The tests will remove everything after each run, but if you stop it or something goes wrong during initialization, you'll need to do this manually. Check `make kill-broker` for an example.

Also, it is assumed that your network bridge runs using the default **172.17.X.X** range. If this is not the case, you need to fix it in **scripts/publish_messages.sh** and **scripts/run_mqtt_subscriber.sh**

To run the broker without any publisher or client:
```
make test-0-clients
```


To run the broker with 100 clients:
```
make test-100-clients
```


With 1000 clients:
```
make test-1000-clients
```

