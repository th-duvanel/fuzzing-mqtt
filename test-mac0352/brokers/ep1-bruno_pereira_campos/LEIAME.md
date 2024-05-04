# EP1 - MAC0352 Broker MQTT

## Sobre

Esse projeto é a implementação de um servidor broker do protocolo MQTT.  
Ele permite a comunicação entre clientes sub e pub também do protocolo MQTT, permitindo:

- a conexão de vários clientes simultaneamente;
- incrição de um cliente em um tópico e envio de mensagem par esse tópico;
- publicação de mensagem em um tópico;
- desconexão de cliente.

## Compilação e Execução

Para compilar utilize o comando:

```
make
```

Para executar o broker utilize o comando:

```
./broker 1883
```

Para interagir com ele, utilize, mosquitto_pub e sub:

```
mosquitto_pub -t 'test/topic' -m 'hello world' -q 0
mosquitto_sub -t 'test/topic' -v -q 0
```

## Biblioteca Externas Utilizadas

Foi utilizada a bibliota de hashing MD5 em C que pode ser encontrada em [https://github.com/Zunawe/md5-c](link)

## Observações

Esse servidor foi feito apenas para objetivos educacionais, é possível que haja alguns erros.  
É esperado apenas mensagens com QoS=0.
