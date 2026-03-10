#ifndef ENGINE_FILESYSTEM_VFS_EGGVFS_H
#define ENGINE_FILESYSTEM_VFS_EGGVFS_H

#include <Engine/Filesystem/VFS/ArchiveVFS.h>

#define EGG_DECOMPRESSION_BUFFER_SIZE 8192
#define EGG_BLOCK_BUFFER_SIZE 8192

enum {
	EGG_COMPRESSION_TYPE_NONE,
	EGG_COMPRESSION_TYPE_GZIP
};

class EggVFS : public ArchiveVFS {
private:
	int ReadHeader();
	bool LoadEntry(std::string name, size_t size);
	static bool InitInflateStream(void* inflateStream);
	static bool ReadGzipHeader(Stream* stream);
	int Seek(size_t position);
	int Decompress(Uint8* out, size_t bytesToRead);
	static unsigned int CalculateChecksum(char* header);
	static size_t GetNextBlockOffset(size_t size);
	bool ShouldPreloadEntry(VFSEntry* entry);

	size_t StartOffset = 0;
	size_t StreamPosition = 0;

	int CompressionType = EGG_COMPRESSION_TYPE_NONE;
	char InputBuffer[EGG_DECOMPRESSION_BUFFER_SIZE];
	char BlockBuffer[EGG_BLOCK_BUFFER_SIZE];

	void* InflateStream = nullptr;

public:
	EggVFS(Uint16 flags) : ArchiveVFS(flags) {};
	virtual ~EggVFS();

	bool Open(Stream* stream);
	bool Open(Stream* stream, int type, size_t position);

	static bool IsFile(Stream* stream, int& compressionType);

	virtual bool ReadEntryData(VFSEntry* entry, Uint8* memory, size_t memSize);
	virtual void Close();

	static Uint64 MaxCompressedPreloadSize;
};

#endif /* ENGINE_FILESYSTEM_VFS_EGGVFS_H */
