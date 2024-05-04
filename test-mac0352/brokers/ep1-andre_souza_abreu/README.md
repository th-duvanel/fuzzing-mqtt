# SERVIDOR MOSQUITTO

Este é uma implementação do servidor  mosquitto  do  protocolo  MQTT,  tendo  em
vista atender os requisitos do Exercício Programa 1  da  disciplina  MAC0352  do
curso de Bachelarado em Ciência da Computação.

## Compilação

Para compilar o código fonte, basta  rodar  `make`.  O  software  gerado  estará
localizado em `./bin/mosquitto`.

## Uso

Execute o servidor providenciado como único argumento o número da porta na  qual
ele deve ouvir conexões. Por exemplo:

```shell
./bin/mosquitto 1883
```

Para automatizar este processo, você pode rodar `make run`, o qual irá  rodar  o
comando acima.

Após isto, pode-se usar  clientes  `mosquitto_sub`  e  `mosquitto_pub`  para  se
inscrever em e publicar em tópicos.

## Estrutura do projeto

O diretório [bin](./bin) é onde os arquivos executáveis serão colocados após
a compilação.

O diretório [src](./src) contém  todo  o  código  fonte,  incluindo  bibliotecas
externas. Os arquivos que estão diretamente neste diretório foram  escritos  por
mim. Eles includem o programa principal do servidor e alguns utilitários  usados
para propósitos de modularidade. 

O diretório [src/lib](./src/lib) contém bibliotecas de  terceiros.  Na  verdade,
contém apenas dois arquivos de uma única biblioteca implementando os  algoritmos
da  família  `sha-2`.  O  código  fonte  dela  está  disponível  no  [github  em
amosnier/sha-2](https://github.com/amosnier/sha-2)  sob  a  licença  de  domínio
público ZCBSD.

## Funcionamento do projeto

O funcionamento do servidor, detalhes de implementação, lógica interna,  uso  de
bibliotecas externas -- bem como  os  outros  detalhes  --  estão  explicado  no
[slide  apresentacao.pdf](./apresentacao.pdf)  disponiblizados  junto  com  este
projeto.

## Autor

André Souza Abreu
