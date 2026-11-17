
#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <string.h>

#define TAM_MENSAGEM 1024

// =============================================
// FUNCAO PARA VALIDAR O ENDERECO IPv4
// =============================================

int validarIP(char ip[])
{
    unsigned int octeto1, octeto2, octeto3, octeto4;
    char extra;

    int resultado = sscanf(
        ip,
        "%u.%u.%u.%u%c",
        &octeto1,
        &octeto2,
        &octeto3,
        &octeto4,
        &extra
    );

    if (resultado != 4)
    {
        return 0;
    }

    if (octeto1 > 255 || octeto2 > 255 ||
        octeto3 > 255 || octeto4 > 255)
    {
        return 0;
    }

    return 1;
}


// =============================================
// FUNCAO PARA ENVIAR MENSAGENS
// =============================================

int enviarMensagem(SOCKET cliente, const char *mensagem, int tamanho)
{
    char mensagemComDelimitador[TAM_MENSAGEM + 1];

    int totalEnviado = 0;
    int bytesEnviados;
    int tamanhoTotal;

    if (tamanho > TAM_MENSAGEM)
    {
        return 0;
    }

    // Copia a mensagem original
    memcpy(mensagemComDelimitador, mensagem, tamanho);

    // Adiciona a quebra de linha
    mensagemComDelimitador[tamanho] = '\n';

    tamanhoTotal = tamanho + 1;

    // Envia todos os bytes
    while (totalEnviado < tamanhoTotal)
    {
        bytesEnviados = send(
            cliente,
            mensagemComDelimitador + totalEnviado,
            tamanhoTotal - totalEnviado,
            0
        );

        if (bytesEnviados == SOCKET_ERROR || bytesEnviados == 0)
        {
            return 0;
        }

        totalEnviado += bytesEnviados;
    }

    return 1;
}


// =============================================
// THREAD RESPONSAVEL POR RECEBER MENSAGENS
// =============================================

DWORD WINAPI receberMensagens(LPVOID parametro)
{
    SOCKET cliente = *(SOCKET*)parametro;

    char bloco[TAM_MENSAGEM];
    char linha[TAM_MENSAGEM * 2];

    int bytesRecebidos;
    int tamanhoLinha = 0;
    int i;

    while (1)
    {
        bytesRecebidos = recv(
            cliente,
            bloco,
            sizeof(bloco),
            0
        );

        if (bytesRecebidos > 0)
        {
            // Processa cada byte recebido
            for (i = 0; i < bytesRecebidos; i++)
            {
                if (bloco[i] == '\n')
                {
                    linha[tamanhoLinha] = '\0';

                    // Remove possivel retorno de carro
                    if (tamanhoLinha > 0 &&
                        linha[tamanhoLinha - 1] == '\r')
                    {
                        linha[tamanhoLinha - 1] = '\0';
                    }

                    printf("\n%s\n", linha);
                    printf("\nDigite sua mensagem: ");
                    fflush(stdout);

                    tamanhoLinha = 0;
                }
                else
                {
                    if (tamanhoLinha < sizeof(linha) - 1)
                    {
                        linha[tamanhoLinha] = bloco[i];
                        tamanhoLinha++;
                    }
                    else
                    {
                        tamanhoLinha = 0;

                        printf("\n[AVISO] Mensagem muito grande.\n");
                    }
                }
            }
        }
        else if (bytesRecebidos == 0)
        {
            printf("\n[INFO] Servidor encerrou a conexao.\n");
            break;
        }
        else
        {
            printf("\n[ERRO] Falha ao receber mensagem: %d\n",
                   WSAGetLastError());
            break;
        }
    }

    return 0;
}


// =============================================
// FUNCAO PRINCIPAL
// =============================================

int main()
{
    WSADATA wsa;

    SOCKET cliente;
    HANDLE threadRecebimento;

    struct sockaddr_in endereco_servidor;

    char nickname[50];
    char ipServidor[32];

    char mensagem[TAM_MENSAGEM];
    char mensagemFormatada[TAM_MENSAGEM];

    int tamanhoMensagem;
    int resultadoEnvio;

    int caractere;
    int erro;

    printf("=================================\n");
    printf("        CLIENTE TCP - CHAT       \n");
    printf("=================================\n\n");


    // =========================================
    // 1. SOLICITA O NICKNAME
    // =========================================

    printf("Digite seu nickname: ");

    if (fgets(nickname, sizeof(nickname), stdin) == NULL)
    {
        return 1;
    }

    if (strchr(nickname, '\n') == NULL)
    {
        while ((caractere = getchar()) != '\n' &&
               caractere != EOF)
        {
        }

        printf("[ERRO] Nickname muito longo.\n");
        return 1;
    }

    nickname[strcspn(nickname, "\n")] = '\0';

    if (strlen(nickname) == 0)
    {
        printf("[ERRO] O nickname nao pode ficar vazio.\n");
        return 1;
    }


    // =========================================
    // 2. SOLICITA O IP DO SERVIDOR
    // =========================================

    
    // =========================================
    // 2. SOLICITA E VALIDA O IP DO SERVIDOR
    // =========================================

    while (1)
    {
        printf("\nDigite o IP do servidor: ");

        if (fgets(ipServidor, sizeof(ipServidor), stdin) == NULL)
        {
            printf("[ERRO] Nao foi possivel ler o endereco IP.\n");
            return 1;
        }

        // Verifica se o endereco foi digitado por completo
        if (strchr(ipServidor, '\n') == NULL)
        {
            while ((caractere = getchar()) != '\n' &&
                   caractere != EOF)
            {
            }

            printf("[ERRO] Endereco IP muito longo.\n");
            continue;
        }

        // Remove a quebra de linha
        ipServidor[strcspn(ipServidor, "\n")] = '\0';

        // Verifica se o IP e valido
        if (!validarIP(ipServidor))
        {
            printf("\n[ERRO] Endereco IP invalido!\n");
            printf("[INFO] Exemplo de IP valido: 192.168.1.100\n");

            continue;
        }

        printf("\n[OK] Endereco IP valido: %s\n", ipServidor);

        break;
    }



    // =========================================
    // 3. VALIDA O ENDERECO IP
    // =========================================

    if (!validarIP(ipServidor))
    {
        printf("\n[ERRO] Endereco IP invalido!\n");
        printf("[INFO] Informe um endereco IPv4 valido.\n");

        return 1;
    }

    printf("\n[OK] Endereco IP valido: %s\n", ipServidor);


    // =========================================
    // 4. INICIALIZA O WINSOCK
    // =========================================

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("[ERRO] Nao foi possivel inicializar o Winsock.\n");
        return 1;
    }

    printf("[OK] Winsock inicializado.\n");


    // =========================================
    // 5. CRIA O SOCKET TCP
    // =========================================

    cliente = socket(AF_INET, SOCK_STREAM, 0);

    if (cliente == INVALID_SOCKET)
    {
        printf("[ERRO] Nao foi possivel criar o socket.\n");

        WSACleanup();
        return 1;
    }

    printf("[OK] Socket TCP criado.\n");


    // =========================================
    // 6. CONFIGURA O ENDERECO DO SERVIDOR
    // =========================================

    memset(&endereco_servidor, 0, sizeof(endereco_servidor));

    endereco_servidor.sin_family = AF_INET;

    endereco_servidor.sin_addr.s_addr = inet_addr(ipServidor);

    endereco_servidor.sin_port = htons(5000);


    // =========================================
    // 7. CONECTA AO SERVIDOR
    // =========================================

    printf("\nConectando ao servidor %s...\n", ipServidor);

    if (connect(
        cliente,
        (struct sockaddr*)&endereco_servidor,
        sizeof(endereco_servidor)
    ) == SOCKET_ERROR)
    {
        erro = WSAGetLastError();

        printf("\n[ERRO] Nao foi possivel conectar ao servidor.\n");

        switch (erro)
        {
            case WSAECONNREFUSED:
                printf("[INFO] O servidor recusou a conexao.\n");
                printf("[INFO] Verifique se o servidor esta iniciado.\n");
                break;

            case WSAETIMEDOUT:
                printf("[INFO] Tempo limite de conexao excedido.\n");
                printf("[INFO] Verifique a rede e o firewall.\n");
                break;

            case WSAENETUNREACH:
                printf("[INFO] Rede de destino inacessivel.\n");
                break;

            case WSAEHOSTUNREACH:
                printf("[INFO] Computador servidor inacessivel.\n");
                break;

            default:
                printf("[INFO] Erro de comunicacao: %d\n", erro);
                break;
        }

        closesocket(cliente);
        WSACleanup();

        return 1;
    }

    printf("[OK] Conectado ao servidor!\n");


    // =========================================
    // 8. CRIA A THREAD DE RECEBIMENTO
    // =========================================

    threadRecebimento = CreateThread(
        NULL,
        0,
        receberMensagens,
        &cliente,
        0,
        NULL
    );

    if (threadRecebimento == NULL)
    {
        printf("[ERRO] Nao foi possivel criar a thread.\n");

        closesocket(cliente);
        WSACleanup();

        return 1;
    }

    printf("[OK] Thread de recebimento iniciada!\n");


    // =========================================
    // 9. INFORMA A ENTRADA NO CHAT
    // =========================================

    snprintf(
        mensagemFormatada,
        sizeof(mensagemFormatada),
        "[INFO] %s entrou no chat!",
        nickname
    );

    tamanhoMensagem = (int)strlen(mensagemFormatada);

    if (!enviarMensagem(cliente, mensagemFormatada, tamanhoMensagem))
    {
        printf("[ERRO] Falha ao informar entrada no chat.\n");
        goto encerrar;
    }

    printf("\n=================================\n");
    printf("          CHAT INICIADO!         \n");
    printf("=================================\n");


    // =========================================
    // 10. ENVIA E PROCESSA AS MENSAGENS
    // =========================================

    while (1)
    {
        printf("\nDigite sua mensagem (/sair para encerrar): ");

        if (fgets(mensagem, sizeof(mensagem), stdin) == NULL)
        {
            break;
        }

        mensagem[strcspn(mensagem, "\n")] = '\0';

        // Ignora mensagens vazias
        if (strlen(mensagem) == 0)
        {
            continue;
        }


        // Verifica o comando para sair
        if (strcmp(mensagem, "/sair") == 0)
        {
            snprintf(
                mensagemFormatada,
                sizeof(mensagemFormatada),
                "[INFO] %s saiu do chat.",
                nickname
            );

            tamanhoMensagem = (int)strlen(mensagemFormatada);

            enviarMensagem(cliente, mensagemFormatada, tamanhoMensagem);

            printf("\n[INFO] Voce encerrou o chat.\n");

            break;
        }


        // Formata a mensagem com o nickname
        snprintf(
            mensagemFormatada,
            sizeof(mensagemFormatada),
            "[%s]: %s",
            nickname,
            mensagem
        );

        tamanhoMensagem = (int)strlen(mensagemFormatada);

        // Envia a mensagem
        resultadoEnvio = enviarMensagem(
            cliente,
            mensagemFormatada,
            tamanhoMensagem
        );

        if (!resultadoEnvio)
        {
            printf("[ERRO] Falha ao enviar mensagem.\n");
            break;
        }
    }


    // =========================================
    // 11. ENCERRAMENTO DA CONEXAO
    // =========================================

encerrar:

    printf("\nEncerrando conexao...\n");

    shutdown(cliente, SD_BOTH);

    // Aguarda a thread de recebimento terminar
    WaitForSingleObject(threadRecebimento, INFINITE);

    // Libera o identificador da thread
    CloseHandle(threadRecebimento);

    // Fecha o socket
    closesocket(cliente);

    // Encerra o Winsock
    WSACleanup();

    printf("[OK] Cliente encerrado.\n");

    return 0;
}

