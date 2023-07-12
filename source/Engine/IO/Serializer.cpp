#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>

#include <map>

class Serializer {
public:
    std::map<Obj*, Uint32> ObjToID;
    std::vector<Obj*>      ObjList;
    Stream*                StreamPtr;

    size_t                 StoredStreamPos;

    enum {
        VAL_TYPE_NULL,
        VAL_TYPE_INTEGER,
        VAL_TYPE_DECIMAL,
        VAL_TYPE_OBJECT,

        END = 0xFF
    };

    enum {
        OBJ_TYPE_STRING,
        OBJ_TYPE_ARRAY,
        OBJ_TYPE_MAP
    };
};
#endif

#include <Engine/IO/Serializer.h>
// #include <Engine/Bytecode/VMThread.h>
// #include <Engine/Bytecode/BytecodeObject.h>
// #include <Engine/Bytecode/BytecodeObjectManager.h>
// #include <Engine/Bytecode/Compiler.h>
// #include <Engine/Bytecode/Values.h>

PUBLIC Serializer::Serializer(Stream* stream) {
    StreamPtr = stream;
    ObjToID.clear();
    ObjList.clear();
}

PRIVATE void Serializer::WriteValue(VMValue val) {
    switch (val.Type) {
        case VAL_DECIMAL:
        case VAL_LINKED_DECIMAL: {
            float d = AS_DECIMAL(val);
            StreamPtr->WriteByte(Serializer::VAL_TYPE_DECIMAL);
            StreamPtr->WriteFloat(d);
            break;
        }
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER: {
            int i = AS_INTEGER(val);
            StreamPtr->WriteByte(Serializer::VAL_TYPE_INTEGER);
            StreamPtr->WriteInt64(i);
            break;
        }
        case VAL_OBJECT: {
            Obj* obj = AS_OBJECT(val);

            Uint32 objectID = GetUniqueObjectID(obj);
            if (objectID == 0xFFFFFFFF) {
                StreamPtr->WriteByte(Serializer::VAL_TYPE_NULL);
                break;
            }

            switch (obj->Type) {
                case OBJ_STRING:
                case OBJ_ARRAY:
                case OBJ_MAP:
                    StreamPtr->WriteByte(Serializer::VAL_TYPE_OBJECT);
                    StreamPtr->WriteUInt32(objectID);
                    break;
                default:
                    StreamPtr->WriteByte(Serializer::VAL_TYPE_NULL);
                    break;
            }
        }
        default:
            StreamPtr->WriteByte(Serializer::VAL_TYPE_NULL);
            break;
    }
}

PRIVATE void Serializer::WriteObject(Obj* obj) {
    switch (obj->Type) {
        case OBJ_STRING: {
            WriteObjectPreamble(Serializer::OBJ_TYPE_STRING);

            ObjString* string = (ObjString*)obj;
            StreamPtr->WriteUInt32(string->Length);
            StreamPtr->WriteBytes(string->Chars, string->Length);

            PatchObjectSize();
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

            PatchObjectSize();
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
                StreamPtr->WriteString(ptr);
            });

            map->Values->WithAll([this](Uint32 hash, VMValue mapVal) -> void {
                StreamPtr->WriteUInt32(hash);
                WriteValue(mapVal);
            });

            PatchObjectSize();
            break;
        }
        default:
            Log::Print(Log::LOG_ERROR, "Cannot serialize an object of type %s!", GetTypeString(OBJECT_VAL(obj)));
            break;
    }
}

PRIVATE Uint32 Serializer::GetUniqueObjectID(Obj* obj) {
    if (ObjToID.count(obj))
        return ObjToID[obj];

    return 0xFFFFFFFF;
}

PRIVATE void Serializer::WriteObjectPreamble(Uint8 type) {
    StreamPtr->WriteByte(type);
    StreamPtr->WriteUInt32(0);
    StoredStreamPos = StreamPtr->Position();
}

PRIVATE void Serializer::PatchObjectSize() {
    size_t curPos = StreamPtr->Position();
    size_t size = curPos - StoredStreamPos;
    StreamPtr->Seek(StoredStreamPos - 4);
    StreamPtr->WriteUInt32(size);
    StreamPtr->Seek(curPos);
}

PRIVATE void Serializer::AddUniqueObject(Obj* obj) {
    if (ObjToID.count(obj) || ObjList.size() >= 0xFFFFFFFF)
        return;

    Uint32 id = ObjList.size();
    ObjToID[obj] = id;
    ObjList.push_back(obj);

    switch (obj->Type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)obj;
            for (size_t i = 0; i < array->Values->size(); i++) {
                VMValue arrayVal = (*array->Values)[i];
                if (IS_OBJECT(arrayVal))
                    AddUniqueObject(AS_OBJECT(arrayVal));
            }
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)obj;
            map->Values->WithAll([this](Uint32, VMValue mapVal) -> void {
                if (IS_OBJECT(mapVal))
                    AddUniqueObject(AS_OBJECT(mapVal));
            });
            break;
        }
        default:
            return;
    }
}

PUBLIC void Serializer::Store(VMValue val) {
    // Write header
    StreamPtr->WriteUInt32(0x9D939FF0);
    StreamPtr->WriteUInt32(0);

    // We're gonna patch this later.
    size_t addrPos = StreamPtr->Position();
    StreamPtr->WriteUInt32(0);

    // See if we can add this value as an object
    if (IS_OBJECT(val))
        AddUniqueObject(AS_OBJECT(val));

    // Write the value
    WriteValue(val);

    // End marker
    StreamPtr->WriteByte(Serializer::END);

    // Write the object count
    size_t dirPos = StreamPtr->Position();
    StreamPtr->WriteUInt32(ObjList.size());

    // Serialize all of the objects now
    for (size_t i = 0; i < ObjList.size(); i++)
        WriteObject(ObjList[i]);

    // Write a pointer to the object directory
    size_t curPos = StreamPtr->Position();
    StreamPtr->Seek(addrPos);
    StreamPtr->WriteUInt32(dirPos);
    StreamPtr->Seek(curPos);

    // Done here
    StreamPtr->WriteByte(Serializer::END);
    ObjToID.clear();
    ObjList.clear();
}

PRIVATE void Serializer::GetObject() {
    Uint8 type = StreamPtr->ReadByte();
    Uint32 size = StreamPtr->ReadUInt32();
    switch (type) {
    case Serializer::OBJ_TYPE_STRING: {
        size_t sz = StreamPtr->ReadUInt32();
        ObjString* string = AllocString(sz);
        ObjList.push_back((Obj*)string);
        StreamPtr->ReadBytes(string->Chars, sz);
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
        Log::Print(Log::LOG_ERROR, "Attempted to deserialize an invalid object type!");
        ObjList.push_back(nullptr);
        return;
    }
}

PRIVATE void Serializer::ReadObject(Obj* obj) {
    Uint8 type = StreamPtr->ReadByte();
    Uint32 size = StreamPtr->ReadUInt32();
    switch (type) {
    case Serializer::OBJ_TYPE_STRING: {
        // Nothing to do here
        StreamPtr->Skip(size);
        break;
    }
    case Serializer::OBJ_TYPE_ARRAY: {
        size_t sz = StreamPtr->ReadUInt32();
        ObjArray* array = (ObjArray*)obj;
        for (size_t i = 0; i < sz; i++)
            array->Values->push_back(ReadValue());
        return;
    }
    case Serializer::OBJ_TYPE_MAP: {
        size_t numKeys = StreamPtr->ReadUInt32();
        size_t numValues = StreamPtr->ReadUInt32();
        ObjMap* map = (ObjMap*)obj;
        for (size_t i = 0; i < numKeys; i++) {
            char* mapKey = StreamPtr->ReadString();
            map->Keys->Put(mapKey, HeapCopyString(mapKey, strlen(mapKey)));
        }
        for (size_t i = 0; i < numValues; i++) {
            Uint32 valueHash = StreamPtr->ReadUInt32();
            map->Values->Put(valueHash, ReadValue());
        }
        return;
    }
    default:
        return;
    }
}

PRIVATE VMValue Serializer::ReadValue() {
    VMValue returnValue = NULL_VAL;
    Uint8 type = StreamPtr->ReadByte();
    switch (type) {
    case Serializer::VAL_TYPE_INTEGER:
        returnValue = INTEGER_VAL((int)StreamPtr->ReadInt64());
        break;
    case Serializer::VAL_TYPE_DECIMAL:
        returnValue = DECIMAL_VAL(StreamPtr->ReadFloat());
        break;
    case Serializer::VAL_TYPE_NULL:
        break;
    case Serializer::VAL_TYPE_OBJECT: {
        Uint32 objectID = StreamPtr->ReadUInt32();
        if (objectID >= ObjList.size())
            Log::Print(Log::LOG_ERROR, "Attempted to read an invalid object ID!");
        else if (ObjList[objectID] != nullptr)
            returnValue = OBJECT_VAL(ObjList[objectID]);
        break;
    }
    case Serializer::END:
        Log::Print(Log::LOG_ERROR, "Unexpected end of serialized data!");
        break;
    default:
        Log::Print(Log::LOG_ERROR, "Attempted to deserialize an invalid value type!");
        break;
    }
    return returnValue;
}

PUBLIC VMValue Serializer::Retrieve() {
    Uint32 magic = StreamPtr->ReadUInt32();
    if (magic != 0x9D939FF0) {
        Log::Print(Log::LOG_ERROR, "Invalid magic!");
        return NULL_VAL;
    }

    Uint32 version = StreamPtr->ReadUInt32();

    VMValue returnValue = NULL_VAL;

    // Read the pointer to the object directory
    size_t dirPos = StreamPtr->ReadUInt32();

    // Store where we were before
    size_t startPos = StreamPtr->Position();

    // Seek to the object directory, and read it
    StreamPtr->Seek(dirPos);

    // Read the object count
    Uint32 count = StreamPtr->ReadUInt32();

    // Read the objects, if there are any
    size_t objListPos = StreamPtr->Position();
    for (Uint32 i = 0; i < count; i++)
        GetObject();

#define CHECK_MARKER \
    if (StreamPtr->ReadByte() != Serializer::END) { \
        Log::Print(Log::LOG_ERROR, "Did not read marker where it was expected to be!"); \
        return returnValue; \
    }

    CHECK_MARKER

    // Deserialize the objects (for real!)
    if (count) {
        StreamPtr->Seek(objListPos);
        for (Uint32 i = 0; i < count; i++)
            ReadObject(ObjList[i]);

        CHECK_MARKER
    }

    // Seek back, so that we can read the values now
    StreamPtr->Seek(startPos);

    // Read the value
    returnValue = ReadValue();

    CHECK_MARKER

    return returnValue;
}
