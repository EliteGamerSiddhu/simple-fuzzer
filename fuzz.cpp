#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>

#ifdef _WIN32

	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif

	#include <winsock2.h>
	#include <ws2tcpip.h>

	#pragma comment(lib, "Ws2_32.lib")

#else

	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <unistd.h>
	#include <cstring>
	#include <cerrno>

	#ifndef SD_SEND
		#define SD_SEND SHUT_WR
	#endif

	#ifndef SOCKET
		#define SOCKET unsigned long long int
	#endif

#endif

#define PORT "80"
#define LOCALHOST "127.0.0.1"
#define BUFLEN 1048

#ifndef INVALID_SOCKET
	#define INVALID_SOCKET (~0)
#endif

#ifndef SOCKET_ERROR
	#define SOCKET_ERROR (-1)
#endif

using namespace std;

void cleanup(SOCKET sock){
	#ifdef _WIN32

	if(sock != INVALID_SOCKET){
		closesocket(sock);
	}
	WSACleanup();

	#else

	if(sock != INVALID_SOCKET){
		close(sock);
	}

	#endif
}

void ParseUrl(const char *mUrl, string &serverName, string &filepath)
{
    string::size_type n;
    string url = mUrl;

    if (url.substr(0,7) == "http://")
        url.erase(0,7);

    if (url.substr(0,8) == "https://")
        url.erase(0,8);

    n = url.find('/');
    if (n != string::npos)
    {
        serverName = url.substr(0,n);
        filepath = url.substr(n);
    }

    else
    {
        serverName = url;
        filepath = "/";
    }
}

string ParseGet(string &servName, string &pPath){
    string getBuf = "GET " + pPath + " HTTP/1.1\r\nHost: " + servName + "\r\n\r\n";

    return getBuf;
}

string HostNameByUrl(const char* url){
    string temp = url;

    int n = temp.find('/');

    if(n != string::npos){
        return temp.substr(0,n);
    }
    else{
        return temp;
    }
}

void pinger_job(string base_url, ifstream &f_in, ofstream &f_out, int num, int total){
    addrinfo *result = NULL, serv_addr;
    SOCKET pinger = INVALID_SOCKET;

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.ai_family = AF_INET;
	serv_addr.ai_socktype = SOCK_STREAM;
	serv_addr.ai_protocol = IPPROTO_TCP;

    int pass;
    char temp[500];
    int line_count = 0;
    string word;

    while(f_in.getline(temp, 500)){
        if((line_count + num) % total == 0){
            result = NULL;
            pinger = INVALID_SOCKET;

            string curr_url = base_url + "/" + string(temp);

            if(curr_url[curr_url.length() - 1] == '\n') curr_url.pop_back();

            string servName = "", pPath = "";
            ParseUrl(curr_url.c_str(), ref(servName), ref(pPath));

            string getRequest = ParseGet(ref(servName), ref(pPath));
            char responseBuf[1024];
            string responseRequest = "";

            pass = getaddrinfo(servName.c_str(), PORT, &serv_addr, &result);
            if(pass != 0){
                continue;
            }

            pinger = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if(pinger == INVALID_SOCKET){
                continue;
            }

            pass = connect(pinger, result->ai_addr, (int)result->ai_addrlen);
            if(pass == SOCKET_ERROR){
                continue;
            }

            if(pinger == INVALID_SOCKET) continue;

            pass = send(pinger, getRequest.c_str(), (int) strlen(getRequest.c_str()) + 1, 0);
            if (pass == SOCKET_ERROR) {
                continue;
            }

            do {
                pass = recv(pinger, responseBuf, 1024, 0);
                if(pass >= 0){
                    responseRequest += responseBuf;
                }
            } while (pass > 0);

            string responseCode = responseRequest.substr(responseRequest.find(' ') + 1, 3);

            cout << responseCode << endl;

            if(responseCode == "404" || responseCode == "400"){
                continue;
            }

            curr_url += "\n";

            f_out << curr_url;

            freeaddrinfo(result);

            #ifdef _WIN32
                closesocket(pinger);
            #else
                close(pinger);
            #endif
        }

        ++line_count;
    }
}

int main(int argc, char* argv[]){

    if(argc == 1){
        cerr << "Arguments : <base_url> <word_list> <output_file>" << endl;
        return 1;
    }

    if(argc < 4 ){
        cerr << "Expected at least 3 arguments" << endl;
        return 1;
    }

    if(argc > 5){
        cerr << "Expected at max 4 arguments" << endl;
    }

    string base_url = argv[1];
    string f_in_path = argv[2];
    string f_out_path = argv[3];
    int t_num = 1;

    // if(argc > 4) t_num = stoi(argv[4]);         Multithreading still being worked on


    int iResult;

	//Initializing Winsocket library

	#ifdef _WIN32

        WSADATA wsaData;

        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            cout << "WsaStartup Failed : " << iResult;
            return 1;
        }

	#endif

    vector<string> words;
    char temp[1024];
    ifstream in_file(f_in_path);
    ofstream out_file(f_out_path); 

    vector<thread> pingers(t_num);


    for(int i = 0; i < t_num; i++){
        pingers[i]= thread(pinger_job, base_url, ref(in_file), ref(out_file), i, t_num);
    }


    for(thread &x: pingers){
        x.join();
    }

    in_file.close();
    out_file.close();
    cleanup(INVALID_SOCKET);

    return 0;
}