
#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <string.h>

// Quantidade máxima de clientes que o servidor pode atender
#define MAX_CLIENTES 10

// Tamanho máximo do bloco de dados recebido em cada leitura
#define TAM_MENSAGEM 1024


// ==========================================================
// ESTRUTURA DE DADOS DE CADA CLIENTE
// ==========================================================

// Esta estrutura armazena as informações necessárias
// para que a thread consiga trabalhar com seu cliente.
typedef struct
{
    SOCKET socketCliente; // Socket utilizado para comunicar com o cliente
    int numeroCliente;    // Número de identificação do cliente
    int posicao;          // Posição do cliente nos vetores globais
    int ativo;            // Indica se o cliente está conectado (1) ou não (0)

} DadosThread;


// ==========================================================
// VARIÁVEIS GLOBAIS
// ==========================================================

// Vetor que armazena os sockets de todos os clientes conectados
SOCKET clientes[MAX_CLIENTES];

// Vetor que armazena os identificadores das threads dos clientes
HANDLE threads[MAX_CLIENTES];

// Vetor que armazena os dados individuais de cada thread
DadosThread dados[MAX_CLIENTES];

// Quantidade atual de clientes conectados
int totalClientes = 0;

// Estrutura utilizada para controlar o acesso simultâneo
// às variáveis compartilhadas entre as threads.
CRITICAL_SECTION travaClientes;


// ==========================================================
// FUNÇÃO DE RECEBIMENTO E RETRANSMISSÃO DE MENSAGENS
// ==========================================================

// Cada cliente possui uma thread executando esta função.
//
// A função recebe mensagens do cliente associado à thread
// e retransmite os dados para todos os demais clientes.
DWORD WINAPI encaminharMensagens(LPVOID parametro)
{
    // Converte o parâmetro recebido para a estrutura DadosThread
    DadosThread *dadosCliente = (DadosThread*)parametro;

    // Buffer utilizado para armazenar as mensagens recebidas
    char mensagem[TAM_MENSAGEM];

    // Variáveis utilizadas no recebimento e envio dos dados
    int bytesRecebidos;
    int bytesEnviados;
    int totalEnviado;
    int i;

    // Guarda as informações do cliente atendido por esta thread
    SOCKET meuSocket = dadosCliente->socketCliente;
    int minhaPosicao = dadosCliente->posicao;
    int meuNumero = dadosCliente->numeroCliente;


    // Mantém a comunicação enquanto o cliente estiver conectado
    while (1)
    {
        // Aguarda receber dados enviados pelo cliente
        bytesRecebidos = recv(
            meuSocket,
            mensagem,
            sizeof(mensagem),
            0
        );

        // Se recebeu dados corretamente, entra neste bloco
        if (bytesRecebidos > 0)
        {
            printf("\n[CLIENTE %d] Mensagem recebida.\n", meuNumero);

            // Bloqueia o acesso às informações compartilhadas.
            // Isso evita que duas threads modifiquem os dados
            // dos clientes simultaneamente.
            EnterCriticalSection(&travaClientes);

            // Percorre todas as posições do vetor de clientes
            for (i = 0; i < MAX_CLIENTES; i++)
            {
                // Verifica se existe um cliente conectado
                // e se ele não é o próprio remetente da mensagem.
                if (clientes[i] != INVALID_SOCKET &&
                    clientes[i] != meuSocket)
                {
                    // Reinicia o contador de bytes enviados
                    totalEnviado = 0;

                    // Continua enviando até transmitir todos os dados
                    while (totalEnviado < bytesRecebidos)
                    {
                        // Envia a mensagem para o outro cliente
                        bytesEnviados = send(
                            clientes[i],
                            mensagem + totalEnviado,
                            bytesRecebidos - totalEnviado,
                            0
                        );

                        // Verifica se ocorreu algum erro no envio
                        // ou se nenhum byte foi enviado.
                        if (bytesEnviados == SOCKET_ERROR ||
                            bytesEnviados == 0)
                        {
                            break;
                        }

                        // Atualiza a quantidade de bytes transmitidos
                        totalEnviado += bytesEnviados;
                    }
                }
            }

            // Libera o acesso às variáveis compartilhadas
            LeaveCriticalSection(&travaClientes);
        }

        // Se recv retornar zero ou erro, o cliente desconectou
        // ou ocorreu uma falha na comunicação.
        else
        {
            // Retorno zero indica encerramento normal da conexão
            if (bytesRecebidos == 0)
            {
                printf("\n[INFO] Cliente %d desconectou.\n", meuNumero);
            }

            // Retorno negativo indica erro na comunicação
            else
            {
                printf(
                    "\n[ERRO] Falha na comunicacao com cliente %d: %d\n",
                    meuNumero,
                    WSAGetLastError()
                );
            }

            // Interrompe o loop de comunicação
            break;
        }
    }


    // ======================================================
    // REMOÇÃO DO CLIENTE DESCONECTADO
    // ======================================================

    // Bloqueia o acesso aos dados antes de modificá-los
    EnterCriticalSection(&travaClientes);

    // Confirma se a posição ainda pertence a este cliente
    if (clientes[minhaPosicao] == meuSocket)
    {
        // Marca a posição do cliente como disponível
        clientes[minhaPosicao] = INVALID_SOCKET;

        // Informa que o cliente não está mais ativo
        dadosCliente->ativo = 0;

        // Atualiza a quantidade de clientes conectados
        totalClientes--;

        // Fecha o socket utilizado pelo cliente
        closesocket(meuSocket);

        // Fecha o identificador da thread e libera sua posição
        if (threads[minhaPosicao] != NULL)
        {
            CloseHandle(threads[minhaPosicao]);
            threads[minhaPosicao] = NULL;
        }

        // Exibe a quantidade atualizada de clientes conectados
        printf(
            "[INFO] Cliente %d removido. Clientes conectados: %d\n",
            meuNumero,
            totalClientes
        );
    }

    // Libera o acesso às variáveis compartilhadas
    LeaveCriticalSection(&travaClientes);

    // Finaliza a execução da thread
    return 0;
}


// ==========================================================
// FUNÇÃO PRINCIPAL
// ==========================================================

int main()
{
    // Estrutura necessária para inicializar o Winsock
    WSADATA wsa;

    // Socket principal do servidor
    SOCKET servidor;

    // Socket criado para cada nova conexão recebida
    SOCKET novoCliente;

    // Estrutura que armazena o endereço e a porta do servidor
    struct sockaddr_in endereco_servidor;

    // Identificador da nova thread criada
    HANDLE novaThread;

    // Variáveis auxiliares
    int i;
    int posicaoLivre;


    // ======================================================
    // INICIALIZAÇÃO DOS VETORES
    // ======================================================

    // Inicializa todas as posições dos vetores
    for (i = 0; i < MAX_CLIENTES; i++)
    {
        // Indica que nenhuma conexão está ocupando a posição
        clientes[i] = INVALID_SOCKET;

        // Inicialmente, nenhuma thread está associada à posição
        threads[i] = NULL;

        // Marca o cliente como inativo
        dados[i].ativo = 0;

        // Inicializa o socket como inválido
        dados[i].socketCliente = INVALID_SOCKET;

        // Define a posição correspondente no vetor
        dados[i].posicao = i;
    }

    // Inicializa a estrutura de controle de acesso simultâneo
    InitializeCriticalSection(&travaClientes);


    // ======================================================
    // APRESENTAÇÃO DO SERVIDOR
    // ======================================================

    printf("=================================\n");
    printf("       SERVIDOR TCP - CHAT       \n");
    printf("=================================\n\n");


    // ======================================================
    // INICIALIZAÇÃO DO WINSOCK
    // ======================================================

    // Inicializa a biblioteca Winsock, responsável
    // pela comunicação de rede no Windows.
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("[ERRO] Nao foi possivel inicializar o Winsock.\n");

        DeleteCriticalSection(&travaClientes);
        return 1;
    }

    printf("[OK] Winsock inicializado.\n");


    // ======================================================
    // CRIAÇÃO DO SOCKET DO SERVIDOR
    // ======================================================

    // Cria um socket utilizando:
    // AF_INET     = protocolo de endereçamento IPv4
    // SOCK_STREAM = comunicação orientada à conexão
    // 0           = utiliza o protocolo padrão (TCP)
    servidor = socket(AF_INET, SOCK_STREAM, 0);

    // Verifica se o socket foi criado corretamente
    if (servidor == INVALID_SOCKET)
    {
        printf("[ERRO] Nao foi possivel criar o socket.\n");

        WSACleanup();
        DeleteCriticalSection(&travaClientes);

        return 1;
    }

    printf("[OK] Socket TCP criado.\n");


    // ======================================================
    // CONFIGURAÇÃO DO ENDEREÇO DO SERVIDOR
    // ======================================================

    // Limpa a estrutura antes de preencher seus campos
    memset(&endereco_servidor, 0, sizeof(endereco_servidor));

    // Define que será utilizado o protocolo IPv4
    endereco_servidor.sin_family = AF_INET;

    // INADDR_ANY permite que o servidor aceite conexões
    // por qualquer interface de rede disponível.
    endereco_servidor.sin_addr.s_addr = htonl(INADDR_ANY);

    // Define a porta 5000 para receber as conexões
    endereco_servidor.sin_port = htons(5000);


    // ======================================================
    // ASSOCIAÇÃO DO SOCKET AO ENDEREÇO E À PORTA
    // ======================================================

    // O bind associa o socket à porta e ao endereço configurados
    if (bind(
        servidor,
        (struct sockaddr*)&endereco_servidor,
        sizeof(endereco_servidor)
    ) == SOCKET_ERROR)
    {
        printf("[ERRO] Nao foi possivel associar o socket.\n");

        closesocket(servidor);
        WSACleanup();
        DeleteCriticalSection(&travaClientes);

        return 1;
    }

    printf("[OK] Servidor associado a todas as interfaces de rede.\n");
    printf("[OK] Porta utilizada: 5000.\n");


    // ======================================================
    // COLOCA O SERVIDOR EM MODO DE ESCUTA
    // ======================================================

    // listen permite que o servidor aguarde novas conexões.
    // O valor 5 representa a fila de conexões pendentes,
    // não a quantidade máxima de clientes conectados.
    if (listen(servidor, 5) == SOCKET_ERROR)
    {
        printf("[ERRO] Nao foi possivel iniciar a escuta.\n");

        closesocket(servidor);
        WSACleanup();
        DeleteCriticalSection(&travaClientes);

        return 1;
    }

    printf("[OK] Servidor esta escutando conexoes.\n");
    printf("\nAguardando clientes...\n");


    // ======================================================
    // LOOP PRINCIPAL DE ACEITAÇÃO DE CLIENTES
    // ======================================================

    // Mantém o servidor funcionando continuamente,
    // permitindo que novos clientes se conectem.
    while (1)
    {
        // Aguarda uma nova conexão de um cliente.
        // Quando alguém se conecta, accept retorna um novo socket.
        novoCliente = accept(servidor, NULL, NULL);

        // Verifica se a conexão foi aceita corretamente
        if (novoCliente == INVALID_SOCKET)
        {
            printf("[ERRO] Falha ao aceitar conexao: %d\n",
                   WSAGetLastError());

            continue;
        }

        // Inicialmente, considera que não existe vaga disponível
        posicaoLivre = -1;


        // ==================================================
        // PROCURA UMA POSIÇÃO LIVRE PARA O NOVO CLIENTE
        // ==================================================

        // Bloqueia o acesso aos vetores durante a procura
        EnterCriticalSection(&travaClientes);

        // Percorre o vetor procurando uma posição sem cliente
        for (i = 0; i < MAX_CLIENTES; i++)
        {
            if (clientes[i] == INVALID_SOCKET)
            {
                posicaoLivre = i;
                break;
            }
        }

        // Se nenhuma posição estiver disponível, o servidor
        // já atingiu sua capacidade máxima de clientes.
        if (posicaoLivre == -1)
        {
            LeaveCriticalSection(&travaClientes);

            printf("[AVISO] Servidor cheio! Conexao recusada.\n");

            // Fecha a conexão que não pôde ser aceita
            closesocket(novoCliente);

            continue;
        }


        // ==================================================
        // REGISTRO DO NOVO CLIENTE
        // ==================================================

        // Armazena o socket do novo cliente no vetor
        clientes[posicaoLivre] = novoCliente;

        // Preenche os dados individuais do cliente
        dados[posicaoLivre].socketCliente = novoCliente;

        // Define um número de identificação para o cliente
        dados[posicaoLivre].numeroCliente = posicaoLivre + 1;

        // Armazena sua posição no vetor
        dados[posicaoLivre].posicao = posicaoLivre;

        // Marca o cliente como ativo
        dados[posicaoLivre].ativo = 1;


        // ==================================================
        // CRIAÇÃO DA THREAD DO CLIENTE
        // ==================================================

        // Cria uma thread responsável pela comunicação
        // individual com o cliente recém-conectado.
        //
        // CREATE_SUSPENDED faz com que a thread seja criada,
        // mas não comece a executar imediatamente.
        //
        // Isso permite guardar seu identificador no vetor
        // antes que a função da thread comece a trabalhar.
        novaThread = CreateThread(
            NULL,
            0,
            encaminharMensagens,
            &dados[posicaoLivre],
            CREATE_SUSPENDED,
            NULL
        );

        // Verifica se a criação da thread falhou
        if (novaThread == NULL)
        {
            printf("[ERRO] Falha ao criar thread do cliente.\n");

            // Desfaz o registro do cliente
            clientes[posicaoLivre] = INVALID_SOCKET;
            dados[posicaoLivre].ativo = 0;
            dados[posicaoLivre].socketCliente = INVALID_SOCKET;

            LeaveCriticalSection(&travaClientes);

            // Fecha o socket da conexão que não pôde ser atendida
            closesocket(novoCliente);

            continue;
        }


        // ==================================================
        // REGISTRO DA THREAD E ATUALIZAÇÃO DOS CLIENTES
        // ==================================================

        // Guarda o identificador da thread no vetor
        threads[posicaoLivre] = novaThread;

        // Atualiza a quantidade de clientes conectados
        totalClientes++;

        // Exibe informações sobre a nova conexão
        printf("\n[OK] Novo cliente conectado!\n");

        printf("[INFO] Cliente %d registrado.\n",
               dados[posicaoLivre].numeroCliente);

        printf("[INFO] Clientes conectados: %d/%d\n",
               totalClientes,
               MAX_CLIENTES);

        // Libera o acesso aos vetores e variáveis compartilhadas
        LeaveCriticalSection(&travaClientes);


        // ==================================================
        // INICIA A EXECUÇÃO DA THREAD
        // ==================================================

        // ResumeThread libera a thread que foi criada suspensa.
        // A partir deste momento, ela começa a executar
        // a função encaminharMensagens().
        if (ResumeThread(novaThread) == (DWORD)-1)
        {
            printf("[ERRO] Nao foi possivel iniciar a thread.\n");

            // Bloqueia o acesso aos dados para desfazer o registro
            EnterCriticalSection(&travaClientes);

            // Confirma se a posição ainda pertence a este cliente
            if (clientes[posicaoLivre] == novoCliente)
            {
                // Remove o cliente dos vetores
                clientes[posicaoLivre] = INVALID_SOCKET;
                dados[posicaoLivre].ativo = 0;
                dados[posicaoLivre].socketCliente = INVALID_SOCKET;

                // Atualiza a quantidade de clientes conectados
                totalClientes--;

                // Fecha o identificador da thread
                CloseHandle(threads[posicaoLivre]);
                threads[posicaoLivre] = NULL;
            }

            LeaveCriticalSection(&travaClientes);

            // Fecha o socket do cliente
            closesocket(novoCliente);
        }
    }


    // ======================================================
    // ENCERRAMENTO DO SERVIDOR
    // ======================================================

    // Este trecho seria executado caso o loop principal
    // fosse encerrado. Como o servidor utiliza while(1),
    // esse encerramento ainda não é acionado normalmente.
    closesocket(servidor);

    // Finaliza a biblioteca Winsock
    WSACleanup();

    // Libera a estrutura de controle de acesso simultâneo
    DeleteCriticalSection(&travaClientes);

    return 0;
}

