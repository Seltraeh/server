#pragma once

#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN

// Sent back to the client after a gacha pull to drive the gate animation.
// The obfuscated group key "Km35HAXv" corresponds to the gacha action result.
//
//  unitInstanceKey  ("edy7fq3L") - unique string ID of the summoned unit instance
//  gateAnimFlag     ("u0vkt9yH") - selects which gate animation to play;
//                                  13762 is the standard rare-pull animation.
struct GachaActionResult : public IResponse
{
    std::string unitInstanceKey;
    int gateAnimFlag;

    explicit GachaActionResult()
        : unitInstanceKey("0"), gateAnimFlag(0)
    {}

    const char* getGroupName() const override { return "Km35HAXv"; }
    bool isArray() const override { return false; }

protected:
    void SerializeFields(Json::Value& v, size_t) const override
    {
        v["edy7fq3L"] = unitInstanceKey;
        v["u0vkt9yH"] = gateAnimFlag;
    }
};

RESPONSE_NS_END
