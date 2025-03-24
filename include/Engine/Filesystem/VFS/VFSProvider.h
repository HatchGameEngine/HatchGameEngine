#ifndef ENGINE_FILESYSTEM_VFS_VFSPROVIDER_H
#define ENGINE_FILESYSTEM_VFS_VFSPROVIDER_H

#include <Engine/Filesystem/VFS/VFSEntry.h>

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

enum VFSType { FILESYSTEM, HATCH, MEMORY };

#define VFS_READABLE 1
#define VFS_WRITABLE 2

struct VFSOpenStream {
	VFSEntry* EntryPtr;
	Stream* StreamPtr;
};

enum VFSEnumerationResult {
	CANNOT_ENUMERATE = CHAR_MIN,
	CANNOT_ENUMERATE_RELATIVELY,
	INVALID_PATH,

	NO_RESULTS = 0,
	SUCCESS
};

struct VFSEnumeration {
	VFSEnumerationResult Result;
	std::vector<std::string> Entries;
};

class VFSProvider {
private:
	std::string MountPoint;
	Uint16 Flags;

protected:
	bool Opened;
	std::vector<VFSOpenStream> OpenStreams;

	void AddOpenStream(VFSEntry* entry, Stream* stream);

	virtual VFSEntry* FindFile(const char* filename);

public:
	VFSProvider(Uint16 flags);
	virtual ~VFSProvider();

	bool IsOpen();
	bool IsReadable();
	bool IsWritable();
	bool CanUnmount();

	void SetWritable(bool writable);

	virtual std::string TransformFilename(const char* filename);
	virtual bool SupportsCompression();
	virtual bool SupportsEncryption();
	virtual bool HasFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual VFSEnumeration EnumerateFiles(const char* path);
	virtual Stream* OpenReadStream(const char* filename);
	virtual Stream* OpenWriteStream(const char* filename);
	virtual Stream* OpenAppendStream(const char* filename);
	virtual bool CloseStream(Stream*);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_VFSPROVIDER_H */
