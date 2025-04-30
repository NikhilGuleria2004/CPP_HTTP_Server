#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <thread>
#include <filesystem>
#include <mutex>

using namespace std;
namespace fs = std::filesystem;

std::mutex fileMutex;

namespace HTTP {
    struct Request {
        string method;
        string resource;
        string version;
        map<string, string> headers;
        string body;
    };

    Request parseRequest(const string& rawRequest) {
        Request req;
        istringstream iss(rawRequest);
        string line;

        getline(iss, line, '\n');
        if (!line.empty() && line.back() == '\r') line.erase(line.size() - 1);
        istringstream requestLine(line);
        requestLine >> req.method >> req.resource >> req.version;

        while (true) {
            getline(iss, line, '\n');
            line.erase(0, line.find_first_not_of(" \t\r"));
            line.erase(line.find_last_not_of(" \t\r") + 1);
            if (line.empty()) break;
            size_t colonPos = line.find(':');
            if (colonPos != string::npos) {
                string key = line.substr(0, colonPos);
                string value = line.substr(colonPos + 1);
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                req.headers[key] = value;
            }
        }

        if (req.headers.find("Content-Length") != req.headers.end()) {
            int contentLength = stoi(req.headers["Content-Length"]);
            string body;
            while (contentLength > 0 && iss.peek() != EOF) {
                char c = iss.get();
                body += c;
                contentLength--;
            }
            req.body = body;
        }

        return req;
    }
}

string readFullHttpRequest(SOCKET clientSocket) {
    const size_t HEADER_MAX = 8192;
    string data;
    data.resize(HEADER_MAX);
    int bytesRead = recv(clientSocket, &data[0], HEADER_MAX, 0);
    if (bytesRead <= 0) return "";
    data.resize(bytesRead);

    size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == string::npos) return "";
    headerEnd += 4;

    string headers = data.substr(0, headerEnd);
    size_t clPos = headers.find("Content-Length: ");
    if (clPos != string::npos) {
        size_t valStart = clPos + 16;
        size_t valEnd = headers.find("\r\n", valStart);
        if (valEnd != string::npos) {
            string clStr = headers.substr(valStart, valEnd - valStart);
            int contentLength = stoi(clStr);
            size_t totalSize = headerEnd + contentLength;
            while (data.size() < totalSize) {
                size_t toRead = totalSize - data.size();
                string more(toRead, '\0');
                int readBytes = recv(clientSocket, &more[0], toRead, 0);
                if (readBytes <= 0) break;
                data += more.substr(0, readBytes);
            }
        }
    }
    return data;
}

string getMimeType(const string& filePath) {
    static map<string, string> mimeTypes = {
        {".html", "text/html"}, {".htm", "text/html"}, {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"}, {".png", "image/png"}, {".gif", "image/gif"},
        {".css", "text/css"}, {".js", "application/javascript"}, {".pdf", "application/pdf"},
        {".zip", "application/zip"}, {".mp4", "video/mp4"}, {".webm", "video/webm"},
        {".ogg", "audio/ogg"}, {".mp3", "audio/mpeg"}
    };
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != string::npos && dotPos < filePath.size() - 1) {
        string extension = filePath.substr(dotPos);
        auto it = mimeTypes.find(extension);
        if (it != mimeTypes.end()) return it->second;
    }
    return "application/octet-stream";
}

string generateResponse(const HTTP::Request& req) {
    string resource = req.resource;
    if (resource == "/") resource = "/index.html";
    string root = "./www";
    fs::path filePath = root + resource;

    if (req.method == "GET" || req.method == "HEAD") {
        if (fs::is_directory(filePath)) filePath /= "index.html";
        if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
            ifstream file(filePath, ios::binary | ios::ate);
            if (file.is_open()) {
                size_t size = file.tellg();
                file.seekg(0, ios::beg);
                string content(size, '\0');
                file.read(&content[0], size);
                string mimeType = getMimeType(filePath.string());
                string response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: " + mimeType + "\r\n";
                response += "Content-Length: " + to_string(size) + "\r\n";
                response += "Connection: close\r\n";
                if (req.method == "GET") {
                    response += "\r\n";
                    response += content;
                } else {
                    response += "\r\n";
                }
                return response;
            } else {
                string body = "<h1>403 Forbidden</h1>";
                return "HTTP/1.1 403 Forbidden\r\nContent-Type: text/html\r\nContent-Length: " + to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            }
        } else {
            string body = "<h1>404 Not Found</h1>";
            return "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: " + to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        }
    } else if (req.method == "POST") {
        std::lock_guard<std::mutex> lock(fileMutex);
        ofstream file(filePath, ios::binary);
        if (file.is_open()) {
            file << req.body;
            file.close();
            string response = "HTTP/1.1 201 Created\r\nContent-Type: text/html\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            return response;
        } else {
            string body = "<h1>500 Internal Server Error</h1>";
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: " + to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        }
    } else if (req.method == "PUT") {
        std::lock_guard<std::mutex> lock(fileMutex);
        ofstream file(filePath, ios::binary);
        if (file.is_open()) {
            file << req.body;
            file.close();
            string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            return response;
        } else {
            string body = "<h1>500 Internal Server Error</h1>";
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: " + to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        }
    } else if (req.method == "DELETE") {
        std::lock_guard<std::mutex> lock(fileMutex);
        if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
            fs::remove(filePath);
            string response = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
            return response;
        } else {
            string body = "<h1>404 Not Found</h1>";
            return "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: " + to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        }
    } else {
        string body = "<h1>405 Method Not Allowed</h1>";
        return "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\nContent-Length: " + to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    }
}

void handleClient(SOCKET clientSocket) {
    string fullRequest = readFullHttpRequest(clientSocket);
    if (!fullRequest.empty()) {
        HTTP::Request req = HTTP::parseRequest(fullRequest);
        string response = generateResponse(req);
        send(clientSocket, response.c_str(), response.size(), 0);
    } else {
        string badRequest = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 22\r\nConnection: close\r\n\r\n<h1>400 Bad Request</h1>";
        send(clientSocket, badRequest.c_str(), badRequest.size(), 0);
    }
    closesocket(clientSocket);
}

int main() { 
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cerr << "WSAStartup failed: " << iResult << endl;
        return 1;
    }

    SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        cerr << "Error at socket(): " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons(8080);

    if (bind(ListenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        cerr << "bind() failed: " << WSAGetLastError() << endl;
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(ListenSocket, 5) == SOCKET_ERROR) {
        cerr << "listen() failed: " << WSAGetLastError() << endl;
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server is listening on port 8080..." << endl;

    while (true) {
        SOCKET clientSocket = accept(ListenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "accept failed with error: " << WSAGetLastError() << endl;
            continue;
        }

        thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    closesocket(ListenSocket);
    WSACleanup();
    return 0;
}