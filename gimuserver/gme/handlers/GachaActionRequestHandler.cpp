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
    constexpr uint64_t SUMMON_UNIT_ID = 10011; // TODO: randomize from gacha pool

    Response::UserUnitInfo unitInfo;
    unitInfo.overwrite = false;
    {
        Response::UserUnitInfo::Data d;
        d.userID        = user.info.userID;
        d.userUnitID    = 9999; // TODO: use next AUTOINCREMENT id from user_units
        d.unitID        = SUMMON_UNIT_ID;
        d.unitTypeID    = 1;
        d.unitLv        = 1;
        d.newFlg        = 1;
        d.receiveDate   = 100;
        d.FeBP          = 100;
        d.FeMaxUsableBP = 200;
        d.exp = 1; d.totalExp = 100;

        // Look up real stats from the unit master
        const auto* e = System::Instance().Units().FindUnit(SUMMON_UNIT_ID);
        if (e)
        {
            d.element       = e->element;
            d.baseHp        = e->maxHp;
            d.baseAtk       = e->maxAtk;
            d.baseDef       = e->maxDef;
            d.baseHeal      = e->maxRec;
            d.leaderSkillID = e->lsId;
            d.skillID       = e->bbId;
            d.extraSkillID  = e->esId;
        }
        else
        {
            d.element = "fire";
            d.baseHp = d.baseAtk = d.baseDef = d.baseHeal = 1000;
        }

        d.addHp  = d.addAtk  = d.addDef  = d.addHeal  = 100;
        d.extHp  = d.extAtk  = d.extDef  = d.extHeal  = 100;
        d.limitOverHP = d.limitOverAtk = d.limitOverDef = d.limitOverHeal = 200;

        unitInfo.Mst.emplace_back(d);
    }
    unitInfo.Serialize(res);

    Response::GachaActionResult result;
    result.unitInstanceKey = std::to_string(unitInfo.Mst[0].userUnitID);
    result.gateAnimFlag    = 13762; // standard rare-pull animation
    result.Serialize(res);

	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
