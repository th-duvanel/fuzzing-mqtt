########################################################################################################################
# Used for debugging and running on localhost
build:
	gcc -o ep1 src/main.c src/mqtt.c src/util.c src/topic.c -Wall -g -pedantic -Wno-unused-result

run: build
	./ep1 1883

########################################################################################################################
# Used to run the broker on a Docker container
build-broker-image:
	docker build -f infra/Dockerfile . -t ep1-joao-henri

run-broker: build-broker-image
	docker run -d --name ep1-joao-henri ep1-joao-henri

kill-broker:
	docker kill ep1-joao-henri && docker rm ep1-joao-henri

########################################################################################################################
# Used to create a container that runs multiple MQTT subscribers
build-clients:
	docker build -f infra/client.Dockerfile . -t ep1-joao-henri-clients

run-clients: build-clients
	docker run -e clients=$(clients) -d --name ep1-joao-henri-clients ep1-joao-henri-clients

kill-clients:
	docker kill ep1-joao-henri-clients && docker rm ep1-joao-henri-clients

########################################################################################################################
# Used to create a container that publishes messages to the MQTT broker
build-publisher:
	docker build -f infra/publisher.Dockerfile . -t ep1-joao-henri-publisher

run-publisher: build-publisher
	docker run -d --name ep1-joao-henri-publisher ep1-joao-henri-publisher

kill-publisher:
	docker kill ep1-joao-henri-publisher && docker rm ep1-joao-henri-publisher

########################################################################################################################
# Run tests and get CPU and NETWORK IO data
test-0-clients: run-broker
	rm -f test-outputs/network-0-test test-outputs/cpu-0-test
	./scripts/monitor_cpu_and_network_usage.sh test-outputs/cpu-0-test test-outputs/network-0-test
	make kill-broker

test-100-clients: run-broker
	rm -f test-outputs/network-100-test test-outputs/cpu-100-test
	make run-clients clients=100 &
	make run-publisher &
	./scripts/monitor_cpu_and_network_usage.sh test-outputs/cpu-100-test test-outputs/network-100-test
	make kill-broker
	make kill-clients
	make kill-publisher

test-1000-clients: run-broker
	rm -f test-outputs/network-1000-test test-outputs/cpu-1000-test
	make run-clients clients=1000 &
	make run-publisher &
	./scripts/monitor_cpu_and_network_usage.sh test-outputs/cpu-1000-test test-outputs/network-1000-test
	make kill-broker
	make kill-clients
	make kill-publisher

########################################################################################################################
# Kill all containers
kill-all:
	docker kill ep1-joao-henri && docker rm ep1-joao-henri
	docker kill ep1-joao-henri-clients && docker rm ep1-joao-henri-clients
	docker kill ep1-joao-henri-publisher && docker rm ep1-joao-henri-publisher