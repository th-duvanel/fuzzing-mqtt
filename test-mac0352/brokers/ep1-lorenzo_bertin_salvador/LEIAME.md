## EP1 MAC0352 - Lorenzo Bertin Salvador - N°USP: 11795356

## Broker MQTT

# Instalação:
Para compilar:
```bash
$ make
```
Para executar o broker (se port < 1024, será necessário rodar o programa como root):
```bash
$ ./ep1broker {port}
```
Se nenhuma porta for assinalada, o broker usa a porta padrão do protocolo MQTT(1883).

# Makefile:
Contém as instruções para o compilador:
#### Flags utilizadas:
+ -Wall (habilita todos os warnings do compilador)
+ -ln (linka a biblioteca `<math.h>` )

Durante a execução do broker, dependendo de como a conexão é finalizada, alguns arquivos que deveriam ser temporários ainda podem estar presentes no diretório /tmp. Para removê-los, basta rodar:
```bash
$ make clean
```
ou
```bash
$ rm -f /tmp/broker11795356*
```
que é exatamente o que o primeiro comando executa.

# ep1broker.c:
O código fonte do servidor broker.
Se durante a interação dos clientes com o servidor parecer que o programa está travando, talvez seja necessário aumentar o cooldown que o programa checa os descritores. Para isso, basta aumentar o valor da macro `#define COOLDOWN` nas primeiras linhas do código e recompilá-lo. 
# apresentacao.pdf:
Os slides explicando o funcionamento do servidor e expondo sua performance.
