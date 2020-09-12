#ifndef EDCOMM_SYSTEM_H
#define EDCOMM_SYSTEM_H

#include <edsystem.h>
#include <vector>
#include <edlight_system.h>
#include <string>

#define SOCKET_BUFF_SIZE 
#define COMMAND_BYTE_SIZE 72

struct rplidar_scan_message;
struct pulsed_light_message;
struct nav_message;
class edsocket;

const std::string PACKET_ID_PLAY = "Play";
const std::string PACKET_ID_PAUSE = "Pause";
const std::string PACKET_ID_STOP = "Stop";
const std::string PACKET_ID_CONFIGURE = "Configure";
const uint8_t PACKET_ID_SIZE = 4;

struct Packet_ID
{
    union
    {
        uint32_t hashed_id;
        uint8_t data[PACKET_ID_SIZE];
    };
};


class edcomm_system : public edsystem
{
  public:

	edcomm_system();

	virtual ~edcomm_system();
	
    virtual void init();

	virtual void release();

	uint16_t port();

	void set_port(uint16_t port_);

	virtual void update();

	uint32_t recvFromClients(uint8_t * data, uint32_t max_size);

	void sendToClients(uint8_t * data, uint32_t size);

	virtual std::string typestr() {return TypeString();}
	
	static std::string TypeString() {return "edcomm_system";}

  private:
	typedef std::vector<edsocket*> ClientArray;
	
    void _handle_byte(uint8_t byte);
	void _sendScan(rplidar_scan_message * scanmessage);
    void _do_command();
	void _do_configure(uint8_t cur_byte);
	void _clean_closed_connections();

	ClientArray m_clients;
	
	int32_t m_server_fd;
	uint16_t m_port;
    Packet_ID m_cur_packet;
    uint32_t m_cur_index;
	Automation_Data m_cur_config;
};


#endif
