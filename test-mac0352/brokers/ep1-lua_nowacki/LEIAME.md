# LEIAME
Uma versão simplificada de MQTT Broker de acordo com a versão 3.1.1 que implementa partes do protocolo.

## Introdução

Um MQTT Broker é um tipo de aplicação capaz de receber e enviar pacotes associados a tópicos. Ele roda através de uma camada TCP/IP. 

## Implementação

O Broker implementa os seguintes tipos de pacotes: 

* CONNECT
* CONNACK
* PUBLISH
* PUBACK
* SUBSCRIBE
* SUBACK
* PINGREQ
* PINGRESP
* DISCONNECT

## Compilando
### Linux
Certifique-se que tem o gcc instalado na máquina. Entre diretório code e rode o seguinte comando: 

```
./build.sh
```

O binário resultante estará na pasta build com o nome mqtt_broker.out

### Windows
Certifique-se que tem o mscv instalado na máquina. Rode o `` vcvarsall.bat x64`` para ativar o compilador na linha de comando e depois rode o seguinte comando no diretório code

```
build.bat
```

O binário resultante estará na pasta build com o nome mqtt_broker.exe

## Executando o programa

Na pasta build rode o comando 

```
./mqtt_broker.out <PORT>
```

Onde PORT deve ser substituído pela porta em que quer rodar o broker. Exemplo: 

```
./mqtt_broker.out 1883
> 1650938573: Opening ipv4 listen socket on port 1883
```

## Significado do output
O output do programa é feito em linhas que seguem o seguinte formato: 

```
<timestamp>: <message>
```

Exemplo: 

```
1650938772: New connection from 127.0.0.1
```

Em casos em que um pacote MQTT é recebido ou enviado pelo programa o output vem formatado da seguinte forma

```
<timestamp>: <protocol_name> <direction> <client_id>
```

E caso o protocolo se refira a algum tópico em específico: 

```
<timestamp>: <pro...> <dir..> <cli..> (<topic_name>)
```

Exemplos de output: 

```
1650938772: CONNECT    <- mosquitto_sub_0             
1650938772: CONNACK    -> mosquitto_sub_0                     
1650938772: SUBSCRIBE  <- mosquitto_sub_0 (magenta)       
1650938772: SUBACK     -> mosquitto_sub_0   
1650938832: PINGREQ    <- mosquitto_sub_0                      1650938832: PINGRESP   -> mosquitto_sub_0
```

### Testes
Para testar o broker é possível usar os scripts pub_test.py e sub_test.py. Para isso certifique-se da instalação do python3 na máquina que irá rodar os scripts, assim como do mosquitto_sub e do mosquitto_pub

```
#/ python3 --version
> Python 3.8.10
```

### Rodando os scripts de test

Os scripts de test assumem que o broker está executando na porta 1883. 

Os argumentos do script sub_test.py são, em sequência, endereço do host, número total de clientes sub e número de tópicos (<1000), 
selecionados aleatoriamente entre os N primeiros da lista topics.txt

```
#/ python3 sub_test.py
> Usage: <host_address> <number_of_sub_clients> <number_of_sub_topics>
```

Os argumentos do script sub_test.py são, em sequência, endereço do host, número total de clientes pub,  número de tópicos (<1000), 
selecionados aleatoriamente entre os N primeiros da lista topics.txt e tamanho em bytes das mensagens enviadas. 

```
#/ python3 pub_test.py
> Usage: <host_address> <number_of_pub_clients> <number_of_pub_topics> <size_of_messages>
```

## Medindo a perfomance 
O script measure.py pode ser utilizado para medir a perfomance do broker. Ele requer que a biblioteca psutil esteja instalada no sistema. E assume que o nome do executável do broker é mqtt_broker.out.

```
#/ python3 measure.py
> Usage: <result_filename> <experiment_number>
```

O arquivo resultante terá o formato: ``<result_filename>_<experiment_number>.csv``

O script imprime na tela e grava a cada 0.1s bytes enviados pelo sistema, bytes recebido pelo sistema, utilização da CPU % e número de conexões do broker. 

### Exemplo

```
#/ python3 test_only_broker 1
> timestamp, cpu_percent, bytes_sent, bytes_recv, num_connection
0.0, 0.0, 0, 0, 1
0.1, 0.0, 0, 60, 1
0.2, 0.0, 0, 60, 1
0.3, 0.0, 0, 60, 1
0.4, 0.0, 0, 60, 1
0.5, 0.0, 0, 60, 1
0.6, 0.0, 0, 60, 1
0.7, 0.0, 0, 60, 1
0.8, 0.0, 0, 60, 1
..
..
-> Ele roda até ser interrompido pelo usuário
#/ head test_only_broker_1.csv
> timestamp, cpu_percent, bytes_sent, bytes_recv, num_connection
0.0, 0.0, 0, 0, 1
0.1, 0.0, 0, 60, 1
0.2, 0.0, 0, 60, 1
0.3, 0.0, 0, 60, 1
0.4, 0.0, 0, 60, 1
0.5, 0.0, 0, 60, 1
0.6, 0.0, 0, 60, 1
0.7, 0.0, 0, 60, 1
0.8, 0.0, 0, 60, 1

```