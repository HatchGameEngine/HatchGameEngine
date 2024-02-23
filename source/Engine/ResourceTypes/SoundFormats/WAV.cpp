#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>

#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class WAV : public SoundFormat {
private:
    // WAV Specific
    int DataStart = 0;
};
#endif

#include <Engine/ResourceTypes/SoundFormats/WAV.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>

struct RIFF_Header {
    Uint32        Magic;
    Uint32        ChunkSize;
    Uint32        Format;
};
struct fmt_Header {
    Uint32        Magic;
    Uint32        Size;
    Uint32        AudioFormat;
};
struct WAVheader {
    /* RIFF Chunk Descriptor */
    Uint8         RIFF[4];        // RIFF Header Magic header
    Uint32        ChunkSize;      // RIFF Chunk Size
    Uint8         WAVE[4];        // WAVE Header
    /* "fmt" sub-chunk */
    Uint8         FMT[4];
    Uint32        FMTSize;        // Size of the fmt chunk
    Uint16        AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
    Uint16        Channels;
    Uint32        Frequency;
    Uint32        BytesPerSecond;
    Uint16        BlockAlign;
    Uint16        BitsPerSample;
    /* "data" sub-chunk */
    Uint8         DATA[4]; // "data"  string
    Uint32        DATASize;  // Sampled data length
    Uint8         OverflowBuffer[64];
};

PUBLIC STATIC SoundFormat* WAV::Load(const char* filename) {
    WAV* wav = NULL;
    class Stream* stream = ResourceStream::New(filename);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Could not open file '%s'!", filename);
        goto WAV_Load_FAIL;
    }

    wav = new (std::nothrow) WAV;
    if (!wav) {
        goto WAV_Load_FAIL;
    }

    wav->StreamPtr = stream;

    WAVheader header;
    // RIFF Header
    stream->ReadBytes(&header, 0xC);
    // fmt Header
    stream->ReadBytes(&header.FMT, 0x8);
    stream->ReadBytes(&header.AudioFormat, header.FMTSize);
    // data Header
    stream->ReadBytes(&header.DATA, 0x08); // DATA

    memset(&wav->InputFormat, 0, sizeof(SDL_AudioSpec));
    wav->InputFormat.format    = (header.BitsPerSample & 0xFF);
    wav->InputFormat.channels  = header.Channels;
    wav->InputFormat.freq      = header.Frequency;
    wav->InputFormat.samples   = 4096;

    switch (header.AudioFormat) {
        case 0x0001:
            wav->InputFormat.format |= 0x8000;
            break;
        case 0x0003:
            wav->InputFormat.format |= 0x8000;
            // Allow only 32-bit floats
            if (header.BitsPerSample == 0x20)
                wav->InputFormat.format |= 0x0100;
            break;
    }

    wav->DataStart = (int)stream->Position();

    wav->TotalPossibleSamples = (int)(header.DATASize / (((header.BitsPerSample & 0xFF) >> 3) * header.Channels));
    // stream->Seek(wav->DataStart);

    // Common
    wav->LoadFinish();

    goto WAV_Load_SUCCESS;

    WAV_Load_FAIL:
    if (wav) {
        wav->Dispose();
        delete wav;
    }
    wav = NULL;

    WAV_Load_SUCCESS:
    return wav;
}

PUBLIC        int          WAV::LoadSamples(size_t count) {
    size_t read,
        bytesForSample = 0,
        total = 0;

    if (StreamPtr == nullptr)
        return 0;

    if (SampleBuffer == NULL) {
        SampleBuffer = (Uint8*)Memory::TrackedMalloc("SoundData::SampleBuffer", TotalPossibleSamples * SampleSize);
        Samples.reserve(TotalPossibleSamples);
    }

    char* buffer = (char*)SampleBuffer + Samples.size() * SampleSize;
    char* bufferStartSample = buffer;

    if ((size_t)count > TotalPossibleSamples - Samples.size())
        count = TotalPossibleSamples - Samples.size();

    StreamPtr->Seek(DataStart + Samples.size() * SampleSize);
    read = StreamPtr->ReadBytes(buffer, count * SampleSize);

    if (read == 0) return 0;

    bytesForSample = read;
    while (bytesForSample >= SampleSize) {
        Samples.push_back((Uint8*)bufferStartSample);
        bufferStartSample += SampleSize;
        bytesForSample -= SampleSize;
        total++;
    }

    return (int)total;
}

PUBLIC        void         WAV::Dispose() {
    // WAV specific clean up functions
    // Common cleanup
    SoundFormat::Dispose();
}
