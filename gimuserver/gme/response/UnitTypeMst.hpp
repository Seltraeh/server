#pragma once
#include "../GmeRequest.hpp"

// UnitTypeMstResponse — maps unitTypeId (nBTx56W9) to type name and stat modifier strings.
// Group key: TODO — run extract_group_keys.py in IDA to find UnitTypeMstResponse::getGroupName
// Source data: F_UNIT_TYPE_MST_Ver52.json
//
// Known type IDs (sent as nBTx56W9 in UserUnitInfo):
//   1 = Lord    (balanced)
//   2 = Anima   (+HP, -Rec)
//   3 = Breaker (+Atk, -Def)
//   4 = Guardian(+Def, -Rec)
//   5 = Oracle  (+Rec, -Def)
//   6 = Rex     (+all)

RESPONSE_NS_BEGIN
struct UnitTypeMst : public IResponse
{
    // TODO: replace with real group key from IDA (extract_group_keys.py)
    const char* getGroupName() const override { return "UNKNOWN1"; }

    struct Data
    {
        std::string unitTypeId;   // nBTx56W9
        std::string typeName;     // Z3ocmb5J  e.g. "MST_UNITTYPES_1_NAME"
        std::string hpMod;        // 76IHLVsz  e.g. "0,0" or "5,10"
        std::string atkMod;       // 2M4mQZgk
        std::string defMod;       // Sj9zR38K
        std::string recMod;       // 6D3YN9rc
        std::string chance;       // ChD5b0jR  e.g. "23.00"

        void Serialize(Json::Value& v) const
        {
            v["nBTx56W9"] = unitTypeId;
            v["Z3ocmb5J"] = typeName;
            v["76IHLVsz"] = hpMod;
            v["2M4mQZgk"] = atkMod;
            v["Sj9zR38K"] = defMod;
            v["6D3YN9rc"] = recMod;
            v["ChD5b0jR"] = chance;
        }
    };

    std::vector<Data> Mst;

protected:
    size_t getRespCount() const override { return Mst.size(); }
    void SerializeFields(Json::Value& v, size_t i) const override
    {
        Mst.at(i).Serialize(v);
    }
};
RESPONSE_NS_END
