#include <Engine/Network/HTTP.h>

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>

#ifdef USING_CURL

#if !WIN32
#include <sys/fcntl.h>
#include <sys/select.h>
#endif
#include <curl/curl.h>

CURL* curl = NULL;

struct CurlData {
	Uint8* Ptr;
	size_t Length;
};
size_t CurlWriteFunction(void* ptr, size_t size, size_t n, CurlData* data) {
	size_t len = data->Length + size * n;

	data->Ptr = (Uint8*)realloc(data->Ptr, len + 1);
	if (data->Ptr == NULL) {
		exit(EXIT_FAILURE);
	}

	memcpy(data->Ptr + data->Length, ptr, size * n);
	data->Ptr[len] = 0;
	data->Length = len;

	return size * n;
}
CurlData* CurlGET(CURL* curl, const char* url) {
	if (!curl) {
		return NULL;
	}

	CurlData* data = (CurlData*)malloc(sizeof(CurlData));
	if (!data) {
		return NULL;
	}

	data->Length = 0;
	data->Ptr = (Uint8*)calloc(1, 1);
	if (!data->Ptr) {
		free(data);
		return NULL;
	}

	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	// curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunction);

	CURLcode res = curl_easy_perform(curl);
	if (res != 0) {
		Log::Print(Log::LOG_ERROR,
			"curl_easy_perform failed with code: %d, on URL: %s",
			res,
			url);
	}

	return data;
}

struct _GET_Bundle {
	char* URL;
	ObjBoundMethod Callback;
};
int _GET_FromThread(void* op) {
	_GET_Bundle* bundle = (_GET_Bundle*)op;

	CurlData* data = CurlGET(curl, bundle->URL);
	if (!data) {
		return false;
	}

	Uint8* ptr = data->Ptr;
	size_t length = data->Length;

	if (ScriptManager::Lock()) {
		// ObjString* string = TakeString((char*)ptr, length);
		// VMValue    callbackVal =
		// OBJECT_VAL(&bundle->Callback);
		// // if (bundle->Callback.Function->Arity != 1)
		// // ScriptManager::ThrowError

		// Push(callbackVal);
		// Push(OBJECT_VAL(string));
		// CallValue(callbackVal, 1);
		// while (Run_Instruction() >= 0);

		ScriptManager::Unlock();
	}

	free(data);
	free(bundle);
	return 0;
}

#endif

bool HTTP::GET(const char* url, Uint8** outBuf, size_t* outLen, ObjBoundMethod* callback) {
#ifdef USING_CURL

	if (!curl) {
		curl = curl_easy_init();
		if (!curl) {
			return false;
		}
	}

	if (callback && false) {
		// NOTE: Mutex lock/unlock while using ScriptManager
		// from other thread. if (callback->Function->Arity !=
		// 1) ScriptManager::ThrowError
		_GET_Bundle* bundle = (_GET_Bundle*)malloc(sizeof(_GET_Bundle) + strlen(url) + 1);
		bundle->URL = (char*)(bundle + 1);
		bundle->Callback = *callback;
		bundle->Callback.Object.Next = NULL;
		strcpy(bundle->URL, url);
		SDL_CreateThread(_GET_FromThread, "_GET_FromThread", bundle);
		return false;
	}

	CurlData* data = CurlGET(curl, url);
	if (!data) {
		return false;
	}

	*outBuf = data->Ptr;
	*outLen = data->Length;
	free(data);
	return true;

#endif

	return false;
}
