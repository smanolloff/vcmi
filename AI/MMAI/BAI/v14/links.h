/*
 * links.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "schema/v14/types.h"

namespace MMAI::BAI::V14
{
class Links : public Schema::V14::ILinks
{
public:
	std::vector<int64_t> srcIndex; // [src1, src2, ...]
	std::vector<int64_t> dstIndex; // [dst1, dst2, ...]
	std::vector<float> attributes; // [a11, a12, a13, a21, a22, a23, ...]  (for attributeSize=3)
	int attributeSize;

	std::vector<int64_t> getSrcIndex() const override
	{
		return srcIndex;
	}
	std::vector<int64_t> getDstIndex() const override
	{
		return dstIndex;
	}

	std::vector<float> getAttributes() const override
	{
		return attributes;
	}

	int getAttributeSize() const override
	{
		return attributeSize;
	}

	void add(int src, int dst, float attr)
	{
		srcIndex.push_back(src);
		dstIndex.push_back(dst);
		attributes.push_back(attr);
	}

	void add(int src, int dst, std::vector<float> & attrs)
	{
		srcIndex.push_back(src);
		dstIndex.push_back(dst);
		for(const float attr : attrs)
			attributes.push_back(attr);
	}
};
}
