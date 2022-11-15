#include <stdio.h>
#include <cstdlib>
#include <iostream> //pode dar problema
#include <string>
#include <cstring>
#include <omp.h>
#include <mpi.h>

using namespace std;

const int tamanhoDoBloco = 5;
const int quantidadeDeFrames = 6;
const int quantidadeDeBlocosPorFrame = 5;
typedef struct bloco
{
    unsigned char blocoDeVerdade[tamanhoDoBloco][tamanhoDoBloco];
    int x;
    int y;
} bloco;

typedef struct correspondencia
{
    int indiceFrame;
    int xReferencia;
    int yReferencia;
    int xAtual;
    int yAtual;

} correspondencia;

correspondencia manipula(bloco a, int i); // Função que vai fazer algum tipo de manipulação do dado

int main(int argc, char **argv)
{
    int quantidade_de_maquinas, meu_codigo;

    bloco blocosArray[quantidadeDeFrames] = {}; // Vetor de entrada
    for (int i = 0; i < quantidadeDeFrames; i++)
    {
        blocosArray[i].x = rand() % 100;
        blocosArray[i].y = rand() % 100;
        blocosArray[i].blocoDeVerdade[0][0] = 'a';
        blocosArray[i].blocoDeVerdade[0][1] = 'b';
        blocosArray[i].blocoDeVerdade[1][0] = 'c';
        blocosArray[i].blocoDeVerdade[1][1] = 'd';
    }
    correspondencia correspondenciaFinal[quantidadeDeFrames*quantidadeDeBlocosPorFrame];

    // Precisa usar a sintaxe de ponteiro. Se usar array NÃO FUNCIONA e não me perguntem porquê.
    int *elementosPorProcesso = (int *)malloc(sizeof(int) * quantidade_de_maquinas); // Quantos processos vão pro processador           // array describing how many elements to send to each process
    int *aPartirDoIndice = (int *)malloc(sizeof(int) * quantidade_de_maquinas);
    aPartirDoIndice[0] = 0;

    // Início do MPI
    MPI_Init(&argc, &argv);                                 // Inicialização do MPI
    MPI_Comm_size(MPI_COMM_WORLD, &quantidade_de_maquinas); // Quantos processos envolvidos?
    MPI_Comm_rank(MPI_COMM_WORLD, &meu_codigo);             // Meu identificador

    // Definição do tipo MPI_Bloco, correspondente à struct bloco
    MPI_Datatype MPI_BLOCO;
    MPI_Datatype types[3] = {MPI_UNSIGNED_CHAR, MPI_INT, MPI_INT};
    int quantidadesDeVariaveis[3] = {tamanhoDoBloco * tamanhoDoBloco, 1, 1};
    MPI_Aint ondeAsVariaveisIniciam[3];
    bloco blocoDisp; // bloco pra calcular o displacement

    MPI_Aint base_address;
    MPI_Get_address(&blocoDisp, &base_address);
    MPI_Get_address(&blocoDisp.blocoDeVerdade[0][0], &ondeAsVariaveisIniciam[0]);
    MPI_Get_address(&blocoDisp.x, &ondeAsVariaveisIniciam[1]);
    MPI_Get_address(&blocoDisp.y, &ondeAsVariaveisIniciam[2]);

    ondeAsVariaveisIniciam[0] = MPI_Aint_diff(ondeAsVariaveisIniciam[0], base_address);
    ondeAsVariaveisIniciam[1] = MPI_Aint_diff(ondeAsVariaveisIniciam[1], base_address);
    ondeAsVariaveisIniciam[2] = MPI_Aint_diff(ondeAsVariaveisIniciam[2], base_address);

    MPI_Type_create_struct(3, quantidadesDeVariaveis, ondeAsVariaveisIniciam, types, &MPI_BLOCO);
    MPI_Type_commit(&MPI_BLOCO);

    // Definição da Struct Correspondencia
    MPI_Datatype MPI_CORRESPONDENCIA;
    MPI_Datatype types2[5] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT,MPI_INT};
    int quantidadesDeVariaveis2[5] = {1, 1, 1, 1,1};
    // unsiged char é 1 byte e int é 5 bytes
    MPI_Aint ondeAsVariaveisIniciam2[5] = {0, 4, 8, 12,16};
    MPI_Type_create_struct(5, quantidadesDeVariaveis2, ondeAsVariaveisIniciam2, types2, &MPI_CORRESPONDENCIA);
    MPI_Type_commit(&MPI_CORRESPONDENCIA);

       int processosResto = quantidadeDeFrames % quantidade_de_maquinas; // Quantidade de resto que é gerada pela quantidade de máquina.
    // Essa variável precisa ficar aqui porque senão quantidade_de_maquinas não tá definido

    // Cria vetor de quantidade de elementosPorProcesso
    for (int i = 0; i < quantidade_de_maquinas; i++)
    {
        elementosPorProcesso[i] = 6 / quantidade_de_maquinas;
        if (processosResto > 0)
        { // Se eu tiver processos com resto, toco pra alguma máquina
            elementosPorProcesso[i]++;
            processosResto--;
        }
    }

    // Cria vetor de deslocamento
    for (int i = 1; i < quantidade_de_maquinas; i++)
    {
        aPartirDoIndice[i] = aPartirDoIndice[i - 1] + elementosPorProcesso[i - 1];
    }

    int tamanhoChunck = elementosPorProcesso[meu_codigo];

    // printf("Chuncks (quantidade de items do array por proceso) no processo %d-> %d\n", meu_codigo, tamanhoChunck);

    bloco blocosAux[tamanhoChunck]; // Vetor auxiliar
    MPI_Scatterv(
        blocosArray,          // Vetor que tem as informações que eu quero mandar pra todo mundo
        elementosPorProcesso, // Quantas posições desse array eu quero mandar pra cada processo
        aPartirDoIndice,      // A partir de qual índice do arquivo original eu quero mandar essas posições
        MPI_BLOCO,            // Tipo do array que quero mandar pra todo mundo
        blocosAux,            // Variável que representa a "fatia" do meu array original que cada processo vai receber
        tamanhoChunck,        // Tamanho da "fatia"
        MPI_BLOCO,            // Tipo dos elementos da "fatia"
        0,                    // Processo de origem
        MPI_COMM_WORLD);

    // processamento
    // em cada processo, eu tenho uma fatia de tamanhoChunck de elementos do meu array.
    // Essa fatia é um array que vai começar em zero e vai até tamanhoChunck-1
    int tamanhoDasCorrespondencias = tamanhoChunck*quantidadeDeBlocosPorFrame;
    correspondencia correspondenciasResultado[tamanhoDasCorrespondencias];
    for (int i = 0; i < tamanhoChunck; i++)
    {
        for (int j = 0; j < quantidadeDeBlocosPorFrame; j++)
        {
        int indice = (1*i)+j;
            
        correspondenciasResultado[indice] = manipula(blocosAux[i],i);
        // printf("editando blocossAux[%d] do processo %d que tem o valor (%d,%d)\n", indice, meu_codigo, correspondenciasResultado[i].xAtual, correspondenciasResultado[i].xReferencia);
        }        
    }
    // for (int i = 0; i < tamanhoChunck*quantidadeDeBlocosPorFrame; i++)
    // {
    //     printf("editando blocossAux[%d] do processo %d que tem o valor (%d,%d)\n", i, meu_codigo, correspondenciasResultado[i].xAtual, correspondenciasResultado[i].xReferencia);
        
    // }
    

    MPI_Gatherv(
        correspondenciasResultado,    // Variável que armazena as "fatias" que eu quero armazenar no meu array de retorno
        tamanhoDasCorrespondencias,   // Quantidades de variáveis que eu tenho na minha fatia e que vou colocar no resultado
        MPI_CORRESPONDENCIA,          // Tipo dos elementos da fatia
        correspondenciaFinal,         // Variavel onde eu vou juntar todas as fatias
        elementosPorProcesso,         // quantidades de elementos dessa fatia que eu quero colocar na variável que armazena todas as fatias
        aPartirDoIndice,              // A partir de qual índice eu coloco as fatias do array BLOCOerno no array resposta
        MPI_CORRESPONDENCIA,          // Tipo dos elementos que da variável que armazena as fatias
        0,                            // Processo pra onde todas as fatias são enviadas
        MPI_COMM_WORLD);

    if (meu_codigo == 0)
    {
        // Print do array numerosPorProcesso
        for (int i = 0; i < quantidade_de_maquinas; i++)
        {
            // printf("numerosPorProcesso[%d] = %d\n", i, elementosPorProcesso[i]);
        }
        // Print do array de deslocamento
        for (int i = 0; i < quantidade_de_maquinas; i++)
        {
            // printf("deslocamento[%d] = %d\n", i, aPartirDoIndice[i]);
        }
        // Print do resultado final
        for (int i = 0; i < quantidadeDeFrames*quantidadeDeBlocosPorFrame; i++)
        {
            printf("resultado[%d] = (%d,%d)=>(%d,%d)\n", correspondenciaFinal[i].indiceFrame, correspondenciaFinal[i].xAtual, correspondenciaFinal[i].yAtual, correspondenciaFinal[i].xReferencia, correspondenciaFinal[i].yReferencia);
        }
        printf("Processo 0\n");
    }
    MPI_Finalize(); // Finalização
    free(elementosPorProcesso);
    free(aPartirDoIndice);
}

correspondencia manipula(bloco a, int i)
{
    correspondencia c;
    c.indiceFrame = i;
    c.xAtual = a.x+i;
    c.yAtual = a.y;
    c.xReferencia = a.x + a.y +i;
    c.yReferencia = a.x - a.y;
    return c;
}
