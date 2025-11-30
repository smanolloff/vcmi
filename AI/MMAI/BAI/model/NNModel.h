/*
 * NNModel.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <onnxruntime_cxx_api.h>

#include "schema/base.h"
#include "schema/v13/types.h"

namespace MMAI::BAI
{

inline Ort::Env & ort_env()
{
	static Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "app"};
	return env;
}

class NNModel : public MMAI::Schema::IModel
{
public:
	explicit NNModel(std::string & path, float temperature, uint64_t seed);

	Schema::ModelType getType() override;
	std::string getName() override;
	int getVersion() override;
	Schema::Side getSide() override;
	int getAction(const MMAI::Schema::IState * s) override;
	double getValue(const MMAI::Schema::IState * s) override;

private:
	std::string path;
	float temperature;
	std::string name;
	int version;
	Schema::Side side;

	std::mt19937 rng;
	std::vector<std::vector<std::vector<int32_t>>> all_buckets;
	std::vector<std::vector<std::vector<int32_t>>> action_table;
	std::vector<Ort::AllocatedStringPtr> input_name_ptrs;
	std::vector<Ort::AllocatedStringPtr> output_name_ptrs;

	std::unique_ptr<Ort::Session> model = nullptr;
	Ort::AllocatorWithDefaultOptions allocator;
	Ort::MemoryInfo meminfo;

	std::pair<std::vector<Ort::Value>, int>
	prepareInputsV13(const MMAI::Schema::IState * state, const MMAI::Schema::V13::ISupplementaryData * sup);

	template<typename T>
	Ort::Value toTensor(const std::string & name, std::vector<T> & vec, const std::vector<int64_t> & shape);

	template<typename T>
	std::vector<T> t2v(const std::string & name, const Ort::Value & tensor, int numel);

	std::vector<const char *> input_names;
	std::vector<const char *> output_names;
};

} // namespace MMAI::BAI
