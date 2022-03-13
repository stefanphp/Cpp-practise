#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "hash.hpp"
#pragma comment(lib, "Ws2_32.lib")


void ws_handshake(SOCKET& user, const std::string ip, unsigned int port)
{
    char magickey[37] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char needle[20] = "Sec-WebSocket-Key: ";
    char buff[512] = "";
    recv(user, buff, 512, 0);
    const std::string header(buff);
    std::string key = header.substr(header.find(needle)+19, 24);
    const char* input = "";
    input = (key.append(magickey)).c_str();
    char out[28] = "";
    WebSocketHandshake::generate(input, out);
    std::ostringstream response;
    response << "HTTP/1.1 101 Web Socket Protocol Handshake\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "WebSocket-Location: ws://" << ip << ":" << port << "\r\n";
    response << "Sec-WebSocket-Accept: " << out << "\r\n\r\n";
    send(user, response.str().c_str(), response.str().length(), 0);
    //std::cout << response.str();
}

void ws_unmask(std::string& buff)
{
    int length = buff[1] & 127;
    std::string decoded;
    std::string mask;
    std::string data;
    
    try
    {
        if (128-length == 126) {
            mask = buff.substr(4, 4);
            data = buff.substr(8);
        }
        else if (128 - length == 127) {
            mask = buff.substr(10, 4);
            data = buff.substr(14);
        }
        else {
            mask = buff.substr(2, 4);
            data = buff.substr(6);
        }
        
        for (int i = 0; i < length; ++i)
            buff[i] = data[i] ^ mask[i % 4];        
    }
    catch (const std::out_of_range& err) {
        std::cout << "Error: " << err.what();
    }
}

void ws_mask()
{
/*
    length = len(text);
    if length <= 125:
        header = struct.pack('BB', 129, length) # php CC
    elif length > 125 and length < 65536:
        header = struct.pack('BBH', 129, 126, length) # CCn
    elif(length >= 65536):
        header = struct.pack('BBLL', 129, 127, length) # CCNN 
    return header + text
*/
}

void ws_write()
{

}

void ws_read()
{

}

SOCKET setup_socket(const std::string& ip, uint16_t port)
{       
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    const char opt = 1;
    SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    addr.sin_port = htons(port);
    bind(soc, (sockaddr*)&addr, sizeof(addr));
    listen(soc, 4);
    std::cout << "Listening on " << ip << ':' << port << ".\r\n";
    return soc;
}


int main(int argc, char** argv)
{    
    int port = 8080;
    const char ip[16] = "192.168.1.20";
    SOCKET soc = setup_socket(ip, port);

    fd_set master;
    fd_set masterCpy;
    FD_ZERO(&master);
    FD_ZERO(&masterCpy);
    FD_SET(soc, &master);

    sockaddr_in new_addr{};
    int new_addrsz = sizeof(new_addr);
     
    char welcome_msg[] = "Welcome!\r\nServer is WIP.\n\n";
    char buff[4096], infobuff[128] = "";
    std::ostringstream out;
    int buff_sz = 0;

    while (1)
    {         
        masterCpy = master;
        select(0, &masterCpy, NULL, NULL, 0);
        ZeroMemory(&new_addr, sizeof(new_addr));
        ZeroMemory(infobuff, 128);
        
        for (unsigned int i = 0; i < masterCpy.fd_count; i++)
        {
            if (masterCpy.fd_array[i] == soc) // User online
            {                 
                SOCKET new_soc = accept(soc, (sockaddr*)&new_addr, &new_addrsz);
                ws_handshake(new_soc, ip, port);
                FD_SET(new_soc, &master);
                std::cout << "[INFO] Online: " << (int)new_soc << "\n";
                sprintf_s(infobuff, 128, "[INFO]> [#%d] online.\n", (int)new_soc); // Notify others
                //for (int j = master.fd_count; j >= 0; --j)
                    //if(master.fd_array[j] != soc && master.fd_array[j] != new_soc)
                        
                continue;
            }             
             
            ZeroMemory(&buff, 4096);
            buff_sz = recv(masterCpy.fd_array[i], buff, 4096, 0);
            std::string decoded(buff);
            ws_unmask(decoded);
                          
            if (buff_sz <= 6) // User offline
            {
                closesocket(masterCpy.fd_array[i]);
                FD_CLR(masterCpy.fd_array[i], &master);
                std::cout << "[INFO] Offline: " << masterCpy.fd_array[i] << "\n";
                sprintf_s(infobuff, 128, "[INFO]> [#%d] offline.\n", (int)masterCpy.fd_array[i]); // Notify others
                //for (int j = master.fd_count; j >= 0; --j)
                    //if (master.fd_array[j] != soc && master.fd_array[j] != masterCpy.fd_array[i])
                         
                continue;
            }
             
            std::cout << "[#" << masterCpy.fd_array[i] << "]> " << buff << "\n";

            for (unsigned int j = 0; j < master.fd_count; j++) // Broadcast user message to others
            {
                if (master.fd_array[j] != soc && master.fd_array[j] != masterCpy.fd_array[i])
                {                     
                    out.str("");
                    out.clear();
                    out << "[" << (int)masterCpy.fd_array[i] << "]> " << decoded << "\n";
                    //send(master.fd_array[j], out.str().c_str(), out.str().length(), 0);
                }
            }
        }
    }

    closesocket(soc);
    WSACleanup();
}
