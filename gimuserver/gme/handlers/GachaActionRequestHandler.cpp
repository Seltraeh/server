#include "GachaActionRequestHandler.hpp"
#include "gme/response/UserUnitInfo.hpp"
#include "gme/response/GachaActionResult.hpp"
#include "core/System.hpp"

void Handler::GachaActionRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;

    // TODO: Integrate SQL persistence so the summoned unit is stored in user_units
    //   and the next available id from the AUTOINCREMENT sequence is used as userUnitID.
    // TODO: Randomize unitID based on the gacha pool rates from gacha.json instead
    //   of always returning the same hard-coded unit.
    Response::UserUnitInfo unitInfo;
    unitInfo.overwrite = false;
    {
        Response::UserUnitInfo::Data d;
        d.userID      = user.info.userID;
        d.userUnitID  = 9999;
        d.unitID      = 10011;
        d.unitTypeID  = 1;
        d.element     = "light";
        d.unitLv      = 1;
        d.newFlg      = 1;
        d.receiveDate = 100;
        d.FeBP        = 100;
        d.FeMaxUsableBP = 200;
        d.baseHp  = 5000; d.addHp  = 100; d.extHp  = 100; d.limitOverHP   = 200;
        d.baseAtk = 1000; d.addAtk = 100; d.extAtk = 100; d.limitOverAtk  = 200;
        d.baseDef = 1000; d.addDef = 100; d.extDef = 100; d.limitOverDef  = 200;
        d.baseHeal= 1000; d.addHeal= 100; d.extHeal= 100; d.limitOverHeal = 200;
        d.exp = 1; d.totalExp = 100;
        unitInfo.Mst.emplace_back(d);
    }
    unitInfo.Serialize(res);

    Response::GachaActionResult result;
    result.unitInstanceKey = std::to_string(unitInfo.Mst[0].userUnitID);
    result.gateAnimFlag    = 13762; // standard rare-pull animation
    result.Serialize(res);

	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
