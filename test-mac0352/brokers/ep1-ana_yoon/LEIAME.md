# EP1 - MAC0352

## Autoria

Nome: Ana Yoon Faria de Lima
NUSP: 11795273

## Visão geral

Implementação simplificada de um broker MQTT, que deve permitir a conexão e inscrição de clientes em um determinado tópico, o que deve ocasionar o envio das mensagens deste tópico para o cliente. Para isso, foi utilizado como base o código `mac-0352-servidor-exemplo-ep1.c`, disponibilizado pelo professor Daniel Batista. Detalhes sobre a implementação estão explicitados no próprio código e nos slides presentes no `.pdf` desse projeto.

## Compilação

O arquivo `mac0352-ep1-broker.c` contém a implementação do broker MQTT simplificado. Para compilá-lo, é necessário colocar a flag `-lpthread`, devido ao uso da biblioteca `pthread.h` e a flag `-lm`, devido ao uso da bilbioteca `math.h`. Isso é feito no Makefile, para facilitar a execução.

## Execução

Para executar o projeto, rode:

```
make all
```
O makefile irá gerar um executável com nome `broker`. Para executá-lo, deve-se passar a porta em que se deseja executar como primeiro argumento. Por exemplo:
```
./broker 1883
```
Para limpar (apagar o executável), rode:
```
make clean
```
## Exemplo de funcionamento

Em uma máquina, rode o servidor:
```
./broker <porta>
```

Em outra máquina, pode-se rodar os clientes, tanto o publisher quanto o subscriber, passando-se o tópico em que se deseja publicar/inscrever, o IP da máquina em que o servidor está rodando e, no caso do publisher, a mensagem a ser publicada:

```
mosquitto_sub -V mqttv311 -p <porta> -t <topico> -h <IP_DA_MAQUINA_DO_SERVIDOR>
```

```
mosquitto_pub -V mqttv311 -p <porta> -t <topico> -m <mensagem> -h <IP_DA_MAQUINA_DO_SERVIDOR>
```

## Referências

- [Documentação do MQTT](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)
- [Execução paralela em C](https://stackoverflow.com/questions/10818658/parallel-execution-in-c-program)
- [Biblioteca pthreads.h](https://man7.org/linux/man-pages/man7/pthreads.7.html)
- [Multithreading em C](https://www.geeksforgeeks.org/multithreading-c-2/)
- [Documentação da função open()](https://linux.die.net/man/3/open)
- [Biblioteca dirent.h](https://www.decodeschool.com/C-Programming/File-Operations/C-Program-to-read-all-the-files-located-in-the-specific-directory)

