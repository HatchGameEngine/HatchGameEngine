#include <Engine/IO/NetworkStream.h>
#include <Engine/Includes/StandardSDL2.h>

NetworkStream * NetworkStream::New(
	const char * filename, Uint32 access )
{
	NetworkStream * stream = new NetworkStream;
	if( !stream )
	{
		return NULL;
	}

	// there needs to be a thread that can be used for all network
	// transmissions either that or the sockets need to be
	// nonblocking

	switch( access )
	{
	case NetworkStream::SERVER_SOCKET: {
		// int sockfd = socket(domain, type, protocol);
		// int setsockopt(int sockfd, int level, int optname,
		// const void *optval, socklen_t optlen); int bind(int
		// sockfd, const struct sockaddr *addr, socklen_t
		// addrlen); int listen(int sockfd, int backlog); int
		// new_socket = accept(int sockfd, struct sockaddr
		// *addr, socklen_t *addrlen);
		break;
	}
	case NetworkStream::CLIENT_SOCKET: {
		// int sockfd = socket(domain, type, protocol);
		// int connect(int sockfd, const struct sockaddr *addr,
		// socklen_t addrlen);
		break;
	}
	}

	stream->f = fopen( filename, "wb" );
	if( !stream->f )
		goto FREE;

	fseek( stream->f, 0, SEEK_END );
	stream->size = ftell( stream->f );
	fseek( stream->f, 0, SEEK_SET );

	return stream;

FREE:
	delete stream;
	return NULL;
}

void NetworkStream::Close( )
{
	fclose( f );
	f = NULL;
	Stream::Close( );
}
void NetworkStream::Seek( Sint64 offset )
{
	fseek( f, offset, SEEK_SET );
}
void NetworkStream::SeekEnd( Sint64 offset )
{
	fseek( f, offset, SEEK_END );
}
void NetworkStream::Skip( Sint64 offset )
{
	fseek( f, offset, SEEK_CUR );
}
size_t NetworkStream::Position( ) { return ftell( f ); }
size_t NetworkStream::Length( ) { return size; }

size_t NetworkStream::ReadBytes( void * data, size_t n )
{
	// if (!f) Log::Print(Log::LOG_ERROR, "Attempt to read from
	// closed stream.")
	return fread( data, 1, n, f );
}

size_t NetworkStream::WriteBytes( void * data, size_t n )
{
	return fwrite( data, 1, n, f );
}
