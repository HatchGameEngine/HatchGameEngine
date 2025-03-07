#include <Engine/IO/Serializer.h>

#include <Engine/Utilities/StringUtils.h>

Uint32 Serializer::Magic = 0x9D939FF0;
Uint32 Serializer::Version = 0x00000001;

Serializer::Serializer(Stream* stream) {
	StreamPtr = stream;
	ObjToID.clear();
	ObjList.clear();
	ChunkList.clear();
	StringList.clear();
}

void Serializer::WriteValue(VMValue val) {
	switch (val.Type) {
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL: {
		float d = AS_DECIMAL(val);
		StreamPtr->WriteByte(Serializer::VAL_TYPE_DECIMAL);
		StreamPtr->WriteFloat(d);
		return;
	}
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER: {
		int i = AS_INTEGER(val);
		StreamPtr->WriteByte(Serializer::VAL_TYPE_INTEGER);
		StreamPtr->WriteInt64(i);
		return;
	}
	case VAL_OBJECT: {
		Obj* obj = AS_OBJECT(val);
		Uint32 objectID = GetUniqueObjectID(obj);
		if (objectID != 0xFFFFFFFF) {
			switch (obj->Type) {
			case OBJ_STRING:
			case OBJ_ARRAY:
			case OBJ_MAP:
				StreamPtr->WriteByte(Serializer::VAL_TYPE_OBJECT);
				StreamPtr->WriteUInt32(objectID);
				return;
			default:
				StreamPtr->WriteByte(Serializer::VAL_TYPE_NULL);
				return;
			}
		}
	}
	default:
		StreamPtr->WriteByte(Serializer::VAL_TYPE_NULL);
		return;
	}
}

void Serializer::WriteObject(Obj* obj) {
	switch (obj->Type) {
	case OBJ_STRING: {
		WriteObjectPreamble(Serializer::OBJ_TYPE_STRING);

		ObjString* string = (ObjString*)obj;
		StreamPtr->WriteUInt32(GetUniqueStringID(string->Chars, string->Length));
		break;
	}
	case OBJ_ARRAY: {
		WriteObjectPreamble(Serializer::OBJ_TYPE_ARRAY);

		ObjArray* array = (ObjArray*)obj;
		size_t sz = array->Values->size();
		StreamPtr->WriteUInt32(sz);

		for (size_t i = 0; i < sz; i++) {
			VMValue arrayVal = (*array->Values)[i];
			WriteValue(arrayVal);
		}
		break;
	}
	case OBJ_MAP: {
		WriteObjectPreamble(Serializer::OBJ_TYPE_MAP);

		ObjMap* map = (ObjMap*)obj;
		Uint32 numKeys = 0;
		Uint32 numValues = 0;
		map->Keys->WithAll([&numKeys](Uint32, char*) -> void {
			numKeys++;
		});
		map->Values->WithAll([&numValues](Uint32, VMValue) -> void {
			numValues++;
		});

		StreamPtr->WriteUInt32(numKeys);
		StreamPtr->WriteUInt32(numValues);

		map->Keys->WithAll([this](Uint32, char* ptr) -> void {
			StreamPtr->WriteUInt32(GetUniqueStringID(ptr, strlen(ptr)));
		});
		map->Values->WithAll([this](Uint32 hash, VMValue mapVal) -> void {
			StreamPtr->WriteUInt32(hash);
			WriteValue(mapVal);
		});
		break;
	}
	default:
		Log::Print(Log::LOG_WARN,
			"Cannot serialize an object of type %s; ignoring",
			GetObjectTypeString(obj->Type));
		WriteObjectPreamble(Serializer::OBJ_TYPE_UNIMPLEMENTED);
	}

	PatchObjectSize();
}

void Serializer::WriteObjectsChunk() {
	BeginChunk(Serializer::CHUNK_OBJS);

	StreamPtr->WriteUInt32(ObjList.size());

	for (size_t i = 0; i < ObjList.size(); i++) {
		WriteObject(ObjList[i]);
	}

	FinishChunk();
	AddChunkToList();
}

void Serializer::WriteTextChunk() {
	BeginChunk(Serializer::CHUNK_TEXT);

	StreamPtr->WriteUInt32(StringList.size());

	for (size_t i = 0; i < StringList.size(); i++) {
		Uint32 length = StringList[i].Length;
		StreamPtr->WriteUInt32(length);
		StreamPtr->WriteBytes(StringList[i].Chars, length);
	}

	FinishChunk();

	// Ensure that TEXT chunks are read early
	ChunkList.insert(ChunkList.begin(), LastChunk);
}

Uint32 Serializer::GetUniqueObjectID(Obj* obj) {
	if (ObjToID.count(obj)) {
		return ObjToID[obj];
	}

	return 0xFFFFFFFF;
}

void Serializer::BeginChunk(Uint32 type) {
	CurrentChunkType = type;
	StoredChunkPos = StreamPtr->Position();
}

void Serializer::FinishChunk() {
	LastChunk.Type = CurrentChunkType;
	LastChunk.Offset = StoredChunkPos;
	LastChunk.Size = StreamPtr->Position() - StoredChunkPos;

	// Write end marker
	StreamPtr->WriteByte(Serializer::END);
}

void Serializer::AddChunkToList() {
	ChunkList.push_back(LastChunk);
}

void Serializer::WriteObjectPreamble(Uint8 type) {
	StreamPtr->WriteByte(type);
	StreamPtr->WriteUInt32(0); // To be patched in later

	StoredStreamPos = StreamPtr->Position();
}

void Serializer::PatchObjectSize() {
	size_t curPos = StreamPtr->Position();
	size_t size = curPos - StoredStreamPos;
	StreamPtr->Seek(StoredStreamPos - 4);
	StreamPtr->WriteUInt32(size);
	StreamPtr->Seek(curPos);
}

void Serializer::AddUniqueString(char* chars, size_t length) {
	if (StringList.size() >= 0xFFFFFFFF) {
		return;
	}

	for (size_t i = 0; i < StringList.size(); i++) {
		if (StringList[i].Length == length && !memcmp(StringList[i].Chars, chars, length)) {
			return;
		}
	}

	Serializer::String str;
	str.Length = length;
	str.Chars = chars;
	StringList.push_back(str);
}

Uint32 Serializer::GetUniqueStringID(char* chars, size_t length) {
	for (size_t i = 0; i < StringList.size(); i++) {
		if (StringList[i].Length == length && !memcmp(StringList[i].Chars, chars, length)) {
			return (Uint32)i;
		}
	}

	return 0xFFFFFFFF;
}

void Serializer::AddUniqueObject(Obj* obj) {
	if (ObjToID.count(obj) || ObjList.size() >= 0xFFFFFFFF) {
		return;
	}

	ObjToID[obj] = ObjList.size();
	ObjList.push_back(obj);

	switch (obj->Type) {
	case OBJ_STRING: {
		ObjString* string = (ObjString*)obj;
		AddUniqueString(string->Chars, string->Length);
		return;
	}
	case OBJ_ARRAY: {
		ObjArray* array = (ObjArray*)obj;
		for (size_t i = 0; i < array->Values->size(); i++) {
			VMValue arrayVal = (*array->Values)[i];
			if (IS_OBJECT(arrayVal)) {
				AddUniqueObject(AS_OBJECT(arrayVal));
			}
		}
		return;
	}
	case OBJ_MAP: {
		ObjMap* map = (ObjMap*)obj;
		map->Keys->WithAll([this](Uint32, char* mapKey) -> void {
			AddUniqueString(mapKey, strlen(mapKey));
		});
		map->Values->WithAll([this](Uint32, VMValue mapVal) -> void {
			if (IS_OBJECT(mapVal)) {
				AddUniqueObject(AS_OBJECT(mapVal));
			}
		});
		return;
	}
	default:
		return;
	}
}

void Serializer::Store(VMValue val) {
	// Write header
	StreamPtr->WriteUInt32(Serializer::Magic);
	StreamPtr->WriteUInt32(Serializer::Version);

	// We're gonna patch this later.
	size_t chunkAddrPos = StreamPtr->Position();
	StreamPtr->WriteUInt32(0);

	// See if we can add this value as an object
	if (IS_OBJECT(val)) {
		AddUniqueObject(AS_OBJECT(val));
	}

	// Write the value
	WriteValue(val);

	// End marker
	StreamPtr->WriteByte(Serializer::END);

	// Write the objects chunk, if there are objects
	if (ObjList.size()) {
		Serializer::WriteObjectsChunk();
	}

	// Write the text chunk, if there are strings
	// (currently only possible if there are objects)
	if (StringList.size()) {
		Serializer::WriteTextChunk();
	}

	// Write the chunk list
	size_t chunkListPos = StreamPtr->Position();

	Uint32 numChunks = ChunkList.size();

	StreamPtr->WriteUInt32(numChunks);

	for (Uint32 i = 0; i < numChunks; i++) {
		StreamPtr->WriteUInt32BE(ChunkList[i].Type);
		StreamPtr->WriteUInt32(ChunkList[i].Offset);
		StreamPtr->WriteUInt32(ChunkList[i].Size);
	}

	// Write a pointer to the chunk list
	size_t curPos = StreamPtr->Position();
	StreamPtr->Seek(chunkAddrPos);
	StreamPtr->WriteUInt32(chunkListPos);
	StreamPtr->Seek(curPos);

	// Done here
	StreamPtr->WriteByte(Serializer::END);
	ObjToID.clear();
	ObjList.clear();
}

void Serializer::GetObject() {
	Uint8 type = StreamPtr->ReadByte();
	Uint32 size = StreamPtr->ReadUInt32();
	switch (type) {
	case Serializer::OBJ_TYPE_STRING: {
		Uint32 stringID = StreamPtr->ReadUInt32();
		if (stringID >= StringList.size()) {
			Log::Print(Log::LOG_ERROR, "Attempted to read an invalid string ID!");
			return;
		}
		else if (StringList[stringID].Chars == nullptr) {
			ObjList.push_back(nullptr);
			return;
		}
		Uint32 length = StringList[stringID].Length;
		ObjString* string = AllocString(length);
		memcpy(string->Chars, StringList[stringID].Chars, length);
		ObjList.push_back((Obj*)string);
		return;
	}
	case Serializer::OBJ_TYPE_ARRAY: {
		ObjArray* array = NewArray();
		ObjList.push_back((Obj*)array);
		StreamPtr->Skip(size);
		return;
	}
	case Serializer::OBJ_TYPE_MAP: {
		ObjMap* map = NewMap();
		ObjList.push_back((Obj*)map);
		StreamPtr->Skip(size);
		return;
	}
	default:
		if (type == OBJ_TYPE_UNIMPLEMENTED) {
			Log::Print(Log::LOG_WARN, "Ignoring unimplemented object type");
		}
		else {
			Log::Print(
				Log::LOG_ERROR, "Attempted to deserialize an invalid object type!");
		}
		ObjList.push_back(nullptr);
		StreamPtr->Skip(size);
		return;
	}
}

void Serializer::ReadObject(Obj* obj) {
	Uint8 type = StreamPtr->ReadByte();
	Uint32 size = StreamPtr->ReadUInt32();
	switch (type) {
	case Serializer::OBJ_TYPE_ARRAY: {
		Uint32 sz = StreamPtr->ReadUInt32();
		ObjArray* array = (ObjArray*)obj;
		for (Uint32 i = 0; i < sz; i++) {
			array->Values->push_back(ReadValue());
		}
		return;
	}
	case Serializer::OBJ_TYPE_MAP: {
		Uint32 numKeys = StreamPtr->ReadUInt32();
		Uint32 numValues = StreamPtr->ReadUInt32();
		ObjMap* map = (ObjMap*)obj;
		for (Uint32 i = 0; i < numKeys; i++) {
			Uint32 stringID = StreamPtr->ReadUInt32();
			if (stringID >= StringList.size()) {
				Log::Print(
					Log::LOG_ERROR, "Attempted to read an invalid string ID!");
				continue;
			}
			else if (StringList[stringID].Chars != nullptr) {
				Uint32 length = StringList[stringID].Length;
				char* mapKey = StringUtils::Create(
					(void*)StringList[stringID].Chars, length);
				if (mapKey) {
					map->Keys->Put(mapKey, mapKey);
				}
			}
		}
		for (Uint32 i = 0; i < numValues; i++) {
			Uint32 valueHash = StreamPtr->ReadUInt32();
			map->Values->Put(valueHash, ReadValue());
		}
		return;
	}
	default:
		StreamPtr->Skip(size);
		return;
	}
}

VMValue Serializer::ReadValue() {
	Uint8 type = StreamPtr->ReadByte();
	switch (type) {
	case Serializer::VAL_TYPE_INTEGER:
		return INTEGER_VAL((int)StreamPtr->ReadInt64());
	case Serializer::VAL_TYPE_DECIMAL:
		return DECIMAL_VAL(StreamPtr->ReadFloat());
	case Serializer::VAL_TYPE_NULL:
		break;
	case Serializer::VAL_TYPE_OBJECT: {
		Uint32 objectID = StreamPtr->ReadUInt32();
		if (objectID >= ObjList.size()) {
			Log::Print(Log::LOG_ERROR, "Attempted to read an invalid object ID!");
		}
		else if (ObjList[objectID] != nullptr) {
			return OBJECT_VAL(ObjList[objectID]);
		}
		break;
	}
	case Serializer::END:
		Log::Print(Log::LOG_ERROR, "Unexpected end of serialized data!");
		break;
	default:
		Log::Print(Log::LOG_ERROR, "Attempted to deserialize an invalid value type!");
		break;
	}
	return NULL_VAL;
}

bool Serializer::ReadObjectsChunk() {
	// Read the object count
	Uint32 count = StreamPtr->ReadUInt32();
	if (!count) {
		return StreamPtr->ReadByte() == Serializer::END;
	}

	// Read the objects, if there are any
	size_t objListPos = StreamPtr->Position();
	for (Uint32 i = 0; i < count; i++) {
		GetObject();
	}

	// Check for the end of chunk marker
	if (StreamPtr->ReadByte() != Serializer::END) {
		return false;
	}

	// Deserialize the objects (for real!)
	StreamPtr->Seek(objListPos);
	for (Uint32 i = 0; i < count; i++) {
		ReadObject(ObjList[i]);
	}

	// Check for the end of chunk marker (again!)
	return StreamPtr->ReadByte() == Serializer::END;
}

bool Serializer::ReadTextChunk() {
	// Read the count
	Uint32 count = StreamPtr->ReadUInt32();

	// Read the text, if there's any
	for (Uint32 i = 0; i < count; i++) {
		Serializer::String str;
		str.Length = StreamPtr->ReadUInt32();
		str.Chars = (char*)Memory::Calloc(str.Length, sizeof(char));
		if (str.Chars) {
			StreamPtr->ReadBytes(str.Chars, str.Length);
		}
		else {
			StreamPtr->Skip(str.Length);
		}
		StringList.push_back(str);
	}

	// Check for the end of chunk marker
	return StreamPtr->ReadByte() == Serializer::END;
}

VMValue Serializer::Retrieve() {
	Uint32 magic = StreamPtr->ReadUInt32();
	if (magic != Serializer::Magic) {
		Log::Print(Log::LOG_ERROR, "Invalid magic!");
		return NULL_VAL;
	}

	Uint32 version = StreamPtr->ReadUInt32();
	if (version != Serializer::Version) {
		Log::Print(Log::LOG_ERROR, "Invalid version!");
		return NULL_VAL;
	}

	// Read the pointer to the chunk list
	size_t chunkListPos = StreamPtr->ReadUInt32();

	// Store where we were before
	size_t startPos = StreamPtr->Position();

	// Seek to the chunk list, and read it
	StreamPtr->Seek(chunkListPos);

	Uint32 numChunks = StreamPtr->ReadUInt32();
	for (Uint32 i = 0; i < numChunks; i++) {
		Serializer::Chunk chunk;
		chunk.Type = StreamPtr->ReadUInt32BE();
		chunk.Offset = StreamPtr->ReadUInt32();
		chunk.Size = StreamPtr->ReadUInt32();
		ChunkList.push_back(chunk);
	}

	for (size_t i = 0; i < ChunkList.size(); i++) {
		Serializer::Chunk& chunk = ChunkList[i];

		Uint32 type = chunk.Type;
		Uint32 offset = chunk.Offset;

		Uint8* typeArr = (Uint8*)(&type);
		bool success = false;

		StreamPtr->Seek((size_t)offset);

		switch (type) {
		case Serializer::CHUNK_OBJS:
			success = Serializer::ReadObjectsChunk();
			break;
		case Serializer::CHUNK_TEXT:
			success = Serializer::ReadTextChunk();
			break;
		default:
			Log::Print(Log::LOG_WARN,
				"Skipping unknown chunk type %c%c%c%c",
				typeArr[3],
				typeArr[2],
				typeArr[1],
				typeArr[0]);
			break;
		}

		if (!success) {
			Log::Print(Log::LOG_ERROR,
				"Did not read end of chunk marker where it was expected to be!");
		}
	}

	// Seek back, so that we can read the value now
	StreamPtr->Seek(startPos);

	// Read the value
	VMValue returnValue = ReadValue();

	// Check for the EOF marker
	// (Although it doesn't really matter at this point, but it can
	// catch a malformed data stream)
	if (StreamPtr->ReadByte() != Serializer::END) {
		Log::Print(Log::LOG_ERROR,
			"Did not read end of file marker where it was expected to be!");
	}

	// Free all text strings
	for (size_t i = 0; i < StringList.size(); i++) {
		Memory::Free(StringList[i].Chars);
	}

	return returnValue;
}
