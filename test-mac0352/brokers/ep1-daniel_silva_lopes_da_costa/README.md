# EP1 - Broker MQTT

É  um protocolo de mensagens leve para sensores e pequenos dispositivos móveis otimizado para redes TCP/IP.  Os princípios arquitetônicos são minimizar o uso de banda de rede e uso de recursos dos equipamentos enquanto garantindo confiabilidade e algum nível de garantia de entrega.

Nesse exercício programa foi implementada a versão 3.1.1 do broker MQTT na linguagem C, com Qos=0.


.## Arquivos
- **mqtt-broker.c** - Arquivo principal que armazena as principais funções de rede com a declaração do soquete e das estruturas para armazenar endereços. Também é onde acontece a leitura dos pacotes enviados pelo cliente e também a escrita de pacotes de saída;
- **definitions.h** - Armazena a definição para as principais constantes utilizadas no programa, por exemplo o número 3 para indicar o pacote do tipo publisher;
- **utils.c** - Funções auxiliares, que geralmente envolvem mudanças ou buscas nas estruturas de dados;
- **Makefile** - responsável pela compilação do código.


## Como Utilizar
Para utilizar o broker MQTT, será necessário ter os dois clientes do mosquito: mosquitto_sub e mosquitto_pub.
 
Inicialmente geramos o executável com o makefile:
 
~~~
make
~~~
 
Em seguida podemos executar passando a parta em que o broker vai receber os pacotes de controle:
 
~~~
./broker 8000
~~~
 
Em seguida em outra janela do terminal podemos rodar um cliente do mosquito:
 
~~~
mosquitto_sub -t 'Im' -V 311 -p 8000
~~~
 
Nesse caso estamos declarando um subscriber, do tópico 'Im', também foi especificada a porta e a versão do mqtt.
 
Com isso, em outro terminal podemos declarar um publisher:
 
~~~
mosquitto_pub -t 'Im' -m 'fire' -V 311 -p 8000
~~~
 
Ele publicará no mesmo tópico que o subscriber está ouvindo, a mensagem 'fire'.


## Dificuldades Encontradas
#### Formato da mensagem enviada para o subscriber
 
Perdi muito tempo por não saber o formato da mensagem que seria enviada para o publisher, após o envio pelo subscriber, por algum motivo assumi que seria apenas a mensagem em si em ASCII, por isso na decodificação do pacote do publisher separei a mensagem. No final consegui descobrir que mensagem deveria ser enviada em completo para o subscriber.
 
 
#### Estutura de dados para salvar as informações
 
Outro ponto negativo foi que apenas usei lista para armazenar a lista de tópicos e de subscribers o que deixou a solução não escalável e ineficaz - tentei implementar as estruturas com lista ligadas, mas por algum motivo, mesmo fazendo o malloc global, quando mudava de processo, o arquivo salvo no nó apresentava inconsistências - esse foi outro motivo que me levou a uma grande perda de tempo.
 
 
#### Esperar um tempo entre uma publicação e outra.
 
Outro problema é que não estou conseguindo enviar mensagens para um mesmo tópico de maneira instantânea. Se uma mensagem for enviada, tem que esperar aproximadamente 60s para enviar outra mensagem, do contrário a mensagem não vai chegar nos subscribers.
 
 
## Tempo de dedicação
- 09/04/2022 - Dedicação de 2h
- 10/04/2022 - Dedicação de 1h
- 15/04/2022 - Dedicação de 7h
- 16/04/2022 - Dedicação de 5h
- 17/04/2022 - Dedicação de 6h
- 21/04/2022 - Dedicação de 9h
- 22/04/2022 - Dedicação de 4h
- 23/04/2022 - Dedicação de 10h
- 24/04/2022 - Dedicação de 15h
- 25/04/2022 - Dedicação de 12h
 
 
## Autor
- Daniel Silva Lopes da Costa - NUSP 11302720  