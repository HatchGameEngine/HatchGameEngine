#ifndef ENGINE_IO_SERIALIZER_H
#define ENGINE_IO_SERIALIZER_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class Serializer {
private:
	void WriteValue(VMValue val);
	void WriteObject(Obj* obj);
	void WriteObjectsChunk();
	void WriteTextChunk();
	Uint32 GetUniqueObjectID(Obj* obj);
	void BeginChunk(Uint32 type);
	void FinishChunk();
	void AddChunkToList();
	void WriteObjectPreamble(Uint8 type);
	void PatchObjectSize();
	void AddUniqueString(char* chars, size_t length);
	Uint32 GetUniqueStringID(char* chars, size_t length);
	void AddUniqueObject(Obj* obj);
	void GetObject();
	void ReadObject(Obj* obj);
	VMValue ReadValue();

public:
	std::map<Obj*, Uint32> ObjToID;
	std::vector<Obj*> ObjList;
	Stream* StreamPtr;
	size_t StoredStreamPos;
	size_t StoredChunkPos;
	Uint32 CurrentChunkType;
	struct Chunk {
		Uint32 Type;
		size_t Offset;
		size_t Size;
	};
	std::vector<Serializer::Chunk> ChunkList;
	Serializer::Chunk LastChunk;
	struct String {
		Uint32 Length;
		char* Chars;
	};
	std::vector<Serializer::String> StringList;
	enum { CHUNK_OBJS = MAGIC_BE32("OBJS"), CHUNK_TEXT = MAGIC_BE32("TEXT") };
	enum { VAL_TYPE_NULL, VAL_TYPE_INTEGER, VAL_TYPE_DECIMAL, VAL_TYPE_OBJECT, END = 0xFF };
	enum { OBJ_TYPE_UNIMPLEMENTED, OBJ_TYPE_STRING, OBJ_TYPE_ARRAY, OBJ_TYPE_MAP };
	static Uint32 Magic;
	static Uint32 Version;

	Serializer(Stream* stream);
	void Store(VMValue val);
	bool ReadObjectsChunk();
	bool ReadTextChunk();
	VMValue Retrieve();
};

#endif /* ENGINE_IO_SERIALIZER_H */
