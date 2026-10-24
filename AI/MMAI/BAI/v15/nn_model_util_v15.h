/*
 * nn_model.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "AI/MMAI/common.h"
#include <onnxruntime_cxx_api.h>

namespace MMAI::BAI::V15::ModelUtil
{
template<class... Args>
[[noreturn]] inline void throwf(const std::string & fmt, Args &&... args)
{
	boost::format f("NNModel: " + fmt);
	(void)std::initializer_list<int>{((f % std::forward<Args>(args)), 0)...};
	throw std::runtime_error(f.str());
}
template<typename T>
void assertValidTensor(const std::string & name, const Ort::Value & tensor, int ndim)
{
	auto type_info = tensor.GetTensorTypeAndShapeInfo();
	auto shape = type_info.GetShape();
	auto dtype = type_info.GetElementType();

	if(shape.size() != ndim)
		throwf("assertValidTensor: %s: bad ndim: want: %d, have: %d", name, ndim, shape.size());

	if constexpr(std::is_same_v<T, float>)
	{
		if(dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
			throwf("assertValidTensor: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT), EI(dtype));
	}
	else if constexpr(std::is_same_v<T, int>)
	{
		if(dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32)
			throwf("assertValidTensor: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32), EI(dtype));
	}
	else if constexpr(std::is_same_v<T, int64_t>)
	{
		if(dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64)
			throwf("assertValidTensor: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64), EI(dtype));
	}
	else if constexpr(std::is_same_v<T, bool>)
	{
		if(dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL)
			throwf("assertValidTensor: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL), EI(dtype));
	}
	else
	{
		throwf("assertValidTensor: %s: can only work with bool, int and float", name);
	}
}

template<typename T>
T toScalar(const std::string & name, const Ort::Value & tensor)
{
	auto type_info = tensor.GetTensorTypeAndShapeInfo();
	auto shape = type_info.GetShape();
	auto dtype = type_info.GetElementType();

	if constexpr(std::is_same_v<T, float>)
	{
		if(dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
			throwf("toScalar: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT), EI(dtype));
	}
	else if constexpr(std::is_same_v<T, int64_t>)
	{
		if(dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64)
			throwf("toScalar: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64), EI(dtype));
	}
	else
	{
		throwf("toScalar: %s: unsupported scalar type", name);
	}

	int64_t numel = 1;
	for(auto d : shape)
		numel *= d;

	if(numel != 1)
		throwf("toScalar: %s: bad numel: want: 1, have: %d", name, numel);

	return tensor.GetTensorData<T>()[0];
}

template<typename T>
std::vector<T> toVec1D(const std::string & name, const Ort::Value & tensor, int64_t numel)
{
	assertValidTensor<T>(name, tensor, 1);

	auto type_info = tensor.GetTensorTypeAndShapeInfo();
	auto shape = type_info.GetShape();

	if(shape.at(0) != numel)
		throwf("toVec1D: %s: bad numel: want: %d, have: %d", name, numel, shape.at(0));

	const T * data = tensor.GetTensorData<T>();

	auto res = std::vector<T>{};
	res.reserve(numel);
	res.assign(data, data + numel);
	return res;
}

}
