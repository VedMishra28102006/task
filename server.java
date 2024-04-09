import java.io.*;
import java.net.*;

public class Server {
    private static final int MAX_SIZE = 256;
    private static final int MAX_BUFFER = 1024;

    public static void error(String msg) {
        System.err.println(msg);
        System.exit(1);
    }

    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("Usage: java Server <port>");
            System.exit(1);
        }

        try {
            int portno = Integer.parseInt(args[0]);
            ServerSocket serverSocket = new ServerSocket(portno);
            System.out.println("Listening on port: " + portno);

            while (true) {
                Socket clientSocket = serverSocket.accept();
                InputStream in = clientSocket.getInputStream();
                OutputStream out = clientSocket.getOutputStream();

                byte[] buffer = new byte[MAX_BUFFER];
                int n = in.read(buffer);

                if (n < 0) {
                    error("Failed to read");
                }

                String request = new String(buffer, 0, n);
                System.out.println("Request:\n" + request);

                if (request.startsWith("GET / ")) {
                    File fptr = new File("index.html");
                    if (!fptr.exists()) {
                        System.err.println("Error opening view file");
                        System.exit(1);
                    }

                    FileInputStream fis = new FileInputStream(fptr);
                    byte[] html = new byte[(int) fptr.length()];
                    fis.read(html);
                    fis.close();

                    String response = "HTTP/1.1 200 OK\r\n" +
                            "Content-Length: " + html.length + "\r\n" +
                            "Content-Type: text/html\r\n\r\n";
                    out.write(response.getBytes());
                    out.write(html);
                } else if (request.startsWith("GET /getTasks ")) {
                    File fptr = new File("data.xml");
                    if (!fptr.exists()) {
                        System.err.println("Error opening data file");
                        System.exit(1);
                    }

                    FileInputStream fis = new FileInputStream(fptr);
                    byte[] xml = new byte[(int) fptr.length()];
                    fis.read(xml);
                    fis.close();

                    String response = "HTTP/1.1 200 OK\r\n" +
                            "Content-Length: " + xml.length + "\r\n" +
                            "Content-Type: text/xml\r\n\r\n";
                    out.write(response.getBytes());
                    out.write(xml);
                } else if (new String(buffer).startsWith("POST /postTask ")) {
                    String start = new String(buffer).substring(new String(buffer).indexOf("\"value\":\"") + 9);
                    String err = "";
                    String response = "";
    
                    if (start == null) {
                        err = "{\"error\":\"Bad post string\"}";
                        response = "HTTP/1.1 400 Bad Request\r\n" +
                                "Content-Length: " + err.length() + "\r\n" +
                                "Content-Type: application/json\r\n\r\n" +
                                err;
                    } else {
                        start = start.substring(0, start.indexOf("\""));
                        String val = start;

                        if (val.isEmpty()) {
                            err = "{\"error\":\"The field is empty\", \"field\":\"taskInput\"}";
                            response = "HTTP/1.1 400 Bad Request\r\n" +
                                    "Content-Length: " + err.length() + "\r\n" +
                                    "Content-Type: application/json\r\n\r\n" +
                                    err;
                        } else if (val.length() > 30) {
                            err = "{\"error\":\"The field must have under near 30 chars\", \"field\":\"taskInput\"}";
                            response = "HTTP/1.1 400 Bad Request\r\n" +
                                    "Content-Length: " + err.length() + "\r\n" +
                                    "Content-Type: application/json\r\n\r\n" +
                                    err;
                        } else {
                            try {
                                RandomAccessFile xml = new RandomAccessFile("data.xml", "rw");
                                xml.seek(0);
                                String lastLine = "";
                                String currentLine;
                                String newId = "0";

                                while ((currentLine = xml.readLine()) != null) {
                                    if (currentLine.contains("<id>")) {
                                        newId = currentLine.substring(currentLine.indexOf("<id>") + 4, currentLine.indexOf("</id>"));
                                    }
                                    lastLine = currentLine;
                                }
                                xml.seek(xml.length()-7);
                                xml.writeBytes("    <task><id>" + (Integer.parseInt(newId) + 1) + "</id><value>" + val + "</value></task>\r\n</data>");
                                xml.close();

                                err = "{\"success\":\"1\", \"id\":\"" + (Integer.parseInt(newId) + 1) + "\"}";
                                response = "HTTP/1.1 200 OK\r\n" +
                                        "Content-Length: " + err.length() + "\r\n" +
                                        "Content-Type: application/json\r\n\r\n" +
                                        err;
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                        }
                    }
                    out.write(response.getBytes());

                } else if (new String(buffer).startsWith("DELETE /deleteTask ")) {
                    String start = new String(buffer).substring(new String(buffer).indexOf("\"id\":\"") + 6);
                    String err = "";
                    String response = "";
                
                    if (start == null || start.equals("") || start.startsWith("\"")) {
                        err = "{\"error\":\"Bad post string\"}";
                        response = "HTTP/1.1 400 Bad Request\r\n" +
                                "Content-Length: " + err.length() + "\r\n" +
                                "Content-Type: application/json\r\n\r\n" +
                                err;
                    } else {
                        start = start.substring(0, start.indexOf("\""));
                        String id = start;
                
                        try {
                            RandomAccessFile xml = new RandomAccessFile("data.xml", "rw");
                            xml.seek(0);
                            int lineNum = 0;
                            int deleted = 1;
                            String currentLine;
                            String newId = "0";
                            String lines[] = new String[MAX_SIZE];
                            while ((currentLine = xml.readLine()) != null) {
                                if (currentLine.contains("<id>")) {
                                    newId = currentLine.substring(currentLine.indexOf("<id>") + 4, currentLine.indexOf("</id>"));
                                    if (id.equals(newId)) {
                                        deleted = 0;
                                    } else {
                                        lines[lineNum] = currentLine;
                                        lineNum++;
                                    }
                                } else {
                                    lines[lineNum] = currentLine;
                                    lineNum++;
                                }
                            }
                            xml.setLength(0);
                            xml.seek(0);
                            for (int i = 0; i < lineNum; i++) {
                                xml.writeBytes((i != (lineNum-1)) ? lines[i] + "\r\n" : lines[i]);
                            }
                
                            xml.close();
                
                            if (deleted == 0) {
                                err = "{\"success\":\"1\", \"id\":\"" + (Integer.parseInt(newId) + 1) + "\"}";
                                response = "HTTP/1.1 200 OK\r\n" +
                                        "Content-Length: " + err.length() + "\r\n" +
                                        "Content-Type: application/json\r\n\r\n" +
                                        err;
                            } else {
                                err = "{\"error\":\"Task not found\"}";
                                response = "HTTP/1.1 404 Not Found\r\n" +
                                        "Content-Length: " + err.length() + "\r\n" +
                                        "Content-Type: application/json\r\n\r\n" +
                                        err;
                            }
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }
                    out.write(response.getBytes());
                } else if (new String(buffer).startsWith("PUT /putTask ")) {
                    String start = new String(buffer).substring(new String(buffer).indexOf("\"id\":\"") + 6);
                    String err = "";
                    String response = "";
                
                    if (start == null || start.equals("") || start.startsWith("\"")) {
                        err = "{\"error\":\"Bad post string\"}";
                        response = "HTTP/1.1 400 Bad Request\r\n" +
                                "Content-Length: " + err.length() + "\r\n" +
                                "Content-Type: application/json\r\n\r\n" +
                                err;
                    } else {
                        start = start.substring(0, start.indexOf("\""));
                        String id = start;

                        start = new String(buffer).substring(new String(buffer).indexOf("\"newValue\":\"") + 12);
    
                        if (start == null) {
                            err = "{\"error\":\"Bad post string\"}";
                            response = "HTTP/1.1 400 Bad Request\r\n" +
                                    "Content-Length: " + err.length() + "\r\n" +
                                    "Content-Type: application/json\r\n\r\n" +
                                    err;
                        } else {
                            start = start.substring(0, start.indexOf("\""));
                            String val = start;

                            if (val.isEmpty()) {
                                err = "{\"error\":\"The field is empty\", \"field\":\"editInput"+id+"\"}";
                                response = "HTTP/1.1 400 Bad Request\r\n" +
                                        "Content-Length: " + err.length() + "\r\n" +
                                        "Content-Type: application/json\r\n\r\n" +
                                        err;
                            } else if (val.length() > 30) {
                                err = "{\"error\":\"The field must have under near 30 chars\", \"field\":\"editInput"+id+"\"}";
                                response = "HTTP/1.1 400 Bad Request\r\n" +
                                        "Content-Length: " + err.length() + "\r\n" +
                                        "Content-Type: application/json\r\n\r\n" +
                                        err;
                            } else {
                                try {
                                    RandomAccessFile xml = new RandomAccessFile("data.xml", "rw");
                                    xml.seek(0);
                                    int lineNum = 0;
                                    int edited = 1;
                                    String currentLine;
                                    String newId = "0";
                                    String lines[] = new String[MAX_SIZE];
                                    while ((currentLine = xml.readLine()) != null) {
                                        if (currentLine.contains("<id>")) {
                                            newId = currentLine.substring(currentLine.indexOf("<id>") + 4, currentLine.indexOf("</id>"));
                                            if (id.equals(newId)) {
                                                edited = 0;
                                                lines[lineNum] = "    <task><id>" + newId + "</id><value>" + val + "</value></task>";
                                            } else {
                                                lines[lineNum] = currentLine;
                                            }
                                        } else {
                                            lines[lineNum] = currentLine;
                                        }
                                        lineNum++;
                                    }
                                    xml.setLength(0);
                                    xml.seek(0);
                                    for (int i = 0; i < lineNum; i++) {
                                        xml.writeBytes((i != (lineNum-1)) ? lines[i] + "\r\n" : lines[i]);
                                    }
                
                                    xml.close();
                
                                    if (edited == 0) {
                                        err = "{\"success\":\"1\", \"id\":\"" + (Integer.parseInt(newId) + 1) + "\"}";
                                        response = "HTTP/1.1 200 OK\r\n" +
                                                "Content-Length: " + err.length() + "\r\n" +
                                                "Content-Type: application/json\r\n\r\n" +
                                                err;
                                    } else {
                                        err = "{\"error\":\"Task not found\"}";
                                        response = "HTTP/1.1 404 Not Found\r\n" +
                                                "Content-Length: " + err.length() + "\r\n" +
                                                "Content-Type: application/json\r\n\r\n" +
                                                err;
                                    }
                                } catch (IOException e) {
                                    e.printStackTrace();
                                }
                            }
                        }
                    }
                    out.write(response.getBytes());
                }

                clientSocket.close();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}