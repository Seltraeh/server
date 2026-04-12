#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FrontierGateInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "dPM7oJDl"; }

	struct Data {
		uint32_t m_hBNPQAU0 = 0;
		uint32_t m_TPR79fyI = 0;
		uint32_t m_j0Uszek2 = 0;
		uint32_t m_PlayProgress = 0;
		std::string m_j28VNcUW = "";
		uint32_t m_69bpUIXR = 0;
		uint32_t m_qA7M9EjP = 0;
	};

	std::vector<Data> items;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
		Json::Value arr(Json::arrayValue);
		
		for (const auto& data : items) {
			Json::Value item;
			item["hBNPQAU0"] = std::to_string(data.m_hBNPQAU0);
			item["TPR79fyI"] = std::to_string(data.m_TPR79fyI);
			item["j0Uszek2"] = std::to_string(data.m_j0Uszek2);
			item["pG2n1A28"] = std::to_string(data.m_PlayProgress);
			item["j28VNcUW"] = data.m_j28VNcUW;
			item["69bpUIXR"] = std::to_string(data.m_69bpUIXR);
			item["qA7M9EjP"] = std::to_string(data.m_qA7M9EjP);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
