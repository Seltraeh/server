#include "MissionStartRequestHandler.hpp"
#include "gme/response/SignalKey.hpp"
#include "gme/response/UserUnitInfo.hpp"
#include "gme/response/MissionStartInfo.hpp"
#include "gme/response/BattleGroupMst.hpp"
#include "core/System.hpp"

void Handler::MissionStartRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	Json::Value res;

    // MissionStartInfo
    {
        Response::MissionStartInfo missionInfo;
        Response::MissionStartInfo::Data d;
        d.userID = user.info.userID;
        d.reinforceUserID = "n9ZMPC0t";
        d.friendPoint = 42640;
        d.missionID = "10";
        d.deckNum = 1;
        missionInfo.Mst.emplace_back(d);
        missionInfo.Serialize(res);
    }

    {
        Json::Value status;
        status["Kn51uR4Y"] = "0h6Q08SL";
        res["6FrKacq7"].append(status);
    }

    {
        Json::Value userState; //TODO: This user data overwrites the current user in gme.sqlite. Analyse the current structure to fill in any blanks we may have, comment what values mean what, and overwrite this segment with our database user profile so the data doesnt get swapped after a mission concludes.
        userState["h7eY3sAK"] = "n9ZMPC0t";
        userState["D9wXQI2V"] = "309";
        userState["d96tuT2E"] = "232666";
        userState["YnM14RIP"] = "199";
        userState["0P9X1YHs"] = "196";
        userState["V0yJS7vZ"] = "1650627374";
        userState["f0IY4nj8"] = 540;
        userState["9m5FWR8q"] = "3";
        userState["YS2JG9no"] = "2";
        userState["32HCWt51"] = "1650627005";
        userState["jp9s8IyY"] = 3231;
        userState["ouXxIY63"] = "150";
        userState["Px1X7fcd"] = "620";
        userState["QYP4kId9"] = "383";
        userState["Z0Y4RoD7"] = "1";
        userState["gKNfIZiA"] = 2;
        userState["TwqMChon"] = "-1,-99,-99";
        userState["3u41PhR2"] = "50";
        userState["2rR5s6wn"] = "0";
        userState["5pjoGBC4"] = "200";
        userState["iI7Wj6pM"] = "125";
        userState["J3stQ7jd"] = "42640";
        userState["Najhr8m6"] = "64394391";
        userState["HTVh8a65"] = "99202910";
        userState["03UGMHxF"] = "225";
        userState["bM7RLu5K"] = "La Gimu Trolla PUNTO";
        userState["s2WnRw9N"] = "460,420,430,0,0";
        userState["EfinBo65"] = "7";
        userState["qVBx7g2c"] = "0";
        userState["1RQT92uE"] = "0";
        userState["kW5QuUz7"] = "20220422";
        userState["3w6YDS4z"] = "3";
        userState["lKuj3Ier"] = "";
        userState["JmFn3g9t"] = "0";
        userState["9r3aLmaB"] = "1";
        userState["bya9a67k"] = "2580";
        userState["22rqpZTo"] = "3285";
        userState["KAZmxkgy"] = 0;
        userState["AKP8t3xK"] = 0;
        userState["Nou5bCmm"] = 0;
        userState["s3uU4Lgb"] = 1;
        userState["3a8b9D8i"] = "0";
        userState["7qncTHUJ"] = 0;
        userState["38d7D18b"] = 0;
        userState["D38bda8B"] = 0;
        userState["Qo9doUsp"] = 0;
        userState["d37CaiX1"] = 0;
        userState["92uj7oXB"] = 0;
        res["fEi17cnx"].append(userState);
    }

    {
        Json::Value reinforceUser;
        reinforceUser["h7eY3sAK"] = "n9ZMPC0t";
        reinforceUser["dD64grYH"] = "334";
        reinforceUser["3w6YDS4z"] = "20";
        reinforceUser["d96tuT2E"] = "36216751";
        reinforceUser["pThS5FE3"] = "152762744";
        reinforceUser["mn5Tj3fz"] = "125539575";
        reinforceUser["jG91JRxN"] = "36137127";
        reinforceUser["06phPeqv"] = "35791092";
        reinforceUser["Zq8ej5IN"] = "346035";
        reinforceUser["isRx41jy"] = "147217074";
        reinforceUser["20qd9shE"] = "48090082";
        reinforceUser["Sf95jez7"] = 146280;
        reinforceUser["I29Qgxot"] = "124800";
        reinforceUser["WMC6rNF1"] = "702";
        reinforceUser["Z93pUQhG"] = "2180";
        reinforceUser["Rc6St9h1"] = "45";
        reinforceUser["k5Sjn9Zq"] = "7849";
        reinforceUser["c3Bo97kI"] = "2674";
        reinforceUser["Gt2msFb1"] = "7050";
        reinforceUser["07HgoLtC"] = "1";
        reinforceUser["AEz43gai"] = "0";
        reinforceUser["8CEu9Kcm"] = "0";
        reinforceUser["3DBVLY8H"] = "2887";
        reinforceUser["c4im6B2v"] = 2092;
        reinforceUser["UCN04WxE"] = "1994";
        reinforceUser["ovFJ6Hp0"] = "15045";
        reinforceUser["TW1Mrtp5"] = "502";
        reinforceUser["5NRJQ1LU"] = "824760049";
        reinforceUser["XP06YWdT"] = "569";
        reinforceUser["U8uZLA34"] = "666505";
        reinforceUser["rZQJF5G9"] = "51673";
        reinforceUser["0LwvAF3H"] = "10523";
        reinforceUser["rQ3TAy6I"] = "10116";
        reinforceUser["hoG2ieT5"] = "5461379";
        reinforceUser["6PLsn8xo"] = "2114221";
        reinforceUser["84BC2kXw"] = "3418";
        reinforceUser["5pg7MYCQ"] = "576";
        reinforceUser["mFID53JZ"] = "1124";
        reinforceUser["e6BKoYy9"] = "1814061344";
        res["zI2tJB7R"].append(reinforceUser);
    }

    // BattleGroupMst — one entry per wave (battle_order 1–5)
    {
        Response::BattleGroupMst bgm;
        auto& mst = bgm.Mst;

        auto add = [&](uint32_t gid, uint32_t order, uint32_t monGrp, bool boss) {
            Response::BattleGroupMst::Data d;
            d.group_id          = gid;
            d.mission_id        = 10;
            d.battle_order      = order;
            d.first_atk_rate    = 0;
            d.battle_monster_id = monGrp;
            d.boss_flg          = boss;
            mst.emplace_back(d);
        };

        add(11, 1, 101301, false);
        add(12, 2, 101302, false);
        add(14, 3, 101300, false);
        add(16, 4, 101302, false);
        add(18, 5, 101304, true);

        bgm.Serialize(res);
    }

    {
        Json::Value bonus;
        bonus["k9cxD7Ba"] = "58844709";
        bonus["j3g5P4cq"] = "1";
        bonus["nA95Bdj6"] = "0";
        bonus["5Z1LNoyH"] = "0";
        bonus["LE6JkUp7"] = "1|0:25:30030:1:1| @2|0:25:10030:1:1| @3| |1/1/4:25:10000:1@4|1:30:50030:2:2| @5| | ";
        res["Kz7qfSs5"].append(bonus);
    }

    // BattleMonsterGroupMst — enemy placement per monster group
    {
        Response::BattleMonsterGroupMst bmgm;
        auto& mst = bmgm.Mst;

        auto add = [&](uint32_t monGrp, uint32_t monId, uint32_t order,
                       const std::string& pos, const std::string& itemDrop,
                       const std::string& unitDrop, const std::string& treasDrop) {
            Response::BattleMonsterGroupMst::Data d;
            d.battle_monster_id = monGrp;
            d.monster_id        = monId;
            d.group_order       = order;
            d.position          = pos;
            d.item_drop         = itemDrop;
            d.unit_drop         = unitDrop;
            d.treasure_drop     = treasDrop;
            mst.emplace_back(d);
        };

        add(101301, 30352, 0, "180:302", "7:10000:3,7:10300:3", "25:30030:1:0,30:30030:2:0", "15,25:500,25:250,25:10,25:10000:1");
        add(101301, 40352, 1, "120:248", "7:10000:3,7:10300:3", "25:40030:1:0,30:40030:2:0", "15,25:500,25:250,25:10,25:10000:1");
        add(101302, 10352, 0, "180:302", "7:10000:3,7:10300:3", "25:10030:1:0,30:10030:2:0", "15,25:500,25:250,25:10,25:10000:1");
        add(101302, 50352, 1, "120:248", "7:10000:3,7:10300:3", "25:50030:1:0,30:50030:2:0", "15,25:500,25:250,25:10,25:10000:1");
        add(101302, 30352, 2,  "96:352", "7:10000:3,7:10300:3", "25:30030:1:0,30:30030:2:0", "15,25:500,25:250,25:10,25:10000:1");
        add(101300, 10352, 0, "180:302", "7:10000:3,7:10300:3", "25:10030:1:0,30:10030:2:0", "15,25:500,25:250,25:10,25:10000:1");
        add(101300, 20352, 1, "120:248", "7:10000:3,7:10300:3", "25:20030:1:0,30:20030:2:0", "15,25:500,25:250,25:10,25:10000:1");
        add(101304, 40401, 0, "180:302", "7:10301:5,3:10603:1",  "0:0:0:0,0:0:0:0",           "0,25:500,25:250,25:10,25:10301:1");

        bmgm.Serialize(res);
    }

    // MonsterMst — enemy stat definitions
    {
        Response::MonsterMst mm;
        auto& mst = mm.Mst;

        auto makeMonster = [](uint32_t id, uint32_t hp, uint32_t atk, uint32_t def,
                               uint32_t elem, uint32_t wait,
                               const std::string& efx, const std::string& dmg,
                               uint32_t maxZel, uint32_t zelCnt,
                               uint32_t maxKarma, uint32_t karmaCnt,
                               uint32_t aiId, uint32_t unitId) -> Response::MonsterMst::Data
        {
            Response::MonsterMst::Data d;
            d.monster_id       = id;
            d.hp               = hp;
            d.atk              = atk;
            d.def              = def;
            d.element          = elem;
            d.drop_check_cnt   = 1;
            d.max_zel_drop     = maxZel;
            d.zel_drop_cnt     = zelCnt;
            d.max_karma_drop   = maxKarma;
            d.karma_drop_cnt   = karmaCnt;
            d.wait             = wait;
            d.move_speed_type  = 2;
            d.atk_move_type    = 1;
            d.back_move_type   = 1;
            d.skill_move_type  = 1;
            d.after_image      = 1;
            d.max_act_cnt      = 1;
            d.min_act_cnt      = 1;
            d.act_rate         = 100.0f;
            d.ai_id            = aiId;
            d.unit_id          = unitId;
            d.effect_frame     = efx;
            d.damage_frame     = dmg;
            return d;
        };

        mst.emplace_back(makeMonster(30352, 830,  340,  90, 3, 5, "24:123:1", "30:100:2:1", 33, 5, 21, 5, 1, 30030));
        mst.emplace_back(makeMonster(40352, 760,  370,   0, 4, 2, "24:124:1", "30:100:2:1", 30, 5, 19, 5, 1, 40030));
        mst.emplace_back(makeMonster(10352, 800,  320,  50, 1, 5, "24:121:1", "30:100:2:1", 32, 5, 20, 5, 1, 10030));
        mst.emplace_back(makeMonster(50352, 850,  340,  60, 5, 1, "24:125:1", "30:100:2:1", 34, 5, 21, 5, 1, 50030));
        mst.emplace_back(makeMonster(20352, 780,  330,  20, 2, 5, "24:122:1", "30:100:2:1", 31, 5, 20, 5, 1, 20030));
        mst.emplace_back(makeMonster(40401, 2500, 470,  30, 4, 0, "36:124:1", "42:100:2:1", 100, 5, 63, 5, 1, 40031));

        mm.Serialize(res);
    }

    // UnitSkillMst — skill definitions used by enemies in this mission
    {
        Response::UnitSkillMst usm;
        auto& mst = usm.Mst;

        auto addSkill = [&](uint32_t id, uint32_t type, uint32_t rank,
                            const std::string& proc) {
            Response::UnitSkillMst::Data d;
            d.skill_id       = id;
            d.skill_type     = type;
            d.skill_rank     = rank;
            d.move_flag      = 0;
            d.atk_move_flag  = 0;
            d.drop_check_cnt = 0;
            d.wait           = 0;
            d.element        = 0;
            d.process_id     = proc;
            d.target_type    = "1";
            d.target_area    = "0";
            d.disp_frame     = "5:18";
            d.effect_frame   = "5:18";
            d.start_frame    = "5";
            d.damage_frame   = "18:1";
            mst.emplace_back(d);
        };

        addSkill(2000140, 1, 1, "1:10000:1");
        addSkill(2000141, 1, 1, "1:10300:1");
        addSkill(2000142, 1, 1, "1:10000:1");
        addSkill(2000143, 1, 1, "1:10300:1");
        addSkill(2000144, 1, 1, "1:10000:1");
        addSkill(2000145, 1, 1, "1:10300:1");
        addSkill(2000146, 1, 1, "1:10301:5");
        addSkill(2000147, 3, 1, "3:10603:1");
        addSkill(2000148, 1, 1, "1:10000:1");

        usm.Serialize(res);
    }

    // AIMst — AI behaviour for enemy monsters (ai_id 20017)
    {
        Response::AIMst aim;
        auto& mst = aim.Mst;

        auto addAI = [&](uint32_t pri, const std::string& term, uint32_t tgt,
                         const std::string& search, const std::string& atk, uint32_t pct) {
            Response::AIMst::Data d;
            d.ai_id       = 20017;
            d.priority    = pri;
            d.ai_term     = term;
            d.target      = tgt;
            d.search_term = search;
            d.atk_param   = atk;
            d.percent     = pct;
            mst.emplace_back(d);
        };

        addAI(1,  "1:50:1", 1, "1:1", "2000140", 100);
        addAI(2,  "1:50:2", 1, "1:1", "2000141", 100);
        addAI(3,  "1:50:3", 1, "1:1", "2000142", 100);
        addAI(4,  "1:50:4", 1, "1:1", "2000143", 100);
        addAI(5,  "1:50:5", 1, "1:1", "2000144", 100);
        addAI(6,  "0:0:0",  1, "1:1", "2000140",  50);
        addAI(7,  "0:0:0",  1, "2:1", "2000141",  50);
        addAI(8,  "0:0:0",  1, "1:1", "2000142",  50);
        addAI(9,  "0:0:0",  1, "2:1", "2000143",  50);
        addAI(10, "0:0:0",  1, "1:1", "2000144", 100);
        addAI(11, "0:0:0",  1, "1:1", "2000146",  60);
        addAI(12, "0:0:0",  1, "1:1", "2000147",  40);

        aim.Serialize(res);
    }

    res["8hoyIF9Q"] = Json::arrayValue;
    res["VZwB7f3j"] = Json::arrayValue;

    {
        Json::Value status;
        status["Kn51uR4Y"] = "0";
        res["nAligJSQ"].append(status);
    }

    {
        Json::Value clientInfo;
        clientInfo["h7eY3sAK"] = "n9ZMPC0t";
        clientInfo["B5JQyV8j"] = "Arves100";
        clientInfo["iN7buP0j"] = "WAS-LX1A_android8.0.0";
        clientInfo["Ma5GnU0H"] = "4e457983-74b0-4ea7-9a98-1c5890dfc836";
        res["IKqx1Cn9"].append(clientInfo);
    }

    {
        Json::Value announcement;
        announcement["xJNom6i0"] = "3876";
        announcement["jsRoN50z"] = "http://ios21900.bfww.gumi.sg//news.gumi.sg/bravefrontier/news/files/html/2022-03/Closure_Announcement_033022_1648608188.html";
        res["Pj6zDW3m"] = announcement;
    }

	cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
