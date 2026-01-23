#include <Engine/Application.h>
#include <Engine/Extensions/Discord.h>

bool Discord::Initialized = false;

#if defined(WIN32) || defined(LINUX)
#include <Engine/Bytecode/ScriptManager.h>

#include <Libraries/discord_game_sdk.h>

#ifdef WIN32
#define DISCORD_LIBRARY_NAME "discord_game_sdk.dll"
#else
#define DISCORD_LIBRARY_NAME "discord_game_sdk.so"
#endif

typedef EDiscordResult(DISCORD_API* DiscordCreate_t)(DiscordVersion version,
	DiscordCreateParams* params,
	IDiscordCore** result);

void* library = NULL;
DiscordCreate_t _DiscordCreate = NULL;

IDiscordCore* Core = NULL;
IDiscordActivityManager* ActivityManager = NULL;
IDiscordUserManager* UserManager = NULL;
IDiscordImageManager* ImageManager = NULL;

IDiscordUserEvents UserEvents;

DiscordActivity CurrentActivity;
DiscordIntegrationUserInfo CurrentUser;
DiscordIntegrationUserAvatar UserAvatar;

struct AvatarFetchCallbackData {
	char Identifier[256];
	DiscordIntegrationUserAvatar* Avatar;
	DiscordIntegrationCallback* Callback;
};

void DiscordLogHook(void* hook_data, EDiscordLogLevel level, const char* message) {
	Log::Print(Log::LOG_API, "Discord: %s", message);
}

int DiscordResultToHatchEnum(EDiscordResult result) {
	switch (result) {
	case DiscordResult_Ok:
		return DISCORDRESULT_OK;
	case DiscordResult_ServiceUnavailable:
		return DISCORDRESULT_SERVICEUNAVAILABLE;
	case DiscordResult_InvalidVersion:
		return DISCORDRESULT_INVALIDVERSION;
	case DiscordResult_InvalidPayload:
		return DISCORDRESULT_INVALIDPAYLOAD;
	case DiscordResult_InvalidPermissions:
		return DISCORDRESULT_INVALIDPERMISSIONS;
	case DiscordResult_NotFound:
		return DISCORDRESULT_NOTFOUND;
	case DiscordResult_NotAuthenticated:
		return DISCORDRESULT_NOTAUTHENTICATED;
	case DiscordResult_NotInstalled:
		return DISCORDRESULT_NOTINSTALLED;
	case DiscordResult_NotRunning:
		return DISCORDRESULT_NOTRUNNING;
	case DiscordResult_RateLimited:
		return DISCORDRESULT_RATELIMITED;
	default:
		return DISCORDRESULT_ERROR;
	}
}

void DISCORD_CALLBACK OnUpdateActivityCallback(void* callback_data, EDiscordResult result) {
	if (result == DiscordResult_Ok) {
		Log::Print(Log::LOG_API, "Discord: Presence updated successfully.");
	}
	else {
		Log::Print(
			Log::LOG_API, "Discord: Failed to update presence (Error %d)", (int)result);
	}
}

void DISCORD_CALLBACK OnCurrentUserUpdateCallback(void* callback_data) {
	Discord::User::Update();
}

int CreateImageResource(DiscordIntegrationUserAvatar* avatar) {
	Texture* texture = Graphics::CreateTextureFromPixels(
		avatar->Width, avatar->Height, avatar->Data, avatar->Width * sizeof(Uint32));
	if (texture == nullptr) {
		return -1;
	}

	Image* image = new Image(texture);

	return Scene::AddImageResource(image, avatar->Identifier, SCOPE_GAME);
}

void ExecuteImageFetchCallback(DiscordIntegrationCallback* callback,
	DiscordIntegrationUserAvatar* avatar,
	EDiscordResult result) {
	if (!callback) {
		return;
	}

	switch (callback->Type) {
	case DiscordIntegrationCallbackType_FuncPtr: {
		DiscordIntegrationCallbackFuncPtr funcPtr =
			(DiscordIntegrationCallbackFuncPtr)callback->Function;
		funcPtr(avatar);
		break;
	}
	case DiscordIntegrationCallbackType_Script: {
		VMThreadCallback* scriptCallback = (VMThreadCallback*)callback->Function;

		VMThread* thread = ScriptManager::Threads + scriptCallback->ThreadID;
		VMValue* stackTop = thread->StackTop;

		VMValue value = NULL_VAL;
		if (avatar) {
			int imageIndex = CreateImageResource(avatar);
			if (imageIndex != -1) {
				value = INTEGER_VAL(imageIndex);
			}
		}

		thread->Push(INTEGER_VAL(DiscordResultToHatchEnum(result)));
		thread->Push(value);
		thread->RunValue(scriptCallback->Callable, 2);
		thread->StackTop = stackTop;
		break;
	}
	}
}

void FreeImageFetchCallbackData(AvatarFetchCallbackData* fetchData) {
	DiscordIntegrationCallback* callback = fetchData->Callback;
	if (callback) {
		if (callback->Type == DiscordIntegrationCallbackType_Script) {
			VMThreadCallback* scriptCallback = (VMThreadCallback*)callback->Function;
			Memory::Free(scriptCallback);
		}

		Memory::Free(callback);
	}

	Memory::Free(fetchData);
}

void DISCORD_CALLBACK OnImageFetchCallback(void* callback_data,
	EDiscordResult result,
	DiscordImageHandle handle_result) {
	DiscordIntegrationUserAvatar* avatar = nullptr;
	DiscordImageDimensions dimensions;
	size_t data_length;
	Uint8* data;

	AvatarFetchCallbackData* fetchData = (AvatarFetchCallbackData*)callback_data;
	if (fetchData == nullptr) {
		return;
	}

	DiscordIntegrationCallback* callback = fetchData->Callback;
	if (result != DiscordResult_Ok) {
		goto EXECUTE_CALLBACK;
	}

	result = ImageManager->get_dimensions(ImageManager, handle_result, &dimensions);
	if (result != DiscordResult_Ok) {
		goto EXECUTE_CALLBACK;
	}

	data_length = dimensions.width * dimensions.height * sizeof(Uint32);
	data = (Uint8*)Memory::Malloc(data_length);
	if (data == nullptr) {
		goto EXECUTE_CALLBACK;
	}

	result = ImageManager->get_data(ImageManager, handle_result, data, data_length);
	if (result != DiscordResult_Ok) {
		Memory::Free(data);
		goto EXECUTE_CALLBACK;
	}

	avatar = fetchData->Avatar;

	if (avatar) {
		Memory::Free(avatar->Data);

		avatar->Width = dimensions.width;
		avatar->Height = dimensions.height;
		avatar->Data = data;

		StringUtils::Copy(
			avatar->Identifier, fetchData->Identifier, sizeof(avatar->Identifier) - 1);
	}

EXECUTE_CALLBACK:
	ExecuteImageFetchCallback(callback, avatar, result);
	FreeImageFetchCallbackData(fetchData);
}

int Discord::Init(const char* applicationID) {
	if (Discord::Initialized) {
		return DISCORDRESULT_OK;
	}
	Discord::Initialized = false;

	if (!library) {
		library = SDL_LoadObject(DISCORD_LIBRARY_NAME);
		if (!library) {
			Log::Print(Log::LOG_API,
				"Discord: Failed to load %s! (SDL Error: %s)",
				DISCORD_LIBRARY_NAME,
				SDL_GetError());
			return DISCORDRESULT_ERROR;
		}
	}

	if (!_DiscordCreate) {
		_DiscordCreate = (DiscordCreate_t)SDL_LoadFunction(library, "DiscordCreate");
		if (!_DiscordCreate) {
			Log::Print(Log::LOG_API,
				"Discord: Failed to find function 'DiscordCreate' in %s!",
				DISCORD_LIBRARY_NAME);
			Discord::Unload();
			return DISCORDRESULT_ERROR;
		}
	}

	memset(&UserEvents, 0, sizeof(UserEvents));
	UserEvents.on_current_user_update = OnCurrentUserUpdateCallback;

	DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);
	params.client_id = strtoll(applicationID, NULL, 10);
	params.flags = DiscordCreateFlags_NoRequireDiscord;
	params.user_events = &UserEvents;

	if (Core) {
		Core->destroy(Core);
		Core = NULL;
	}

	EDiscordResult result = _DiscordCreate(DISCORD_VERSION, &params, &Core);
	if (result != DiscordResult_Ok) {
		if (result == DiscordResult_InternalError || result == DiscordResult_NotRunning) {
			Log::Print(Log::LOG_API,
				"Discord: Integration disabled (Discord client not found).");
		}
		else {
			Log::Print(Log::LOG_API,
				"Discord: Initialization failed with unexpected error code %d",
				(int)result);
		}

		Discord::Unload();
		return DiscordResultToHatchEnum(result);
	}

	Core->set_log_hook(Core, DiscordLogLevel_Error, nullptr, DiscordLogHook);

	ActivityManager = Core->get_activity_manager(Core);
	UserManager = Core->get_user_manager(Core);
	ImageManager = Core->get_image_manager(Core);

	Discord::Initialized = true;
	Log::Print(Log::LOG_API,
		"Discord: SDK initialized successfully for Application ID: %s",
		applicationID);

	memset(&CurrentActivity, 0, sizeof(CurrentActivity));
	memset(&CurrentUser, 0, sizeof(CurrentUser));

	CurrentActivity.type = DiscordActivityType_Playing;

	return DiscordResultToHatchEnum(result);
}

void Discord::Update() {
	if (!Discord::Initialized) {
		return;
	}

	Core->run_callbacks(Core);
}

void Discord::Activity::SetDetails(const char* details) {
	if (details && strlen(details) > 0) {
		StringUtils::Copy(
			CurrentActivity.details, details, sizeof(CurrentActivity.details) - 1);
	}
	else {
		memset(CurrentActivity.details, 0, sizeof(CurrentActivity.details) - 1);
	}
}
void Discord::Activity::SetState(const char* state) {
	if (state && strlen(state) > 0) {
		StringUtils::Copy(CurrentActivity.state, state, sizeof(CurrentActivity.state) - 1);
	}
	else {
		memset(CurrentActivity.state, 0, sizeof(CurrentActivity.state) - 1);
	}
}
void Discord::Activity::SetLargeImageKey(const char* key) {
	if (key && strlen(key) > 0) {
		StringUtils::Copy(CurrentActivity.assets.large_image,
			key,
			sizeof(CurrentActivity.assets.large_image) - 1);
	}
	else {
		memset(CurrentActivity.assets.large_image,
			0,
			sizeof(CurrentActivity.assets.large_image) - 1);
	}
}
void Discord::Activity::SetLargeImageText(const char* text) {
	if (text && strlen(text) > 0) {
		StringUtils::Copy(CurrentActivity.assets.large_text,
			text,
			sizeof(CurrentActivity.assets.large_text) - 1);
	}
	else {
		memset(CurrentActivity.assets.large_text,
			0,
			sizeof(CurrentActivity.assets.large_text) - 1);
	}
}
void Discord::Activity::SetLargeImage(const char* key, const char* text) {
	Discord::Activity::SetLargeImageKey(key);
	Discord::Activity::SetLargeImageText(text);
}
void Discord::Activity::SetSmallImageKey(const char* key) {
	if (key && strlen(key) > 0) {
		StringUtils::Copy(CurrentActivity.assets.small_image,
			key,
			sizeof(CurrentActivity.assets.small_image) - 1);
	}
	else {
		memset(CurrentActivity.assets.small_image,
			0,
			sizeof(CurrentActivity.assets.small_image) - 1);
	}
}
void Discord::Activity::SetSmallImageText(const char* text) {
	if (text && strlen(text) > 0) {
		StringUtils::Copy(CurrentActivity.assets.small_text,
			text,
			sizeof(CurrentActivity.assets.small_text) - 1);
	}
	else {
		memset(CurrentActivity.assets.small_text,
			0,
			sizeof(CurrentActivity.assets.small_text) - 1);
	}
}
void Discord::Activity::SetSmallImage(const char* key, const char* text) {
	Discord::Activity::SetSmallImageKey(key);
	Discord::Activity::SetSmallImageText(text);
}

void Discord::Activity::SetElapsedTimer(time_t timestamp) {
	CurrentActivity.timestamps.start = (DiscordTimestamp)timestamp;
	CurrentActivity.timestamps.end = 0;
}
void Discord::Activity::SetRemainingTimer(time_t timestamp) {
	CurrentActivity.timestamps.start = 0;
	CurrentActivity.timestamps.end = (DiscordTimestamp)timestamp;
}
void Discord::Activity::SetPartySize(int size) {
	CurrentActivity.party.size.current_size = size;
}
void Discord::Activity::SetPartyMaxSize(int size) {
	CurrentActivity.party.size.max_size = size;
}

void Discord::Activity::Update() {
	if (!Discord::Initialized) {
		return;
	}

	ActivityManager->update_activity(
		ActivityManager, &CurrentActivity, NULL, OnUpdateActivityCallback);
}

int Discord::User::Update() {
	DiscordUser user;
	EDiscordResult result = UserManager->get_current_user(UserManager, &user);
	if (result != DiscordResult_Ok) {
		Log::Print(Log::LOG_API,
			"Discord: Failed to update current user (Error %d)",
			(int)result);
		return DiscordResultToHatchEnum(result);
	}

	CurrentUser.IDSnowflake = user.id;
	snprintf(CurrentUser.ID, sizeof(CurrentUser.ID), "%lld", (long long)CurrentUser.IDSnowflake);
	StringUtils::Copy(CurrentUser.Username, user.username, sizeof(CurrentUser.Username));
	CurrentUser.IsBot = user.bot;

	return DISCORDRESULT_OK;
}

bool Discord::User::IsUserPresent() {
	return CurrentUser.IDSnowflake != 0;
}

DiscordIntegrationUserInfo* Discord::User::GetDetails() {
	if (!Discord::User::IsUserPresent()) {
		return nullptr;
	}

	return &CurrentUser;
}
void Discord::User::GetAvatar(DiscordIntegrationUserAvatar* avatar,
	int size,
	DiscordIntegrationCallback* callback) {
	AvatarFetchCallbackData* fetchData =
		(AvatarFetchCallbackData*)Memory::Malloc(sizeof(AvatarFetchCallbackData));
	fetchData->Avatar = avatar;
	fetchData->Callback = callback;

	size = std::clamp(Math::CeilPOT(size), 16, 1024);

	snprintf(fetchData->Identifier,
		sizeof fetchData->Identifier,
		"%s-%d-%d",
		CurrentUser.ID,
		size,
		(int)time(NULL));

	DiscordImageHandle imageHandle;
	imageHandle.type = DiscordImageType_User;
	imageHandle.id = CurrentUser.IDSnowflake;
	imageHandle.size = size;

	ImageManager->fetch(ImageManager, imageHandle, true, fetchData, OnImageFetchCallback);
}
void Discord::User::GetAvatar(DiscordIntegrationUserAvatar* avatar,
	DiscordIntegrationCallback* callback) {
	return GetAvatar(avatar, 256, callback);
}
void Discord::User::GetAvatar(int size, DiscordIntegrationCallback* callback) {
	return GetAvatar(&UserAvatar, size, callback);
}

void Discord::UpdatePresence(DiscordIntegrationActivity presence) {
	Discord::Activity::SetDetails(presence.Details);
	Discord::Activity::SetState(presence.State);
	Discord::Activity::SetLargeImage(presence.LargeImageKey, presence.LargeImageText);
	Discord::Activity::SetSmallImage(presence.SmallImageKey, presence.SmallImageText);

	if (presence.EndTime) {
		Discord::Activity::SetRemainingTimer(presence.EndTime);
	}
	else {
		Discord::Activity::SetElapsedTimer(presence.StartTime);
	}

	Discord::Activity::SetPartySize(presence.PartySize);
	Discord::Activity::SetPartyMaxSize(presence.PartyMax);

	Discord::Activity::Update();
}

void Discord::Unload() {
	if (Core) {
		Core->destroy(Core);
		Core = NULL;
	}

	if (library) {
		SDL_UnloadObject(library);
		library = NULL;
	}
}
void Discord::Dispose() {
	if (!Discord::Initialized) {
		return;
	}

	Discord::Unload();
}

#else
int Discord::Init(const char* applicationID) {
	return DISCORDRESULT_ERROR;
}
void Discord::Update() {}

void Discord::Activity::SetDetails(const char* details) {}
void Discord::Activity::SetState(const char* state) {}
void Discord::Activity::SetLargeImageKey(const char* key) {}
void Discord::Activity::SetLargeImageText(const char* text) {}
void Discord::Activity::SetLargeImage(const char* key, const char* text) {}
void Discord::Activity::SetSmallImageKey(const char* key) {}
void Discord::Activity::SetSmallImageText(const char* text) {}
void Discord::Activity::SetSmallImage(const char* key, const char* text) {}

void Discord::Activity::SetElapsedTimer(time_t timestamp) {}
void Discord::Activity::SetRemainingTimer(time_t timestamp) {}
void Discord::Activity::SetPartySize(int size) {}
void Discord::Activity::SetPartyMaxSize(int size) {}

void Discord::Activity::Update() {}

int Discord::User::Update() {
	return DISCORDRESULT_ERROR;
}
bool Discord::User::IsUserPresent() {
	return false;
}
DiscordIntegrationUserInfo* Discord::User::GetDetails() {
	return nullptr;
}
void Discord::User::GetAvatar(DiscordIntegrationUserAvatar* avatar,
	int size,
	DiscordIntegrationCallback* callback) {}
void Discord::User::GetAvatar(DiscordIntegrationUserAvatar* avatar,
	DiscordIntegrationCallback* callback) {}
void Discord::User::GetAvatar(int size, DiscordIntegrationCallback* callback) {}

void Discord::UpdatePresence(DiscordIntegrationActivity presence) {}

void Discord::Unload() {}
void Discord::Dispose() {}

#endif
