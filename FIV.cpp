#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include "FTRApi.h"
#include <stdlib.h>

using namespace std;

#pragma comment(lib,"ws2_32.lib")

#define PORT 8081

FTR_DATA newFingerprintTemplate;

void CheckError(int returnCode, const char* location) {

    if (returnCode == SOCKET_ERROR) {
        int errorCode = WSAGetLastError();
        cerr << "Error " << errorCode << " in " << location << endl;
        WSACleanup();
        exit(1);
    }
}
// Función callback para interactuar con el usuario
void OnStateControl(FTR_USER_CTX ctx, FTR_STATE state, FTR_RESPONSE* response, FTR_SIGNAL signal, FTR_BITMAP_PTR bitmap) {
    switch (signal) {
    case FTR_SIGNAL_TOUCH_SENSOR:
        printf("Por favor ponga su dedo en el lector\n");
        break;
    case FTR_SIGNAL_TAKE_OFF:
        printf("Por favor retire su dedo del lector\n");
        break;
    }
}
void captureFingerprint() {
    DWORD value = FSD_FUTRONIC_USB;
    if (newFingerprintTemplate.pData != NULL) {
        delete[] newFingerprintTemplate.pData;
    }

    // Establece origen de frames como dispositivo USB Futronic
    FTRSetParam(FTR_PARAM_CB_FRAME_SOURCE, (FTR_PARAM_VALUE)value);

    // Obtiene tamaño del frame

    FTR_PARAM_VALUE  frameSize;
    FTRGetParam(FTR_PARAM_IMAGE_SIZE, &frameSize);
    // Función de callback  
    FTRSetParam(FTR_PARAM_CB_CONTROL, OnStateControl);

    int FRAME_WIDTH = 500;
    int FRAME_HEIGHT = 500;
    int intframesize = FRAME_WIDTH * FRAME_HEIGHT;
    // Reserva memoria  
    BYTE* frameBuffer = new BYTE[intframesize];

    // Template
    newFingerprintTemplate.pData = new BYTE[7000];

    // Captura 5 frames
    for (int i = 0; i < 5; i++) {

        FTRCaptureFrame(NULL, frameBuffer);

    }

    FTREnroll(NULL, FTR_PURPOSE_ENROLL, &newFingerprintTemplate);

    delete[] frameBuffer;
}


int main() {

    WSAData wsaData;
    if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CheckError(listenSocket, "socket()");

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress))) {
        CheckError(bind(listenSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)), "bind()");
    }

    if (SOCKET_ERROR == listen(listenSocket, SOMAXCONN)) {
        CheckError(listen(listenSocket, SOMAXCONN), "listen()");
    }

    if (FTRInitialize() != FTR_RETCODE_OK) {
        return 1;
    }

    cout << "Servidor iniciado en puerto " << PORT << endl;

    SOCKET clientSocket;

    while ((clientSocket = accept(listenSocket, NULL, NULL)))
    {
        char buffer[256];
        int bytesRec = recv(clientSocket, buffer, 256, 0);
        string request(buffer, bytesRec);

        if (bytesRec == 0 || bytesRec == SOCKET_ERROR) {
            cerr << "Error recibiendo datos" << endl;
            continue;
        }

        string mensaje = "NADA";
        cout << "Request: " << request << endl;

        if (request == "enroll") {
            captureFingerprint();
            int size = newFingerprintTemplate.dwSize;

            // Envía puntero 
            send(clientSocket,
                (char*)newFingerprintTemplate.pData,
                size, 0);
            system("cls");

        }
        else if (request == "verify") {
            DWORD value = FSD_FUTRONIC_USB;
            printf("Proceso de verificacion\n");
            // Buffer para recibir template
            byte* buffer = new byte[16000];

            // Recibe template del socket  
            int bytes = recv(clientSocket, (char*)buffer, 16000, 0);

            // Construye objeto FTR_DATA  
            FTR_DATA existingTemplate;
            existingTemplate.pData = buffer;
            existingTemplate.dwSize = bytes;
            FTRAPI_RESULT res = FTRSetBaseTemplate(&existingTemplate);

            if (res != FTR_RETCODE_OK) {
                // Template inválido! Manejar error
                cout << "Error: Template invalido " << endl;
            }
            else {
                FTR_FAR  match;
                DGTBOOL vResult;

                // Establece origen de frames como dispositivo USB Futronic
                FTRSetParam(FTR_PARAM_CB_FRAME_SOURCE, (FTR_PARAM_VALUE)value);

                // Obtiene tamaño del frame

                FTR_PARAM_VALUE  frameSize;
                FTRGetParam(FTR_PARAM_IMAGE_SIZE, &frameSize);
                // Función de callback  
                FTRSetParam(FTR_PARAM_CB_CONTROL, OnStateControl);
                FTRAPI_RESULT result = FTRVerify(NULL,
                    &existingTemplate,
                    &vResult,
                    &match);


                if (result != FTR_RETCODE_OK) {
                    cout << "Error en FTRVerify: " << result << endl;
                }
                bool verified = vResult;
                char buffer_verified[1];

                if (verified) {

                    buffer_verified[0] = 1; // Serializado true  

                }
                else {

                    buffer_verified[0] = 0; // Serializado false

                }

                // Envía 1 byte bool
                int sent = send(clientSocket, buffer_verified, 1, 0);

                if (sent != 1) {

                    cout << "Error enviando" << endl;

                }
                else {

                    if (verified) {

                        cout << "Envio resultado true";

                    }
                    else {

                        cout << "Envio resultado false";
                    }

                }
                if (vResult) {
                    send(clientSocket, (char*)true, 4, 0);
                    cout << "FTRVerify: true " << endl;
                }
                else {
                    send(clientSocket, (char*)false, 4, 0);
                    cout << "FTRVerify: false " << endl;
                }
                //send(clientSocket, &isMatched, 1);
                //system("cls");
            }




        }

        int bytesSent = 0;
        int totalBytes = mensaje.size() + 1;


        shutdown(clientSocket, SD_SEND);
        closesocket(clientSocket);
    }

    WSACleanup();
    return 0;
}