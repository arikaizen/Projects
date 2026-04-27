#ifndef SERVER_H
#define SERVER_H

class RequestHandler;

class Server {
public:
    Server(int port, RequestHandler& handler);
    void run();

private:
    int m_port;
    RequestHandler& m_handler;
    int m_listenFd;
};

#endif // SERVER_H
