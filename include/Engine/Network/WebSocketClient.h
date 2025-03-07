#ifndef ENGINE_NETWORK_WEBSOCKETCLIENT_H
#define ENGINE_NETWORK_WEBSOCKETCLIENT_H

#include <Engine/Includes/Standard.h>
#include <Engine/Network/WebSocketIncludes.h>
#include <time.h>

class WebSocketClient {
public:
	enum {
		CLOSING = 0,
		CLOSED = 1,
		CONNECTING = 2,
		OPEN = 3,
	};
	std::vector<uint8_t> rxbuf;
	std::vector<uint8_t> txbuf;
	std::vector<uint8_t> receivedData;
	socket_t socket;
	int readyState;
	bool useMask;
	bool isRxBad;

	static WebSocketClient* New(const char* url);
	void Poll(int timeout);
	void Dispatch(void (*callback)(void* mem, size_t size));
	size_t BytesToRead();
	size_t ReadBytes(void* data, size_t n);
	Uint32 ReadUint32();
	Sint32 ReadSint32();
	float ReadFloat();
	char* ReadString();
	void SendData(int type, const void* message, int64_t message_size);
	void SendBinary(const void* message, int64_t message_size);
	void SendText(const char* message);
	void Close();
};

#endif /* ENGINE_NETWORK_WEBSOCKETCLIENT_H */
