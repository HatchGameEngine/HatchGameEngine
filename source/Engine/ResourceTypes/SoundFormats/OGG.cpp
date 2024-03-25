#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>

#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class OGG : public SoundFormat {
private:
    // OGG Specific
    char           Vorbis[0x800];
};
#endif

#include <Engine/Application.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/SoundFormats/OGG.h>

#ifdef USING_LIBOGG
#define OV_EXCLUDE_STATIC_CALLBACKS
#if defined(OGG_HEADER)
    #include OGG_HEADER
#elif defined(OGG_USE_TREMOR)
    #include <tremor/ivorbisfile.h>
#else
    #include <vorbis/vorbisfile.h>
#endif
#else
#include <Libraries/stb_vorbis.h>
#endif


#ifdef USING_LIBOGG
struct VorbisGroup {
	OggVorbis_File File;
	vorbis_info*   Info = NULL;
	int            Bitstream;
};
#else
struct VorbisGroup {
    stb_vorbis* VorbisSTB;
    void* FileBlock;
};
#endif

PRIVATE STATIC size_t      OGG::StaticRead(void* mem, size_t size, size_t nmemb, void* ptr) {
	class Stream* stream = (class Stream*)ptr;
	return stream->ReadBytes(mem, size * nmemb);
}
PRIVATE STATIC Sint32      OGG::StaticSeek(void* ptr, Sint64 offset, int whence) {
	class Stream* stream = (class Stream*)ptr;
    if (whence == SEEK_CUR) {
		stream->Skip((Sint64)offset);
        return 0;
    }
    else if (whence == SEEK_SET) {
		stream->Seek((Sint64)offset);
        return 0;
    }
    else if (whence == SEEK_END) {
		stream->SeekEnd((Sint64)offset);
        return 0;
    }
    else {
        printf("OGG::StaticSeek: %d\n", whence);
        return -1;
    }
}
PRIVATE STATIC Sint32      OGG::StaticCloseFree(void* ptr) {
    return 0;
}
PRIVATE STATIC Sint32      OGG::StaticCloseNoFree(void* ptr) {
    return 0;
}
PRIVATE STATIC long        OGG::StaticTell(void* ptr) {
    return ((class Stream*)ptr)->Position();
}

PUBLIC STATIC SoundFormat* OGG::Load(const char* filename) {
    VorbisGroup* vorbis;

    OGG* ogg = NULL;
    class Stream* stream = ResourceStream::New(filename);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Could not open file '%s'!", filename);
        goto OGG_Load_FAIL;
    }

#ifdef USING_LIBOGG
    ogg = new (std::nothrow) OGG;
    if (!ogg) {
        goto OGG_Load_FAIL;
    }

    ogg->StreamPtr = stream;

    ov_callbacks callbacks;
	// callbacks = OV_CALLBACKS_STREAMONLY;
	// callbacks = OV_CALLBACKS_STREAMONLY_NOCLOSE;
	// callbacks = OV_CALLBACKS_DEFAULT;
	// callbacks = OV_CALLBACKS_NOCLOSE;

    callbacks.read_func = OGG::StaticRead;
    callbacks.seek_func = OGG::StaticSeek;
    callbacks.tell_func = OGG::StaticTell;
    callbacks.close_func = OGG::StaticCloseNoFree;

    vorbis = (VorbisGroup*)ogg->Vorbis;

    memset(&vorbis->File, 0, sizeof(OggVorbis_File));

    int res;
    if ((res = ov_open_callbacks(ogg->StreamPtr, &vorbis->File, NULL, 0, callbacks)) != 0) {
        Log::Print(Log::LOG_ERROR, "Could not read file '%s'!", filename);
        switch (res) {
            case OV_EREAD:
                Log::Print(Log::LOG_ERROR, "A read from media returned an error.");
                break;
            case OV_ENOTVORBIS:
                Log::Print(Log::LOG_ERROR, "Bitstream does not contain any Vorbis data.");
                break;
            case OV_EVERSION:
                Log::Print(Log::LOG_ERROR, "Vorbis version mismatch.");
                break;
            case OV_EBADHEADER:
                Log::Print(Log::LOG_ERROR, "Invalid Vorbis bitstream header.");
                break;
            case OV_EFAULT:
                Log::Print(Log::LOG_ERROR, "Internal logic fault; indicates a bug or heap/stack corruption.");
                break;
            default:
                Log::Print(Log::LOG_ERROR, "Resource is not valid Vorbis stream!");
                break;
        }
        goto OGG_Load_FAIL;
    }

    vorbis->Info = ov_info(&vorbis->File, -1);

    memset(&ogg->InputFormat, 0, sizeof(SDL_AudioSpec));
    ogg->InputFormat.format    = AUDIO_S16;
    ogg->InputFormat.channels  = vorbis->Info->channels;
    ogg->InputFormat.freq      = (int)vorbis->Info->rate;
    ogg->InputFormat.samples   = 4096;

    ogg->TotalPossibleSamples = (int)ov_pcm_total(&vorbis->File, -1);

    // Common
    ogg->LoadFinish();

    goto OGG_Load_SUCCESS;
#else

    ogg = new (std::nothrow) OGG;
    if (!ogg) {
        goto OGG_Load_FAIL;
    }

    {
        vorbis = (VorbisGroup*)ogg->Vorbis;

        size_t fileLength = stream->Length();
        void* fileData = malloc(fileLength);
        stream->ReadBytes(fileData, fileLength);

        vorbis->FileBlock = fileData;

        int error;
        vorbis->VorbisSTB = stb_vorbis_open_memory((Uint8*)vorbis->FileBlock, fileLength, &error, NULL);
        if (!vorbis->VorbisSTB) {
            Log::Print(Log::LOG_ERROR, "Could not open Vorbis stream for %s!", filename);

            switch (error) {
            case VORBIS_need_more_data:
                Log::Print(Log::LOG_ERROR, "VORBIS_need_more_data");
                break;
            case VORBIS_invalid_api_mixing:
                Log::Print(Log::LOG_ERROR, "VORBIS_invalid_api_mixing");
                break;
            case VORBIS_outofmem:
                Log::Print(Log::LOG_ERROR, "VORBIS_outofmem");
                break;
            case VORBIS_feature_not_supported:
                Log::Print(Log::LOG_ERROR, "VORBIS_feature_not_supported");
                break;
            case VORBIS_too_many_channels:
                Log::Print(Log::LOG_ERROR, "VORBIS_too_many_channels");
                break;
            case VORBIS_file_open_failure:
                Log::Print(Log::LOG_ERROR, "VORBIS_file_open_failure");
                break;
            case VORBIS_seek_without_length:
                Log::Print(Log::LOG_ERROR, "VORBIS_seek_without_length");
                break;
            case VORBIS_unexpected_eof:
                Log::Print(Log::LOG_ERROR, "VORBIS_unexpected_eof");
                break;
            case VORBIS_seek_invalid:
                Log::Print(Log::LOG_ERROR, "VORBIS_seek_invalid");
                break;
            case VORBIS_invalid_setup:
                Log::Print(Log::LOG_ERROR, "VORBIS_invalid_setup");
                break;
            case VORBIS_invalid_stream:
                Log::Print(Log::LOG_ERROR, "VORBIS_invalid_stream");
                break;
            case VORBIS_missing_capture_pattern:
                Log::Print(Log::LOG_ERROR, "VORBIS_missing_capture_pattern");
                break;
            case VORBIS_invalid_stream_structure_version:
                Log::Print(Log::LOG_ERROR, "VORBIS_invalid_stream_structure_version");
                break;
            case VORBIS_continued_packet_flag_invalid:
                Log::Print(Log::LOG_ERROR, "VORBIS_continued_packet_flag_invalid");
                break;
            case VORBIS_incorrect_stream_serial_number:
                Log::Print(Log::LOG_ERROR, "VORBIS_incorrect_stream_serial_number");
                break;
            case VORBIS_invalid_first_page:
                Log::Print(Log::LOG_ERROR, "VORBIS_invalid_first_page");
                break;
            case VORBIS_bad_packet_type:
                Log::Print(Log::LOG_ERROR, "VORBIS_bad_packet_type");
                break;
            case VORBIS_cant_find_last_page:
                Log::Print(Log::LOG_ERROR, "VORBIS_cant_find_last_page");
                break;
            case VORBIS_seek_failed:
                Log::Print(Log::LOG_ERROR, "VORBIS_seek_failed");
                break;
            case VORBIS_ogg_skeleton_not_supported:
                Log::Print(Log::LOG_ERROR, "VORBIS_ogg_skeleton_not_supported");
                break;
            }

            goto OGG_Load_FAIL;
        }

        auto info = stb_vorbis_get_info(vorbis->VorbisSTB);

        memset(&ogg->InputFormat, 0, sizeof(SDL_AudioSpec));
        ogg->InputFormat.format = AUDIO_S16;
        ogg->InputFormat.channels = info.channels;
        ogg->InputFormat.freq = (int)info.sample_rate;
        ogg->InputFormat.samples = 4096;
    }

    ogg->TotalPossibleSamples = (int)stb_vorbis_stream_length_in_samples(vorbis->VorbisSTB);

    // Common
    ogg->LoadFinish();

    goto OGG_Load_SUCCESS;
#endif

    OGG_Load_FAIL:
    if (ogg) {
        ogg->Dispose();
        delete ogg;
    }
    ogg = NULL;

OGG_Load_SUCCESS:
    if (stream)
        stream->Close();
    return ogg;
}

PUBLIC        size_t       OGG::SeekSample(int index) {
    VorbisGroup* vorbis = (VorbisGroup*)this->Vorbis;
#ifdef USING_LIBOGG
    ov_pcm_seek(&vorbis->File, index);
#else
    stb_vorbis_seek(vorbis->VorbisSTB, index);
#endif

    SampleIndex = (size_t)index;
    return SampleIndex;
}
PUBLIC        int          OGG::LoadSamples(size_t count) {
    if (SampleBuffer == NULL) {
        SampleBuffer = (Uint8*)Memory::TrackedMalloc("SoundData::SampleBuffer", TotalPossibleSamples * SampleSize);
        Samples.reserve(TotalPossibleSamples);
    }

    return GetSamples(SampleBuffer, count, -1);
}
PUBLIC        int          OGG::GetSamples(Uint8* buffer, size_t count, Sint32 loopIndex) {
#ifdef USING_LIBOGG
    int read;
    Uint32 total = 0,
           bytesForSample = 0;

    if (count > TotalPossibleSamples - Samples.size())
        count = TotalPossibleSamples - Samples.size();

    size_t remainingBytes = count * SampleSize;
    size_t sampleSizeForOneChannel = SampleSize / InputFormat.channels;

    VorbisGroup* vorbis = (VorbisGroup*)this->Vorbis;

    while (remainingBytes && (read =
		#ifdef OGG_USE_TREMOR
	    ov_read(&vorbis->File, (char*)buffer, remainingBytes, &vorbis->Bitstream)
		#else
		ov_read(&vorbis->File, (char*)buffer, remainingBytes, 0, sampleSizeForOneChannel, 1, &vorbis->Bitstream)
		#endif
		)) {
        #ifdef DEBUG
            if (read == OV_HOLE)
                return 0;
            if (read == OV_EBADLINK)
                return 0;
            if (read == OV_EINVAL)
                return 0;
        #endif

            // Reached end of file, what should be done?
            if (read == 0) {
                // If we want to loop, seek to loop point and continue reading.
                if (loopIndex >= 0) {
                    SeekSample(loopIndex);
                    continue;
                }

                // Otherwise, done reading.
                break;
            }

        remainingBytes -= read;
        buffer += read;
        total += read;
    }
    total /= SampleSize;

    SampleIndex += total;
    return total;
#else
    int read;
    Uint32 total = 0,
        bytesForSample = 0;

    if (count > TotalPossibleSamples - Samples.size())
        count = TotalPossibleSamples - Samples.size();

    size_t remainingBytes = count * SampleSize;
    size_t sampleSizeForOneChannel = SampleSize / InputFormat.channels;

    VorbisGroup* vorbis = (VorbisGroup*)this->Vorbis;

    while (remainingBytes && (read =
        stb_vorbis_get_samples_short_interleaved(vorbis->VorbisSTB, 2, (short*)buffer, remainingBytes / sizeof(short)) * sizeof(short) * 2
        )) {
        if (read < 0)
            return 0;

        // Reached end of file, what should be done?
        if (read == 0) {
            // If we want to loop, seek to loop point and continue reading.
            if (loopIndex >= 0) {
                SeekSample(loopIndex);
                continue;
            }

            // Otherwise, done reading.
            break;
        }

        remainingBytes -= read;
        buffer += read;
        total += read;
    }
    total /= SampleSize;

    SampleIndex += total;
    return total;
#endif
    return 0;
}
PUBLIC VIRTUAL void        OGG::LoadAllSamples() {
    if (SampleBuffer == nullptr) {
        SampleBuffer = (Uint8*)Memory::TrackedMalloc("SoundData::SampleBuffer", TotalPossibleSamples * SampleSize);
        Samples.reserve(TotalPossibleSamples);
    }

    if (Samples.size() < TotalPossibleSamples) {
        Samples.clear();

        size_t lastSample = TellSample();
        SeekSample(0);
        size_t readSamples = LoadSamples(TotalPossibleSamples);
        SeekSample(lastSample);

        Uint8* buffer = SampleBuffer;
        while (readSamples > 0) {
            Samples.push_back(buffer);
            buffer += SampleSize;
            readSamples--;
        }
    }
}

PUBLIC        void         OGG::Dispose() {
#ifdef USING_LIBOGG
	// OGG specific clean up functions
	VorbisGroup* vorbis = (VorbisGroup*)this->Vorbis;
	ov_clear(&vorbis->File);
#else
    VorbisGroup* vorbis = (VorbisGroup*)this->Vorbis;
    if (vorbis->FileBlock) {
        free(vorbis->FileBlock);
    }
    stb_vorbis_close(vorbis->VorbisSTB);
#endif
    // Common cleanup
    SoundFormat::Dispose();
}
