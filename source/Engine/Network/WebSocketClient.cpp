#include <Engine/Network/WebSocketClient.h>

const char* BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

enum opcode_type {
	CONTINUATION = 0x0,
	TEXT_FRAME = 0x1,
	BINARY_FRAME = 0x2,
	CLOSE = 8,
	PING = 9,
	PONG = 0xa,
};
struct wsheader_type {
	unsigned header_size;
	bool fin;
	bool mask;
	int opcode;
	int N0;
	uint64_t N;
	uint8_t masking_key[4];
};

void base64_encode_triple(unsigned char triple[3], char result[4]) {
	int tripleValue, i;

	tripleValue = triple[0];
	tripleValue *= 256;
	tripleValue += triple[1];
	tripleValue *= 256;
	tripleValue += triple[2];

	for (i = 0; i < 4; i++) {
		result[3 - i] = BASE64_CHARS[tripleValue % 64];
		tripleValue /= 64;
	}
}
int base64_encode(unsigned char* source, size_t sourcelen, char* target, size_t targetlen) {
	/* check if the result will fit in the target buffer */
	if ((sourcelen + 2) / 3 * 4 > targetlen - 1) {
		return 0;
	}

	/* encode all full triples */
	while (sourcelen >= 3) {
		base64_encode_triple(source, target);
		sourcelen -= 3;
		source += 3;
		target += 4;
	}

	/* encode the last one or two characters */
	if (sourcelen > 0) {
		unsigned char temp[3];
		memset(temp, 0, sizeof(temp));
		memcpy(temp, source, sourcelen);
		base64_encode_triple(temp, target);
		target[3] = '=';
		if (sourcelen == 1) {
			target[2] = '=';
		}

		target += 4;
	}

	/* terminate the string */
	target[0] = 0;

	return 1;
}

int base64_char_value(char base64char) {
	if (base64char >= 'A' && base64char <= 'Z') {
		return base64char - 'A';
	}
	if (base64char >= 'a' && base64char <= 'z') {
		return base64char - 'a' + 26;
	}
	if (base64char >= '0' && base64char <= '9') {
		return base64char - '0' + 2 * 26;
	}
	if (base64char == '+') {
		return 2 * 26 + 10;
	}
	if (base64char == '/') {
		return 2 * 26 + 11;
	}
	return -1;
}
int base64_decode_triple(char quadruple[4], unsigned char* result) {
	int i, triple_value, bytes_to_decode = 3, only_equals_yet = 1;
	int char_value[4];

	for (i = 0; i < 4; i++) {
		char_value[i] = base64_char_value(quadruple[i]);
	}

	/* check if the characters are valid */
	for (i = 3; i >= 0; i--) {
		if (char_value[i] < 0) {
			if (only_equals_yet && quadruple[i] == '=') {
				/* we will ignore this character
				 * anyway, make it something that does
				 * not break our calculations */
				char_value[i] = 0;
				bytes_to_decode--;
				continue;
			}
			return 0;
		}
		/* after we got a real character, no other '=' are
		 * allowed anymore */
		only_equals_yet = 0;
	}

	/* if we got "====" as input, bytes_to_decode is -1 */
	if (bytes_to_decode < 0) {
		bytes_to_decode = 0;
	}

	/* make one big value out of the partial values */
	triple_value = char_value[0];
	triple_value *= 64;
	triple_value += char_value[1];
	triple_value *= 64;
	triple_value += char_value[2];
	triple_value *= 64;
	triple_value += char_value[3];

	/* break the big value into bytes */
	for (i = bytes_to_decode; i < 3; i++) {
		triple_value /= 256;
	}

	for (i = bytes_to_decode - 1; i >= 0; i--) {
		result[i] = triple_value % 256;
		triple_value /= 256;
	}

	return bytes_to_decode;
}
size_t base64_decode(char* source, unsigned char* target, size_t targetlen) {
	char *src, *tmpptr;
	char quadruple[4], tmpresult[3];
	size_t i, tmplen = 3;
	size_t converted = 0;

	/* concatinate '===' to the source to handle unpadded base64
	 * data */
	src = (char*)malloc(strlen(source) + 5);
	if (src == NULL) {
		return -1;
	}
	strcpy(src, source);
	strcat(src, "====");
	tmpptr = src;

	/* convert as long as we get a full result */
	while (tmplen == 3) {
		/* get 4 characters to convert */
		for (i = 0; i < 4; i++) {
			/* skip invalid characters - we won't reach the
			 * end */
			while (*tmpptr != '=' && base64_char_value(*tmpptr) < 0) {
				tmpptr++;
			}
			quadruple[i] = *(tmpptr++);
		}

		/* convert the characters */
		tmplen = base64_decode_triple(quadruple, (unsigned char*)&tmpresult);

		/* check if the fit in the result buffer */
		if (targetlen < tmplen) {
			free(src);
			return -1;
		}

		/* put the partial result in the result buffer */
		memcpy(target, tmpresult, tmplen);
		target += tmplen;
		targetlen -= tmplen;
		converted += tmplen;
	}

	free(src);
	return converted;
}

socket_t socket_connect(const char* hostname, int port) {
	char sport[16];
	snprintf(sport, 16, "%d", port);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int ret;
	struct addrinfo* result;
	if ((ret = getaddrinfo(hostname, sport, &hints, &result)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		return 1;
	}

	struct addrinfo* p;
	socket_t sockfd = INVALID_SOCKET;
	for (p = result; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == INVALID_SOCKET) {
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) {
			break;
		}

		closesocket(sockfd);
		sockfd = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	return sockfd;
}
size_t socket_send_string(socket_t sockfd, const char* str, ...) {
	char line[1024];
	va_list args;
	va_start(args, str);
	vsnprintf(line, sizeof 1024, str, args);

	return send(sockfd, line, strlen(line), 0);
}

// easywsclient

WebSocketClient* WebSocketClient::New(const char* url) {
#ifdef _WIN32
	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		printf("WSAStartup Failed.\n");
		return NULL;
	}
#endif

	WebSocketClient* socket = new WebSocketClient;

	int i;
	int port;
	int flag = 1;
	int status;
	char host[512];
	char path[512];
	char line[1024];
	// char origin[1]; origin[0] = 0;
	char key_nonce[16];
	char websocket_key[256];
	socket_t sockfd = INVALID_SOCKET;
	if (sscanf(url, "ws://%[^:/]:%d/%s", host, &port, path) == 3) {
		//
	}
	else if (sscanf(url, "ws://%[^:/]/%s", host, path) == 2) {
		port = 80;
	}
	else if (sscanf(url, "ws://%[^:/]:%d", host, &port) == 2) {
		path[0] = '\0';
	}
	else if (sscanf(url, "ws://%[^:/]", host) == 1) {
		port = 80;
		path[0] = '\0';
	}
	else {
		fprintf(stderr, "ERROR: Could not parse WebSocketClient url: %s\n", url);
		goto FREE;
	}

	sockfd = socket_connect(host, port);
	if (sockfd == INVALID_SOCKET) {
		// fprintf(stderr, "Unable to connect to %s:%d\n",
		// host, port);
		goto FREE;
	}

	// XXX: this should be done non-blocking,

	socket_send_string(sockfd, "GET /%s HTTP/1.1\r\n", path);
	if (port == 80) {
		socket_send_string(sockfd, "Host: %s\r\n", host);
	}
	else {
		socket_send_string(sockfd, "Host: %s:%d\r\n", host, port);
	}
	socket_send_string(sockfd, "Upgrade: websocket\r\n");
	socket_send_string(sockfd, "Connection: Upgrade\r\n");

	srand((Uint32)time(NULL));
	for (int z = 0; z < 16; z++) {
		key_nonce[z] = rand() & 0xff;
	}
	base64_encode((uint8_t*)&key_nonce, 16, websocket_key, 256);

	socket_send_string(sockfd, "Sec-WebSocket-Key: %s\r\n", websocket_key);
	socket_send_string(sockfd, "Sec-WebSocket-Version: 13\r\n");
	socket_send_string(sockfd, "\r\n");

	for (i = 0; i < 2 || (i < 1023 && line[i - 2] != '\r' && line[i - 1] != '\n'); ++i) {
		if (recv(sockfd, line + i, 1, 0) == 0) {
			goto FREE;
		}
	}
	line[i] = 0;
	if (i == 1023) {
		fprintf(stderr, "ERROR: Got invalid status line connecting to: %s\n", url);
		goto FREE;
	}
	if (sscanf(line, "HTTP/1.1 %d", &status) != 1 || status != 101) {
		fprintf(stderr, "ERROR: Got bad status connecting to %s: %s\n", url, line);
		goto FREE;
	}
	// TODO: verify response headers,
	while (true) {
		for (i = 0; i < 2 || (i < 1023 && line[i - 2] != '\r' && line[i - 1] != '\n');
			++i) {
			if (recv(sockfd, line + i, 1, 0) == 0) {
				goto FREE;
			}
		}
		if (line[0] == '\r' && line[1] == '\n') {
			break;
		}
	}

	setsockopt(sockfd,
		IPPROTO_TCP,
		TCP_NODELAY,
		(char*)&flag,
		sizeof(flag)); // Disable Nagle's algorithm
#ifdef _WIN32
	{
		u_long on = 1;
		ioctlsocket(sockfd, FIONBIO, &on);
	}
#else
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif

	socket->socket = sockfd;
	socket->readyState = OPEN;
	socket->useMask = false;
	socket->isRxBad = false;
	return socket;

FREE:
	if (sockfd != INVALID_SOCKET) {
		closesocket(sockfd);
	}
	delete socket;
	return NULL;
}
void WebSocketClient::Poll(int timeout) {
	if (readyState == WebSocketClient::CLOSED) {
		if (timeout > 0) {
			timeval tv = {timeout / 1000, (timeout % 1000) * 1000};
			select(0, NULL, NULL, NULL, &tv);
		}
		return;
	}

	if (timeout != 0) {
		fd_set rfds;
		fd_set wfds;
		timeval tv = {timeout / 1000, (timeout % 1000) * 1000};
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(socket, &rfds);
		if (txbuf.size()) {
			FD_SET(socket, &wfds);
		}

		select(socket + 1, &rfds, &wfds, NULL, timeout > 0 ? &tv : NULL);
	}

	while (true) {
		// FD_ISSET(0, &rfds) will be true
		int N = rxbuf.size();
		ssize_t ret;
		rxbuf.resize(N + 1500);
		ret = recv(socket, (char*)&rxbuf[0] + N, 1500, 0);
		if (false) {}
		else if (ret < 0 &&
			(socketerrno == SOCKET_EWOULDBLOCK ||
				socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
			rxbuf.resize(N);
			break;
		}
		else if (ret <= 0) {
			rxbuf.resize(N);
			closesocket(socket);
			readyState = CLOSED;
			printf("readyState = CLOSED      0\n");
			fputs(ret < 0 ? "Connection error!\n" : "Connection closed!\n", stderr);
			break;
		}
		else {
			rxbuf.resize(N + ret);
		}
	}
	while (txbuf.size()) {
		int ret = send(socket, (char*)&txbuf[0], txbuf.size(), 0);
		if (false) {} // ??
		else if (ret < 0 &&
			(socketerrno == SOCKET_EWOULDBLOCK ||
				socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
			break;
		}
		else if (ret <= 0) {
			closesocket(socket);
			readyState = CLOSED;
			printf("readyState = CLOSED      1\n");
			fputs(ret < 0 ? "Connection error!\n" : "Connection closed!\n", stderr);
			break;
		}
		else {
			txbuf.erase(txbuf.begin(), txbuf.begin() + ret);
		}
	}
	if (!txbuf.size() && readyState == CLOSING) {
		closesocket(socket);
		readyState = CLOSED;
		printf("readyState = CLOSED      2\n");
	}
}
void WebSocketClient::Dispatch(void (*callback)(void* mem, size_t size)) {
	// TODO: consider acquiring a lock on rxbuf...
	if (isRxBad) {
		printf("bad rx\n");
		return;
	}
	while (true) {
		wsheader_type ws;
		if (rxbuf.size() < 2) {
			return; /* Need at least 2 */
		}

		const uint8_t* data = (uint8_t*)&rxbuf[0]; // peek, but don't
			// consume
		ws.fin = (data[0] & 0x80) == 0x80;
		ws.opcode = (opcode_type)(data[0] & 0x0f);
		ws.mask = (data[1] & 0x80) == 0x80;
		ws.N0 = (data[1] & 0x7f);
		ws.header_size =
			2 + (ws.N0 == 126 ? 2 : 0) + (ws.N0 == 127 ? 8 : 0) + (ws.mask ? 4 : 0);

		if (rxbuf.size() < ws.header_size) {
			return; /* Need: ws.header_size - rxbuf.size()
			         */
		}

		int i = 0;
		if (ws.N0 < 126) {
			ws.N = ws.N0;
			i = 2;
		}
		else if (ws.N0 == 126) {
			ws.N = 0;
			ws.N |= ((uint64_t)data[2]) << 8;
			ws.N |= ((uint64_t)data[3]) << 0;
			i = 4;
		}
		else if (ws.N0 == 127) {
			ws.N = 0;
			ws.N |= ((uint64_t)data[2]) << 56;
			ws.N |= ((uint64_t)data[3]) << 48;
			ws.N |= ((uint64_t)data[4]) << 40;
			ws.N |= ((uint64_t)data[5]) << 32;
			ws.N |= ((uint64_t)data[6]) << 24;
			ws.N |= ((uint64_t)data[7]) << 16;
			ws.N |= ((uint64_t)data[8]) << 8;
			ws.N |= ((uint64_t)data[9]) << 0;
			i = 10;
			if (ws.N & 0x8000000000000000ull) {
				// https://tools.ietf.org/html/rfc6455
				// writes the "the most significant bit
				// MUST be 0."
				//
				// We can't drop the frame, because (1)
				// we don't we don't know how much data
				// to skip over to find the next
				// header, and (2) this would be an
				// impractically long length, even if
				// it were valid. So just Close() and
				// return immediately for now.
				isRxBad = true;
				fprintf(stderr,
					"ERROR: Frame has invalid frame length. Closing.\n");
				Close();
				return;
			}
		}

		if (ws.mask) {
			ws.masking_key[0] = ((uint8_t)data[i + 0]) << 0;
			ws.masking_key[1] = ((uint8_t)data[i + 1]) << 0;
			ws.masking_key[2] = ((uint8_t)data[i + 2]) << 0;
			ws.masking_key[3] = ((uint8_t)data[i + 3]) << 0;
		}
		else {
			ws.masking_key[0] = 0;
			ws.masking_key[1] = 0;
			ws.masking_key[2] = 0;
			ws.masking_key[3] = 0;
		}

		// Note: The checks above should hopefully ensure this
		// addition
		//       cannot overflow:
		if (rxbuf.size() < ws.header_size + ws.N) {
			return; /* Need: ws.header_size+ws.N -
			           rxbuf.size() */
		}

		// We got a whole message, now do something with it:

		if (ws.opcode == opcode_type::TEXT_FRAME ||
			ws.opcode == opcode_type::BINARY_FRAME ||
			ws.opcode == opcode_type::CONTINUATION) {
			if (ws.mask) {
				for (size_t i = 0; i != ws.N; i++) {
					rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3];
				}
			}

			receivedData.insert(receivedData.end(),
				rxbuf.begin() + ws.header_size,
				rxbuf.begin() + ws.header_size + (size_t)ws.N); // just feed

			if (ws.fin) {
				if (callback) {
					callback(receivedData.data(), receivedData.size());
				}

				receivedData.erase(receivedData.begin(), receivedData.end());
				std::vector<uint8_t>().swap(receivedData); // free memory
			}
		}
		else if (ws.opcode == opcode_type::PING) {
			if (ws.mask) {
				for (size_t i = 0; i != ws.N; ++i) {
					rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3];
				}
			}
			std::string data(rxbuf.begin() + ws.header_size,
				rxbuf.begin() + ws.header_size + (size_t)ws.N);
			SendData(opcode_type::PONG, data.data(), data.size());
		}
		else if (ws.opcode == opcode_type::PONG) {}
		else if (ws.opcode == opcode_type::CLOSE) {
			Close();
		}
		else {
			fprintf(stderr, "ERROR: Got unexpected WebSocketClient message.\n");
			Close();
		}

		rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size + (size_t)ws.N);
	}
}

size_t WebSocketClient::BytesToRead() {
	if (readyState == WebSocketClient::CLOSED || readyState == WebSocketClient::CLOSING) {
		return 0;
	}

	if (isRxBad) {
		printf("bad rx\n");
		return 0;
	}
	while (true) {
		wsheader_type ws;
		/* Need at least 2 */
		if (rxbuf.size() < 2) {
			return 0;
		}

		// peek, but don't consume
		const uint8_t* data = (uint8_t*)&rxbuf[0];
		ws.fin = (data[0] & 0x80) == 0x80;
		ws.opcode = (opcode_type)(data[0] & 0x0f);
		ws.mask = (data[1] & 0x80) == 0x80;
		ws.N0 = (data[1] & 0x7f);
		ws.header_size =
			2 + (ws.N0 == 126 ? 2 : 0) + (ws.N0 == 127 ? 8 : 0) + (ws.mask ? 4 : 0);

		if (rxbuf.size() < ws.header_size) {
			return 0; /* Need: ws.header_size -
			             rxbuf.size() */
		}

		int i = 0;
		if (ws.N0 < 126) {
			ws.N = ws.N0;
			i = 2;
		}
		else if (ws.N0 == 126) {
			ws.N = 0;
			ws.N |= ((uint64_t)data[2]) << 8;
			ws.N |= ((uint64_t)data[3]) << 0;
			i = 4;
		}
		else if (ws.N0 == 127) {
			ws.N = 0;
			ws.N |= ((uint64_t)data[2]) << 56;
			ws.N |= ((uint64_t)data[3]) << 48;
			ws.N |= ((uint64_t)data[4]) << 40;
			ws.N |= ((uint64_t)data[5]) << 32;
			ws.N |= ((uint64_t)data[6]) << 24;
			ws.N |= ((uint64_t)data[7]) << 16;
			ws.N |= ((uint64_t)data[8]) << 8;
			ws.N |= ((uint64_t)data[9]) << 0;
			i = 10;
			if (ws.N & 0x8000000000000000ull) {
				// https://tools.ietf.org/html/rfc6455
				// writes the "the most significant bit
				// MUST be 0."
				//
				// We can't drop the frame, because (1)
				// we don't we don't know how much data
				// to skip over to find the next
				// header, and (2) this would be an
				// impractically long length, even if
				// it were valid. So just Close() and
				// return immediately for now.
				isRxBad = true;
				fprintf(stderr,
					"ERROR: Frame has invalid frame length. Closing.\n");
				Close();
				return 0;
			}
		}

		if (ws.mask) {
			ws.masking_key[0] = ((uint8_t)data[i + 0]) << 0;
			ws.masking_key[1] = ((uint8_t)data[i + 1]) << 0;
			ws.masking_key[2] = ((uint8_t)data[i + 2]) << 0;
			ws.masking_key[3] = ((uint8_t)data[i + 3]) << 0;
		}
		else {
			ws.masking_key[0] = 0;
			ws.masking_key[1] = 0;
			ws.masking_key[2] = 0;
			ws.masking_key[3] = 0;
		}

		// Note: The checks above should hopefully ensure this
		// addition
		//       cannot overflow:
		if (rxbuf.size() < ws.header_size + ws.N) {
			return 0; /* Need: ws.header_size+ws.N -
			             rxbuf.size() */
		}

		// We got a whole message, now do something with it:

		if (ws.opcode == opcode_type::TEXT_FRAME ||
			ws.opcode == opcode_type::BINARY_FRAME ||
			ws.opcode == opcode_type::CONTINUATION) {
			if (ws.mask) {
				for (size_t i = 0; i != ws.N; i++) {
					rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3];
				}
			}

			receivedData.insert(receivedData.end(),
				rxbuf.begin() + ws.header_size,
				rxbuf.begin() + ws.header_size + (size_t)ws.N); // just feed

			if (ws.fin) {
				rxbuf.erase(rxbuf.begin(),
					rxbuf.begin() + ws.header_size + (size_t)ws.N);
				return receivedData.size();

				// receivedData.erase(receivedData.begin(),
				// receivedData.end());
				// std::vector<uint8_t>().swap(receivedData);
				// // free memory
			}
		}
		else if (ws.opcode == opcode_type::PING) {
			if (ws.mask) {
				for (size_t i = 0; i != ws.N; ++i) {
					rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3];
				}
			}
			std::string data(rxbuf.begin() + ws.header_size,
				rxbuf.begin() + ws.header_size + (size_t)ws.N);
			SendData(opcode_type::PONG, data.data(), data.size());
		}
		else if (ws.opcode == opcode_type::PONG) {}
		else if (ws.opcode == opcode_type::CLOSE) {
			Close();
		}
		else {
			fprintf(stderr, "ERROR: Got unexpected WebSocketClient message.\n");
			Close();
		}

		rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size + (size_t)ws.N);
	}
	return 0;
}
size_t WebSocketClient::ReadBytes(void* data, size_t n) {
	memcpy(data, receivedData.data(), n);
	receivedData.erase(receivedData.begin(), receivedData.begin() + n);
	return n;
}
Uint32 WebSocketClient::ReadUint32() {
	Uint32 data;
	ReadBytes(&data, sizeof(data));
	return data;
}
Sint32 WebSocketClient::ReadSint32() {
	Sint32 data;
	ReadBytes(&data, sizeof(data));
	return data;
}
float WebSocketClient::ReadFloat() {
	float data;
	ReadBytes(&data, sizeof(data));
	return data;
}
char* WebSocketClient::ReadString() {
	char* data = (char*)receivedData.data();
	char* dataStart = data;
	char* dataEnd = data + receivedData.size();
	size_t str_len = 0;
	while (*data && data < dataEnd) {
		data++;
		str_len++;
	}

	char* output = (char*)malloc(str_len + 1);
	ReadBytes(output, str_len);
	output[str_len] = 0;

	return output;
}

void WebSocketClient::SendData(int type, const void* message, int64_t message_size) {
	// TODO:
	// Masking key should (must) be derived from a high quality
	// random number generator, to mitigate attacks on
	// non-WebSocketClient friendly middleware:
	const uint8_t masking_key[4] = {0x12, 0x34, 0x56, 0x78};
	// TODO: consider acquiring a lock on txbuf...
	if (readyState == WebSocketClient::CLOSING || readyState == WebSocketClient::CLOSED) {
		return;
	}

	std::vector<uint8_t> header;
	header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) +
			(useMask ? 4 : 0),
		0x00);
	header[0] = 0x80 | type;

	if (message_size < 126) {
		header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
		if (useMask) {
			header[2] = masking_key[0];
			header[3] = masking_key[1];
			header[4] = masking_key[2];
			header[5] = masking_key[3];
		}
	}
	else if (message_size < 65536) {
		header[1] = 126 | (useMask ? 0x80 : 0);
		header[2] = (message_size >> 8) & 0xff;
		header[3] = (message_size >> 0) & 0xff;
		if (useMask) {
			header[4] = masking_key[0];
			header[5] = masking_key[1];
			header[6] = masking_key[2];
			header[7] = masking_key[3];
		}
	}
	else { // TODO: run coverage testing here
		header[1] = 127 | (useMask ? 0x80 : 0);
		header[2] = (message_size >> 56) & 0xff;
		header[3] = (message_size >> 48) & 0xff;
		header[4] = (message_size >> 40) & 0xff;
		header[5] = (message_size >> 32) & 0xff;
		header[6] = (message_size >> 24) & 0xff;
		header[7] = (message_size >> 16) & 0xff;
		header[8] = (message_size >> 8) & 0xff;
		header[9] = (message_size >> 0) & 0xff;
		if (useMask) {
			header[10] = masking_key[0];
			header[11] = masking_key[1];
			header[12] = masking_key[2];
			header[13] = masking_key[3];
		}
	}
	// N.B. - txbuf will keep growing until it can be transmitted
	// over the socket:
	txbuf.insert(txbuf.end(), header.begin(), header.end());
	txbuf.insert(txbuf.end(), (uint8_t*)message, (uint8_t*)message + message_size);
	if (useMask) {
		size_t message_offset = txbuf.size() - message_size;
		for (size_t i = 0; i != (size_t)message_size; i++) {
			txbuf[message_offset + i] ^= masking_key[i & 0x3];
		}
	}
}
void WebSocketClient::SendBinary(const void* message, int64_t message_size) {
	SendData(opcode_type::BINARY_FRAME, message, message_size);
}
void WebSocketClient::SendText(const char* message) {
	SendData(opcode_type::TEXT_FRAME, message, (int64_t)strlen(message));
}
void WebSocketClient::Close() {
	if (readyState == WebSocketClient::CLOSING || readyState == WebSocketClient::CLOSED) {
		return;
	}

	readyState = WebSocketClient::CLOSING;
	printf("readyState = WebSocketClient::CLOSING\n");
	uint8_t closeFrame[6] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00}; // last 4 bytes
		// are a masking
		// key
	std::vector<uint8_t> header(closeFrame, closeFrame + 6);
	txbuf.insert(txbuf.end(), header.begin(), header.end());

#ifdef _WIN32
	WSACleanup();
#endif
}
