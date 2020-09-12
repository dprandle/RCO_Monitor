#include <timer.h>
#include <unistd.h>
#include <main_control.h>
#include <comm_system.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <utility.h>
#include <string.h>
#include <errno.h>
#include <socket.h>
#include <logger.h>

Comm_System::Comm_System() : m_server_fd(0), m_port(0), m_cur_packet(), m_cur_index(0)
{}

Comm_System::~Comm_System()
{}

void Comm_System::init()
{
    sockaddr_in server;

    m_server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0); //
    if (m_server_fd < 0)
    {
        int err = errno;
        elog("Could not create server! Errno: {}", strerror(err));
    }

    int optval = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(m_port);

    if (bind(m_server_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        int err = errno;
        elog("Could not bind server! Errno: {}", strerror(err));
    }

    listen(m_server_fd, 5);
    ilog("Listening on port {}", m_port);
}

uint16_t Comm_System::port()
{
    return m_port;
}

void Comm_System::set_port(uint16_t port_)
{
    m_port = port_;
}

void Comm_System::release()
{
    while (m_clients.begin() != m_clients.end())
    {
        delete m_clients.back();
        m_clients.pop_back();
    }
    close(m_server_fd);
}

uint32_t Comm_System::recvFromClients(uint8_t * data, uint32_t max_size)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < m_clients.size(); ++i)
        total += m_clients[i]->read(data + total, max_size - total);
    return total;
}

void Comm_System::sendToClients(uint8_t * data, uint32_t size)
{
    for (uint32_t i = 0; i < m_clients.size(); ++i)
        m_clients[i]->write(data, size);
}

void Comm_System::update()
{
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int32_t sockfd = accept(m_server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (sockfd != -1)
    {
        Socket * new_client = new Socket(sockfd);
        if (!new_client->start())
        {
            wlog("Received connection but could not start socket - should see deletion next");
        }
        m_clients.push_back(new_client);
        ilog("Server recieved connection from {}:{}",
             inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
    }

    // Check for closed connections and remove them if there are any
    _clean_closed_connections();

    static uint8_t buf[256];
    int32_t cnt = recvFromClients(buf, 256);
    for (int32_t i = 0; i < cnt; ++i)
        _handle_byte(buf[i]);
}

void Comm_System::_clean_closed_connections()
{
    ClientArray::iterator iter = m_clients.begin();
    while (iter != m_clients.end())
    {
        if (!(*iter)->running())
        {
            sockaddr_in cl_addr;
            socklen_t cl_len = sizeof(cl_addr);
            getpeername((*iter)->fd(), (sockaddr *)&cl_addr, &cl_len);
            std::string client_ip = std::string(inet_ntoa(cl_addr.sin_addr)) + ":" +
                                    std::to_string(ntohs(cl_addr.sin_port));

            Threaded_Fd::Error er = (*iter)->error();
            std::string errno_message = strerror(er._errno);

            switch (er.err_val)
            {
            case (Threaded_Fd::ConnectionClosed):
                ilog("Connection closed with {}", client_ip);
                break;
            case (Threaded_Fd::DataOverwrite):
                elog("Socket internal buffer overwritten with new data before previous data was "
                     "sent {} - Errno message: {}",
                     client_ip,
                     errno_message);
                break;
            case (Threaded_Fd::InvalidRead):
                elog("Socket invalid read from {} - Errno message {}", client_ip, errno_message);
                break;
            case (Threaded_Fd::InvalidWrite):
                elog("Socket invalid write to {} - Errno message: {}", client_ip, errno_message);
                break;
            case (Threaded_Fd::ThreadCreation):
                elog("Error in thread creation for connection with {}", client_ip);
                break;
            default:
                elog("No internal error but socket thread not running with {}", client_ip);
                break;
            }
            delete (*iter);
            iter = m_clients.erase(iter);
        }
        else
            ++iter;
    }
}

void Comm_System::_handle_byte(uint8_t byte)
{
    if (m_cur_index < PACKET_ID_SIZE)
    {
        m_cur_packet.data[m_cur_index] = byte;
        ++m_cur_index;
        if (m_cur_index == PACKET_ID_SIZE)
            _do_command();
    }
    else
    {
        _do_configure(byte);
    }
}

void Comm_System::_do_configure(uint8_t cur_byte)
{
}

void Comm_System::_do_command()
{
}
