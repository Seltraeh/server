#include "GmeController.hpp"
#include "handlers/BadgeInfoRequestHandler.hpp"
#include "handlers/DeckEditRequestHandler.hpp"
#include "handlers/FriendGetRequestHandler.hpp"
#include "handlers/GachaActionRequestHandler.hpp"
#include "handlers/GachaListRequestHandler.hpp"
#include "handlers/GetUserInfoRequestHandler.hpp"
#include "handlers/InitializeRequest2Handler.hpp"
#include "handlers/MissionStartRequestHandler.hpp"
#include "handlers/UnitFavoriteRequestHandler.hpp"
#include "handlers/ControlCenterEnterRequestHandler.hpp"    //Required to launch
#include "handlers/UpdateInfoLightRequestHandler.hpp"       //Required to launch - Starts the download process on fresh install
#include "handlers/HomeInfoRequestHandler.hpp"
#include "handlers/ChallengeArenaResetInfoRequestHandler.hpp"

#define REGISTER(name) InitializeHandler(std::make_shared<Handler::##name##Handler>())

void GmeController::InitializeHandlers()
{
    
    REGISTER(BadgeInfoRequest);
    REGISTER(DeckEditRequest);
    REGISTER(FriendGetRequest);    
    REGISTER(GachaActionRequest);
    REGISTER(GachaListRequest);
    REGISTER(GetUserInfoRequest);
	REGISTER(InitializeRequest2);
    REGISTER(MissionStartRequest);
    REGISTER(UnitFavoriteRequest);
	REGISTER(ControlCenterEnterRequest);    //Required to launch (TODO: Add logic)
	REGISTER(UpdateInfoLightRequest);       //Required to launch (TODO: Add logic)
	REGISTER(HomeInfoRequest);              //Required for home screen (TODO: Add logic)
	REGISTER(ChallengeArenaResetInfoRequest); //Required for unit scene access (TODO: Add logic)
}
