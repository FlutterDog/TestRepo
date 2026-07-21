#include "utility/w5500.h"
#include "utility/socket.h"
extern "C" {
	#include "string.h"
}

#include "IGAS_Eth.h"
#include "EthernetClient.h"
#include "EthernetServer.h"

int lastSocket = 0;

EthernetServer::EthernetServer(uint16_t port)
{
	_port = port;
}

void EthernetServer::begin()
{
	for (int sock = 0; sock < MAX_SOCK_NUM; sock++)
	{
		EthernetClient client(sock);
		if (client.status() == SnSR::CLOSED)        // If socket closed - it can be used as a Listening  socket
		{
			socket(sock, SnMR::TCP, _port, 0);
			listen(sock);
			EthernetClass::_server_port[sock] = _port;
			break;
		}
	}
	
	// TODO >>>>>>>> if all sockets busy - what to do? Function does not return anything in this case
}

void EthernetServer::accept()        // Check if any socket listening port
{
	int listening = 0;
	for (int sock = 0; sock < MAX_SOCK_NUM; sock++)
	{

		EthernetClient client(sock);

		if (EthernetClass::_server_port[sock] == _port)
		{
			// SerialUSB.print(sock);
			if (client.status() == SnSR::LISTEN)
			{
				listening = 1;           // We got a listening socket - lets go
				//SerialUSB.println("listen");
			}
			else
			if (client.status() == SnSR::CLOSE_WAIT )//&& !client.available())   //  Should we try to remove client available?
			{
				//SerialUSB.println("stop it");
				client.stop();
				//SerialUSB.println("stoped");
			}
		}
	}

	if (!listening)   // No any socket listening for port. Start new listen socket
	{
		//SerialUSB.println("no sock");
		begin();
		//SerialUSB.println("sock begin");
	}
	
	//SerialUSB.println("accept finished");
}


EthernetClient EthernetServer::available()
{
	accept();

	for (int sock = 0; sock < MAX_SOCK_NUM; sock++)
	{
		EthernetClient client(lastSocket);

		if (EthernetClass::_server_port[lastSocket] == _port &&
		(client.status() == SnSR::ESTABLISHED ||
		client.status() == SnSR::CLOSE_WAIT))
		{
			if (client.available())
			{
				lastSocket++;
			//	SerialUSB.println(sock);
			//	SerialUSB.println(" - available client");
				if (lastSocket == MAX_SOCK_NUM)
				lastSocket = 0;
				
				// XXX: don't always pick the lowest numbered socket.
				return client;
			}
		}
		lastSocket++;						//
		if (lastSocket == MAX_SOCK_NUM)
		lastSocket = 0;
	}
	//SerialUSB.println("available fibished");
	return EthernetClient(MAX_SOCK_NUM);
}

size_t EthernetServer::write(uint8_t b)
{
	return write(&b, 1);
}

size_t EthernetServer::write(const uint8_t *buffer, size_t size)
{
	size_t n = 0;
	
	accept();
	

	for (int sock = 0; sock < MAX_SOCK_NUM; sock++)
	{
		EthernetClient client(sock);

		if (EthernetClass::_server_port[sock] == _port && client.status() == SnSR::ESTABLISHED)
		{
			n += client.write(buffer, size);
		}
	}
	
	return n;
}
