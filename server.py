import socket
import os

MAX_BUFFER = 1024

def error(msg):
    print(msg)
    exit(1)

def resError(err, code, newsockfd):
    err = err.encode()
    if code == 404:
        header = "HTTP/1.1 404 Not Found\r\n"
    elif code == 400:
        header = "HTTP/1.1 400 Bad Request\r\n"
    elif code == 200:
        header = "HTTP/1.1 200 OK\r\n"
    response = (
        header +
        f"Content-Length: {len(err)}\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        f"{err.decode()}"
    ).encode()

    n = newsockfd.send(response)
    if not n:
        error("Failed to write")

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <port>")
        exit(1)

    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if not sockfd:
        error("Unable to open socket")

    sockfd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    portno = int(sys.argv[1])

    sockfd.bind(('', portno))

    sockfd.listen(1)
    print(f"Serving on port: {portno}")

    while True:
        newsockfd, addr = sockfd.accept()
        if not newsockfd:
            error("Unable to accept connection")

        buffer = newsockfd.recv(MAX_BUFFER).decode()

        print("Request:\n%s\n" % buffer)

        if buffer.startswith("GET / "):
            try:
                fptr = open("index.html", "r")
                html = fptr.read()
                fptr.close()

                response = ("HTTP/1.1 200 OK\r\n"
                            f"Content-Length: {len(html)}\r\n"
                            f"Content-Type: text/html\r\n\r\n{html}").encode()
            except Exception as e:
                resError("{\"error\":\"" + str(e) + "\"}", 404, newsockfd)

            newsockfd.send(response)

        elif buffer.startswith("GET /getTasks "):
            try:
                fptr = open("data.xml", "r")
                xml = fptr.read()
                fptr.close()

                response = ("HTTP/1.1 200 OK\r\n"
                            f"Content-Length: {len(xml)}\r\n"
                            f"Content-Type: text/xml\r\n\r\n{xml}").encode()
            except Exception as e:
                resError("{\"error\":\"" + str(e) + "\"}", 404, newsockfd)

            newsockfd.send(response)

        elif buffer.startswith("POST /postTask "):
            try:
                val_start = buffer.find("\"value\":\"")
                if val_start != -1:
                    val_start += 9
                    val_end = buffer.find("\"", val_start)
                    val = buffer[val_start:val_end]

                    if val == "":
                        resError("{\"error\":\"The field is empty\", \"field\":\"taskInput\"}", 400, newsockfd)
                    elif len(val) > 30:
                        resError("{\"error\":\"The field must have under near 30 chars\", \"field\":\"taskInput\"}", 400, newsockfd)
                    else:
                        try:
                            id = None
                            fptr = open("data.xml", "rb+")
                            for line in fptr:
                                id_start = line.find(b"<id>")
                                if id_start != -1:
                                    id_start += 4
                                    id_end = line.find(b"<", id_start)
                                    id = line[id_start:id_end].decode()

                            id = int(id)+1 if id != None else 1
                            fptr.seek(-7, os.SEEK_END)
                            fptr.write(f"    <task><id>{id}</id><value>{val}</value></task>\r\n</data>".encode())
                            fptr.close()

                            resError("{\"success\":1,\"id\":\"" + str(id) + "\"}", 200, newsockfd)
                        except Exception as e:
                            resError("{\"error\":\"" + str(e) + "\"}", 404, newsockfd)
                else:
                    resError("{\"error\":\"Bad post string\"}", 400, newsockfd)
            except Exception as e:
                resError("{\"error\":\"" + str(e) + "\"}", 400, newsockfd)

        elif buffer.startswith("DELETE /deleteTask"):
            try:
                id_start = buffer.find("\"id\":\"")
                if id_start != -1 and buffer[id_start + 6] != "\"":
                    id_start += 6
                    id_end = buffer.find("\"", id_start)
                    id = buffer[id_start:id_end]
                    try:
                        fptr = open("data.xml", "r")
                        lines = fptr.readlines()
                        for i in range(len(lines)-1):
                            newId_start = lines[i].find("<id>")
                            if newId_start != -1:
                                newId_start += 4
                                newId_end = lines[i].find("<", newId_start)
                                newId = lines[i][newId_start:newId_end]
                                if newId == id:
                                    lines.remove(lines[i])
                                    id = "0"
                        fptr.close()
                        if id == "0":
                            os.remove("data.xml")

                            fptr = open("data.xml", "w")
                            fptr.close()

                            fptr = open("data.xml", "a")
                            for i in range(len(lines)):
                                fptr.write(lines[i])

                            fptr.close()

                            resError("{\"success\":1}", 200, newsockfd)
                        else:
                            resError("{\"error\":\"Task not found\"}", 404, newsockfd)
                    except Exception as e:
                        resError("{\"error\":\"" + str(e) + "\"}", 404, newsockfd)
                else:
                    resError("{\"error\":\"Bad post string\"}", 400, newsockfd)
            except Exception as e:
                resError("{\"error\":\"" + str(e) + "\"}", 400, newsockfd)

        elif buffer.startswith("PUT /putTask"):
            try:
                id_start = buffer.find("\"id\":\"")
                if id_start != -1 and buffer[id_start + 6] != "\"":
                    id_start += 6
                    id_end = buffer.find("\"", id_start)
                    id = buffer[id_start:id_end]
                    try:
                        val_start = buffer.find("\"newValue\":\"")
                        if val_start != -1:
                            val_start += 12
                            val_end = buffer.find("\"", val_start)
                            val = buffer[val_start:val_end]

                            if val == "":
                                resError("{\"error\":\"The field is empty\", \"field\":\"editInput"+id+"\"}", 400, newsockfd)
                            elif len(val) > 30:
                                resError("{\"error\":\"The field must have under near 30 chars\", \"field\":\"editInput"+id+"\"}", 400, newsockfd)
                            else:
                                try:
                                    fptr = open("data.xml", "r")
                                    lines = fptr.readlines()
                                    for i in range(len(lines)-1):
                                        newId_start = lines[i].find("<id>")
                                        if newId_start != -1:
                                            newId_start += 4
                                            newId_end = lines[i].find("<", newId_start)
                                            newId = lines[i][newId_start:newId_end]
                                            if newId == id:
                                                lines[i] = f"    <task><id>{id}</id><value>{val}</value></task>\r\n"
                                                id = "0"
                                    fptr.close()
                                    if id == "0":
                                        os.remove("data.xml")

                                        fptr = open("data.xml", "w")
                                        fptr.close()

                                        fptr = open("data.xml", "a")
                                        for i in range(len(lines)):
                                            fptr.write(lines[i])

                                        fptr.close()

                                        resError("{\"success\":1}", 200, newsockfd)
                                    else:
                                        resError("{\"error\":\"Task not found\"}", 404, newsockfd)
                                except Exception as e:
                                    resError("{\"error\":\"" + str(e) + "\"}", 404, newsockfd)
                        else:
                            resError("{\"error\":\"Bad post string\"}", 400, newsockfd)
                    except Exception as e:
                        resError("{\"error\":\"" + str(e) + "\"}", 400, newsockfd)
            except Exception as e:
                resError("{\"error\":\"" + str(e) + "\"}", 400, newsockfd)
        newsockfd.close()

    sockfd.close()