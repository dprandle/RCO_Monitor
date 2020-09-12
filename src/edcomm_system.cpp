#include <edtimer.h>
#include <unistd.h>
#include <edmctrl.h>
#include <edcomm_system.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <edutility.h>
#include <string.h>
#include <errno.h>
#include <edsocket.h>
#include <edlogger.h>

edcomm_system::edcomm_system() : m_server_fd(0), m_port(0), m_cur_packet(), m_cur_index(0)
{}

edcomm_system::~edcomm_system()
{}

void edcomm_system::init()
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

uint16_t edcomm_system::port()
{
    return m_port;
}

void edcomm_system::set_port(uint16_t port_)
{
    m_port = port_;
}

void edcomm_system::release()
{
    while (m_clients.begin() != m_clients.end())
    {
        delete m_clients.back();
        m_clients.pop_back();
    }
    close(m_server_fd);
}

uint32_t edcomm_system::recvFromClients(uint8_t * data, uint32_t max_size)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < m_clients.size(); ++i)
        total += m_clients[i]->read(data + total, max_size - total);
    return total;
}

void edcomm_system::sendToClients(uint8_t * data, uint32_t size)
{
    for (uint32_t i = 0; i < m_clients.size(); ++i)
        m_clients[i]->write(data, size);
}

void edcomm_system::update()
{
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int32_t sockfd = accept(m_server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (sockfd != -1)
    {
        edsocket * new_client = new edsocket(sockfd);
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

void edcomm_system::_clean_closed_connections()
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

            edthreaded_fd::Error er = (*iter)->error();
            std::string errno_message = strerror(er._errno);

            switch (er.err_val)
            {
            case (edthreaded_fd::ConnectionClosed):
                ilog("Connection closed with {}", client_ip);
                break;
            case (edthreaded_fd::DataOverwrite):
                elog("Socket internal buffer overwritten with new data before previous data was "
                     "sent {} - Errno message: {}",
                     client_ip,
                     errno_message);
                break;
            case (edthreaded_fd::InvalidRead):
                elog("Socket invalid read from {} - Errno message {}", client_ip, errno_message);
                break;
            case (edthreaded_fd::InvalidWrite):
                elog("Socket invalid write to {} - Errno message: {}", client_ip, errno_message);
                break;
            case (edthreaded_fd::ThreadCreation):
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

void edcomm_system::_handle_byte(uint8_t byte)
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

void edcomm_system::_do_configure(uint8_t cur_byte)
{
    int adjusted_cur_index = m_cur_index - PACKET_ID_SIZE;
    if (adjusted_cur_index < PACKET_ID_SIZE)
    {
        dlog("Receiving size of each vec - byte: {} and total int val now {} with adjusted index of {}", cur_byte, m_cur_config.sample_cnt, adjusted_cur_index);
        m_cur_config.data[adjusted_cur_index] = cur_byte;
        ++m_cur_index;
        ++adjusted_cur_index;
        if (adjusted_cur_index == PACKET_ID_SIZE)
        {
            int total_byte_count = m_cur_config.sample_cnt * LIGHT_COUNT;
            dlog("Sample count for config is {} and total byte count is {}",m_cur_config.sample_cnt, total_byte_count);
            for (int i = 0; i < LIGHT_COUNT; ++i)
                m_cur_config.lights[i].ms_data.resize(m_cur_config.sample_cnt);
        }
    }
    else
    {
        adjusted_cur_index -= PACKET_ID_SIZE;
        int total_byte_count = m_cur_config.sample_cnt * LIGHT_COUNT;        
        if (adjusted_cur_index < total_byte_count)
        {
            int sample_index = adjusted_cur_index % m_cur_config.sample_cnt;
            int light_index = adjusted_cur_index / m_cur_config.sample_cnt;
            m_cur_config.lights[light_index].ms_data[sample_index] = cur_byte;
            ++m_cur_index;
            ++adjusted_cur_index;
        }

        if (adjusted_cur_index >= total_byte_count)
        {
            edlight_system * lsys = edm.sys<edlight_system>();
            lsys->set_automation_data(m_cur_config);
            m_cur_index = 0;
            m_cur_packet = {};
            m_cur_config = {};
        }
    }
}

void edcomm_system::_do_command()
{
    edlight_system * lsys = edm.sys<edlight_system>();
    if (m_cur_packet.hashed_id == hash_id(PACKET_ID_CONFIGURE))
    {
        ilog("Received configure command");
    }
    else if (m_cur_packet.hashed_id == hash_id(PACKET_ID_PLAY))
    {
        lsys->play();
        m_cur_index = 0;
        zero_buf(m_cur_packet.data, PACKET_ID_SIZE);
    }
    else if (m_cur_packet.hashed_id == hash_id(PACKET_ID_PAUSE))
    {
        lsys->pause();
        m_cur_index = 0;
        zero_buf(m_cur_packet.data, PACKET_ID_SIZE);
    }
    else if (m_cur_packet.hashed_id == hash_id(PACKET_ID_STOP))
    {
        lsys->stop();
        m_cur_index = 0;
        zero_buf(m_cur_packet.data, PACKET_ID_SIZE);
    }
}
