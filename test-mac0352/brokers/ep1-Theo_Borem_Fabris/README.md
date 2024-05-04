# Broker MQTT simples

Este projeto consiste em uma implementação de um servidor
MQTT simples capaz suportar conexões concorrentes de clientes
com requisições de serviços com qualidade de serviço (QoS)
igual a zero.

Cada cliente conectado pode realizar subscrição em tópicos e publicação de mensagens em tópicos.

Vale notar que, conforme explicado no arquivo
`apresentacao.pdf` que contém as decisões de projeto, o
número de arquivos abertos é aproximadamente igual a soma dos
número de publicadores com o produto do número de inscritos
por três. Além disso, o tamanho da fila de conexões definido
na chamada da função `listen` tem valor igual a constante
`LQUEUELEN`, igual a 2000.

## Dependências

* bibliotecas padrão da linguagem C: `stdlib.h`, `stdio.h`, `string.h`, `sys/types.h`, `sys/socket.h`, `arpa/inet.h`, `unistd.h`, `signal.h`, `sys/time.h`, e `sys/resource.h`;
* biblioteca `pthread`: utilizada para implementar o broker com acesso concorrente;
* `GNU gcc`: utilizado no processo de compilação;
* `GNU Make`: utilizado no processo de compilação;
* (opcional) `valgrind`: utilizado para debug do broker; 
* (opcional) `psrecord`: utilizado para realizar a coleta dos dados sobre uso de CPU;
* (opcional) `wireshark`: utilizado para realizar a coleta dos dados sobre uso da rede;
* (opcional) distribuição `Tex`: utilizada para gerar o relatório.

## Instalação

Para gerar o arquivo binário, `./mqttbroker` do broker MQTT
implementado, basta rodar `make` ou, equivalentemente,
`make mqttbroker`.

## Uso

A inicialização do servidor é efetuada pelo comando:

```
./mqttbroker <porta>
```

Onde `<porta>` é a porta no qual serão aceitas conexões ao
servidor.

## Arquivos

* `Makefile`: arquivo da configuração do GNU make;
* `README.md`: explicações sobre o projeto;
* `apresentacao.pdf`: explicações sobre o projeto;
* `mqtt.c` e `mqtt.h`: implementações do protocolo de aplicação do MQTT;
* `mqttbroker.c`: implementação do gerenciamento das threads, conexões e estrutura de dados do broker;
* `queue.c` e `queue.h`: implementação da estrutura de dados fila;
* `search.c` e `search.h`: implementação da estrutura de dados árvore de busca binária.
