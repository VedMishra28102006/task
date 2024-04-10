#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

const int MAX_BUFFER = 1024;

void error(std::string msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

void resError(std::string err, int code, int newsockfd) {
    std::string header;
    if (code == 404) {
        header = "HTTP/1.1 404 Not Found\r\n";
    } else if (code == 400) {
        header = "HTTP/1.1 400 Bad Request\r\n";
    } else if (code == 200) {
        header = "HTTP/1.1 200 OK\r\n";
    }
    std::string response =
        header + "Content-Length: " + std::to_string(err.length()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" + err;
    int n = send(newsockfd, response.c_str(), response.length(), 0);
    if (n <= 0) {
        error("Failed to write");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("Unable to open socket");
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    int portno = atoi(argv[1]);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error("Unable to bind socket");
    }

    listen(sockfd, 1);
    std::cout << "Serving on port: " << portno << std::endl;

    while (true) {
        int newsockfd = accept(sockfd, NULL, NULL);
        if (newsockfd < 0) {
            error("Unable to accept connection");
        }

        char buffer[MAX_BUFFER + 1];  // +1 for null terminator
        int n = recv(newsockfd, buffer, MAX_BUFFER, 0);
        if (n < 0) {
            error("Failed to read");
        }
        buffer[n] = '\0';
        std::cout << "Request:\n" << buffer << std::endl;

        if (strncmp(buffer, "GET / ", 6) == 0) {
            try {
                std::ifstream fptr("index.html");
                if (!fptr) {
                    resError("{\"error\":\"File not found\"}", 404, newsockfd);
                } else {
                    std::string html((std::istreambuf_iterator<char>(fptr)),
                                     std::istreambuf_iterator<char>());
                    fptr.close();
                    std::string response =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: " +
                        std::to_string(html.length()) +
                        "\r\n"
                        "Content-Type: text/html\r\n\r\n" +
                        html;
                    send(newsockfd, response.c_str(), response.length(), 0);
                }
            } catch (std::exception& e) {
                resError("{\"error\":\"" + std::string(e.what()) + "\"}", 500,
                         newsockfd);
            }
        } else if (strncmp(buffer, "GET /getTasks ", 13) == 0) {
            try {
                std::ifstream fptr("data.xml");
                if (!fptr) {
                    resError("{\"error\":\"File not found\"}", 404, newsockfd);
                } else {
                    std::string xml((std::istreambuf_iterator<char>(fptr)),
                                    std::istreambuf_iterator<char>());
                    fptr.close();
                    std::string response =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: " +
                        std::to_string(xml.length()) +
                        "\r\n"
                        "Content-Type: text/xml\r\n\r\n" +
                        xml;
                    send(newsockfd, response.c_str(), response.length(), 0);
                }
            } catch (std::exception& e) {
                resError("{\"error\":\"" + std::string(e.what()) + "\"}", 500,
                         newsockfd);
            }
        } else if (strncmp(buffer, "POST /postTask ", 15) == 0) {
            try {
                std::string bufferStr(buffer);

                size_t val_start = bufferStr.find("\"value\":\"");
                if (val_start != std::string::npos) {
                    val_start += 9;
                    size_t val_end = bufferStr.find("\"", val_start);
                    std::string val =
                        bufferStr.substr(val_start, val_end - val_start);

                    if (val.empty()) {
                        resError(
                            "{\"error\":\"The field is empty\", "
                            "\"field\":\"taskInput\"}",
                            400, newsockfd);
                    } else if (val.length() > 30) {
                        resError(
                            "{\"error\":\"The field must have under 30 "
                            "chars\", \"field\":\"taskInput\"}",
                            400, newsockfd);
                    } else {
                        try {
                            int id = 1;
                            std::ifstream fptr("data.xml");
                            if (!fptr.is_open()) {
                                resError(
                                    "{\"error\":\"Failed to open data file\"}",
                                    500, newsockfd);
                            } else {
                                std::string line;
                                while (std::getline(fptr, line)) {
                                    size_t id_start = line.find("<id>");
                                    if (id_start != std::string::npos) {
                                        id_start += 4;
                                        size_t id_end =
                                            line.find("<", id_start);
                                        id = std::stoi(line.substr(
                                                 id_start, id_end - id_start)) +
                                             1;
                                    }
                                }
                                fptr.close();

                                std::fstream fptr_out("data.xml", std::ios::in | std::ios::out);
                                if (!fptr_out.is_open()) {
                                    resError(
                                        "{\"error\":\"Failed to open data file "
                                        "for writing\"}",
                                        500, newsockfd);
                                } else {
                                    fptr_out.seekp(-7, std::ios_base::end);
                                    fptr_out << "    <task><id>" << id
                                             << "</id><value>" << val
                                             << "</value></task>\r\n</data>";
                                }
                                fptr_out.close();

                                resError("{\"success\":1,\"id\":\"" +
                                             std::to_string(id) + "\"}",
                                         200, newsockfd);
                            }
                        } catch (std::exception& e) {
                            resError(
                                "{\"error\":\"" + std::string(e.what()) + "\"}",
                                500, newsockfd);
                        }
                    }
                } else {
                    resError("{\"error\":\"Bad post string\"}", 400, newsockfd);
                }
            } catch (std::exception& e) {
                resError("{\"error\":\"" + std::string(e.what()) + "\"}", 400,
                         newsockfd);
            }

        } else if (strncmp(buffer, "DELETE /deleteTask", 18) == 0) {
            try {
                std::string bufferStr(buffer);
                int id_start = bufferStr.find("\"id\":\"");
                if (id_start != std::string::npos &&
                    bufferStr[id_start + 6] != '"') {
                    id_start += 6;
                    int id_end = bufferStr.find("\"", id_start);
                    std::string id =
                        bufferStr.substr(id_start, id_end - id_start);
                    try {
                        std::ifstream fptr("data.xml");
                        if (!fptr.is_open()) {
                            throw std::runtime_error("Failed to open file.");
                        }
                        std::vector<std::string> lines;
                        std::string line;
                        while (std::getline(fptr, line)) {
                            int newId_start = line.find("<id>");
                            if (newId_start != std::string::npos) {
                                newId_start += 4;
                                int newId_end = line.find("<", newId_start);
                                std::string newId = line.substr(
                                    newId_start, newId_end - newId_start);
                                if (newId != id) {
                                    lines.push_back(line);
                                }
                            } else {
                                lines.push_back(line);
                            }
                        }
                        fptr.close();

                        if (!lines.empty()) {
                            std::remove("data.xml");
                            std::ofstream fptr("data.xml");
                            if (!fptr.is_open()) {
                                throw std::runtime_error(
                                    "Failed to open file for writing.");
                            }
                            for (auto it = lines.begin(); it != lines.end();
                                 ++it) {
                                fptr << *it;
                                if (it != lines.end() - 1) fptr << std::endl;
                            }
                            fptr.close();
                            resError("{\"success\":1}", 200, newsockfd);
                        } else {
                            resError("{\"error\":\"Task not found\"}", 404,
                                     newsockfd);
                        }
                    } catch (std::exception& e) {
                        resError(
                            "{\"error\":\"" + std::string(e.what()) + "\"}",
                            404, newsockfd);
                    }
                } else {
                    resError("{\"error\":\"Bad post string\"}", 400, newsockfd);
                }
            } catch (std::exception& e) {
                resError("{\"error\":\"" + std::string(e.what()) + "\"}", 400,
                         newsockfd);
            }
        } else if (strncmp(buffer, "PUT /putTask", 12) == 0) {
            try {
                std::string bufferStr(buffer);
                int id_start = bufferStr.find("\"id\":\"");
                if (id_start != std::string::npos &&
                    bufferStr[id_start + 6] != '"') {
                    id_start += 6;
                    int id_end = bufferStr.find("\"", id_start);
                    std::string id =
                        bufferStr.substr(id_start, id_end - id_start);
                    size_t val_start = bufferStr.find("\"newValue\":\"");
                    if (val_start != std::string::npos) {
                        val_start += 12;
                        size_t val_end = bufferStr.find("\"", val_start);
                        std::string val =
                            bufferStr.substr(val_start, val_end - val_start);

                        if (val.empty()) {
                            resError(
                                "{\"error\":\"The field is empty\", "
                                "\"field\":\"editInput"+id+"\"}",
                                400, newsockfd);
                        } else if (val.length() > 30) {
                            resError(
                                "{\"error\":\"The field must have under 30 "
                                "chars\", \"field\":\"taskInput"+id+"\"}",
                                400, newsockfd);
                        } else {
                    try {
                        std::ifstream fptr("data.xml");
                        if (!fptr.is_open()) {
                            throw std::runtime_error("Failed to open file.");
                        }

                        std::vector<std::string> lines;
                        std::string line;
                        while (std::getline(fptr, line)) {
                            int newId_start = line.find("<id>");
                            if (newId_start != std::string::npos) {
                                newId_start += 4;
                                int newId_end = line.find("<", newId_start);
                                std::string newId = line.substr(
                                    newId_start, newId_end - newId_start);
                                if (newId != id) {
                                    lines.push_back(line);
                                } else {
                                    lines.push_back("    <task><id>" + id
                                             + "</id><value>" + val
                                             + "</value></task>");
                                }
                            } else {
                                lines.push_back(line);
                            }
                        }

                        fptr.close();

                        if (!lines.empty()) {
                            std::remove("data.xml");
                            std::ofstream fptr("data.xml");
                            if (!fptr.is_open()) {
                                throw std::runtime_error(
                                    "Failed to open file for writing.");
                            }
                            for (auto it = lines.begin(); it != lines.end();
                                 ++it) {
                                fptr << *it;
                                if (it != lines.end() - 1) fptr << std::endl;
                            }
                            fptr.close();
                            resError("{\"success\":1}", 200, newsockfd);
                        } else {
                            resError("{\"error\":\"Task not found\"}", 404,
                                     newsockfd);
                        }
                    } catch (std::exception& e) {
                        resError(
                            "{\"error\":\"" + std::string(e.what()) + "\"}",
                            404, newsockfd);
                    }
                    }
                } else {
                    resError("{\"error\":\"Bad post string\"}", 400, newsockfd);
                }
                } else {
                    resError("{\"error\":\"Bad post string\"}", 400, newsockfd);
                }
            } catch (std::exception& e) {
                resError("{\"error\":\"" + std::string(e.what()) + "\"}", 400,
                         newsockfd);
            }
        } else {
            resError("{\"error\":\"Invalid request\"}", 400, newsockfd);
        }
        close(newsockfd);
    }

    close(sockfd);
    return 0;
}