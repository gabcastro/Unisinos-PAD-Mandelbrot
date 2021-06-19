## Trabalho final da cadeira de Processamento de Alto Desempenho

### Descrição

Utilização de C++ para criação de um código de processamento paralelo sob a tarefa de vizualização de uma Mandelbrot.

### Configurar ambiente e executar

Caso o X11 não esteja instalado: 
` sudo apt-get isntall xorg `

Para exercutar o programa, basta executar o arquivo Makefile (`make`) no diretório raiz.

### External source

Reutilização do código sequencial: http://www.cs.nthu.edu.tw/~ychung/homework/para_programming/seq_mandelbrot_c.htm


#### Nota

Está funcionando com threads, variáveis de condição e mutex. Porém, os valores estão sendo plotados em forma sequencial, diferente do esperado que era computar primeiro os valores menos trabalhosos.
