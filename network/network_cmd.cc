#include "network_cmd.h"
#include "network.h"
#include "network_packet.h"
#include "network_socket_list.h"
#include "../path_explorer.h"

#include <stdlib.h>

// needed by world to kick clients if needed
SOCKET network_command_t::get_sender()
{
	return packet->get_sender();
}


network_command_t::network_command_t(uint16 id_)
{
	packet = new packet_t();
	id = id_;
	our_client_id = (uint32)network_get_client_id();
	ready = false;
}


// default constructor
network_command_t::network_command_t()
: packet(NULL), id(0), our_client_id(0), ready(false)
{}


bool network_command_t::receive(packet_t *p)
{
	ready = true;
	delete packet;
	packet = p;
	id = p->get_id();
	rdwr();
	return (!packet->has_failed());
}

network_command_t::~network_command_t()
{
	delete packet;
	packet = NULL;
}

void network_command_t::rdwr()
{
	if (packet->is_saving()) {
		packet->set_id(id);
		ready = true;
	}
	packet->rdwr_long(our_client_id);
	dbg->message("network_command_t::rdwr", "%s packet_id=%d, client_id=%ld", packet->is_saving() ? "write" : "read", id, our_client_id);
}


void network_command_t::prepare_to_send()
{
	// saves the data to the packet
	if(!ready) {
		rdwr();
	}
}


bool network_command_t::send(SOCKET s)
{
	prepare_to_send();
	packet->send(s, true);
	bool ok = packet->is_ready();
	if (!ok) {
		dbg->warning("network_command_t::send", "send packet_id=%d to [%d] failed", id, s);
	}
	return ok;
}


bool network_command_t::is_local_cmd()
{
	return (our_client_id == (uint32)network_get_client_id());
}


packet_t* network_command_t::copy_packet() const
{
	if (packet) {
		return new packet_t(*packet);
	}
	else {
		return NULL;
	}
}


void nwc_auth_player_t::rdwr()
{
	network_command_t::rdwr();
	for(uint32 i=0; i<20; i++) {
		packet->rdwr_byte( hash[i] );
	}
	packet->rdwr_byte( player_nr );
	packet->rdwr_short(player_unlocked);
}


nwc_service_t::~nwc_service_t()
{
	delete socket_info;
	delete address_list;
	free(text);
}

extern address_list_t blacklist;

void nwc_service_t::rdwr()
{
	network_command_t::rdwr();

	packet->rdwr_long(flag);
	packet->rdwr_long(number);
	switch(flag) {
		case SRVC_LOGIN_ADMIN:
		case SRVC_BAN_IP:
		case SRVC_UNBAN_IP:
		case SRVC_ADMIN_MSG:
		case SRVC_GET_COMPANY_LIST:
		case SRVC_GET_COMPANY_INFO:
			packet->rdwr_str(text);
			break;

		case SRVC_GET_CLIENT_LIST:
			if (packet->is_loading()) {
				socket_info = new vector_tpl<socket_info_t*>(10);
				// read the list
				socket_list_t::rdwr(packet, socket_info);
			}
			else {
				// write the socket list
				socket_list_t::rdwr(packet);
			}
			break;
		case SRVC_GET_BLACK_LIST:
			if (packet->is_loading()) {
				address_list = new address_list_t();
				address_list->rdwr(packet);
			}
			else {
				blacklist.rdwr(packet);
			}
			break;
		default: ;
	}
}
