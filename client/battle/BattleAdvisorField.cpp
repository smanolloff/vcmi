/*
 * BattleAdvisorField.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "BattleAdvisorField.h"
#include "BattleAdvisor.h"
#include "BattleFieldController.h"
#include "BattleInterface.h"
#include "BattleStacksController.h"
#include "BattleWindow.h"
#include "render/Canvas.h"
#include "render/IImage.h"
#include "../render/Colors.h"
#include "../../lib/CConfigHandler.h"

namespace
{
void drawActionProbabilityOutline(Canvas & canvas, Point offset, int width, int height, const std::vector<Rect> & runs, float phase)
{
	const auto widthSize = static_cast<std::size_t>(width);
	const auto heightSize = static_cast<std::size_t>(height);
	std::vector<uint8_t> mask(widthSize * heightSize, 0);
	const auto index = [widthSize](int x, int y)
	{
		return (static_cast<std::size_t>(y) * widthSize) + static_cast<std::size_t>(x);
	};
	for(const auto & run : runs)
		for(int x = run.x; x < run.right(); ++x)
			mask[index(x, run.y)] = 1;

	std::vector<Point> boundary;
	for(int y = 0; y < height; ++y)
		for(int x = 0; x < width; ++x)
			if(mask[index(x, y)]
				&& (x == 0 || x + 1 == width || y == 0 || y + 1 == height
					|| !mask[index(x - 1, y)] || !mask[index(x + 1, y)]
					|| !mask[index(x, y - 1)] || !mask[index(x, y + 1)]))
				boundary.emplace_back(x, y);
	if(boundary.empty())
		return;

	double centerX = 0.0;
	double centerY = 0.0;
	for(const auto point : boundary)
	{
		centerX += static_cast<double>(point.x);
		centerY += static_cast<double>(point.y);
	}
	centerX /= static_cast<double>(boundary.size());
	centerY /= static_cast<double>(boundary.size());
	std::ranges::sort(boundary, [centerX, centerY](Point left, Point right)
	{
		const double leftAngle = std::atan2(static_cast<double>(left.y) - centerY, static_cast<double>(left.x) - centerX);
		const double rightAngle = std::atan2(static_cast<double>(right.y) - centerY, static_cast<double>(right.x) - centerX);
		return leftAngle < rightAngle;
	});

	static constexpr float trailLength = 0.22f;
	const auto trailIntensity = [phase](float position)
	{
		const auto intensityBehind = [position](float head)
		{
			const float distance = head >= position ? head - position : head + 1.0f - position;
			return std::max(0.0f, 1.0f - (distance / trailLength));
		};
		return std::max(intensityBehind(phase), intensityBehind(std::fmod(phase + 0.5f, 1.0f)));
	};

	for(std::size_t i = 0; i < boundary.size(); ++i)
	{
		const float intensity = trailIntensity(static_cast<float>(i) / static_cast<float>(boundary.size()));
		if(intensity <= 0.0f)
			continue;
		const auto alpha = static_cast<uint8_t>(std::lround(255.0f * intensity));
		const auto green = static_cast<uint8_t>(std::lround(160.0f + (80.0f * intensity)));
		const auto blue = static_cast<uint8_t>(std::lround(32.0f + (96.0f * intensity)));
		canvas.drawColorBlended(Rect(offset + boundary[i], Point(1, 1)), ColorRGBA(255, green, blue, alpha));
	}
}

struct PreferredActionProbabilityRegion
{
	BattleHex hex = BattleHex::INVALID;
	std::optional<BattleHex> attackTarget;
	const std::vector<Rect> * runs = nullptr;
	float probability = 0.0f;
};

PreferredActionProbabilityRegion findPreferredActionProbabilityRegion(
	const BattleActionProbabilities & probabilities,
	const std::vector<Rect> & centerRuns,
	const std::vector<Rect> & opaqueRuns,
	const std::array<std::vector<Rect>, 6> & attackRuns)
{
	PreferredActionProbabilityRegion result;
	result.probability = std::max(probabilities.wait, probabilities.defend);
	const auto consider = [&result](BattleHex hex, float probability, const std::vector<Rect> & runs, std::optional<BattleHex> attackTarget)
	{
		if(probability <= result.probability)
			return;
		result.hex = hex;
		result.attackTarget = attackTarget;
		result.runs = &runs;
		result.probability = probability;
	};

	for(const auto & [hex, directions] : probabilities.meleeAttacks)
		for(std::size_t direction = 0; direction < directions.size(); ++direction)
			if(directions[direction])
				consider(hex, *directions[direction], attackRuns[direction], probabilities.meleeAttackTargets.at(hex).at(direction));
	for(const auto & [hex, probability] : probabilities.moves)
		consider(hex, probability, centerRuns, std::nullopt);
	for(const auto & [hex, probability] : probabilities.attackTargets)
		consider(hex, probability, opaqueRuns, std::nullopt);
	return result;
}
}

BattleAdvisorField::BattleAdvisorField(BattleInterface & owner, BattleFieldController & field, std::shared_ptr<IImage> shade)
	: owner(owner), field(field), cellShade(std::move(shade))
{
	const auto regionAt = [this](int x, int y)
	{
		if(cellShade->isTransparent(Point(x, y)))
			return -1;

		const float nx = (static_cast<float>(x) - (cellShade->width() - 1) / 2.0f) / (cellShade->width() / 2.0f);
		const float ny = (static_cast<float>(y) - (cellShade->height() - 1) / 2.0f) / (cellShade->height() / 2.0f);
		const float radius = std::max({std::abs(nx), std::abs(0.5f * nx + ny), std::abs(0.5f * nx - ny)});
		if(radius <= 0.42f)
			return 6;

		static constexpr std::array directions = {
			std::pair{-0.447214f, -0.894427f},
			std::pair{0.447214f, -0.894427f},
			std::pair{1.0f, 0.0f},
			std::pair{0.447214f, 0.894427f},
			std::pair{-0.447214f, 0.894427f},
			std::pair{-1.0f, 0.0f}
		};
		int result = 0;
		float bestScore = std::numeric_limits<float>::lowest();
		for(int direction = 0; direction < directions.size(); ++direction)
		{
			const auto score = nx * directions[direction].first + ny * directions[direction].second;
			if(score > bestScore)
			{
				bestScore = score;
				result = direction;
			}
		}
		return result;
	};
	const auto buildRuns = [this, &regionAt](std::vector<Rect> & output, const std::function<bool(int)> & includeRegion)
	{
		for(int y = 0; y < cellShade->height(); ++y)
		{
			int runStart = -1;
			for(int x = 0; x <= cellShade->width(); ++x)
			{
				const bool included = x < cellShade->width() && includeRegion(regionAt(x, y));
				if(included && runStart < 0)
					runStart = x;
				else if(!included && runStart >= 0)
				{
					output.emplace_back(runStart, y, x - runStart, 1);
					runStart = -1;
				}
			}
		}
	};
	buildRuns(cellShadeOpaqueRuns, [](int region){ return region >= 0; });
	buildRuns(cellShadeCenterRuns, [](int region){ return region == 6; });
	for(int direction = 0; direction < cellShadeAttackRuns.size(); ++direction)
		buildRuns(cellShadeAttackRuns[direction], [direction](int region){ return region == direction; });
}

void BattleAdvisorField::showHeatmap(Canvas & canvas)
{
	probabilities.reset();
	if(settings["battle"]["showActionProbabilityHeatmap"].Bool() && owner.stacksController && owner.stacksController->getActiveStack())
		battleActionProbabilities().getBattleActionProbabilities(probabilities);
	if(probabilities)
		showActionProbabilityHeatmap(canvas, *probabilities);
}

void BattleAdvisorField::showLabels(Canvas & canvas)
{
	if(probabilities && settings["battle"]["showActionProbabilityPercentages"].Bool())
		showActionProbabilityLabels(canvas, *probabilities);
}

void BattleAdvisorField::showActionProbabilityHeatmap(Canvas & canvas, const BattleActionProbabilities & probabilities)
{
	const float maximumProbability = probabilities.maximum();
	if(maximumProbability <= 0.0f)
		return;

	const auto draw = [this, &canvas, maximumProbability](BattleHex hex, float probability, const std::vector<Rect> & runs)
	{
		if(probability <= 0.0f || !hex.isAvailable())
			return;
		const auto offset = field.hexPositionLocal(hex).topLeft();
		const auto color = battleActionProbabilityColor(probability / maximumProbability);
		for(const auto & run : runs)
			canvas.drawColorBlended(Rect(offset + run.topLeft(), run.dimensions()), color);
	};

	for(const auto & [hex, probability] : probabilities.attackTargets)
		draw(hex, probability, cellShadeOpaqueRuns);
	for(const auto & [hex, probability] : probabilities.moves)
		draw(hex, probability, cellShadeCenterRuns);
	for(const auto & [hex, directions] : probabilities.meleeAttacks)
		for(std::size_t direction = 0; direction < directions.size(); ++direction)
			if(directions[direction])
				draw(hex, *directions[direction], cellShadeAttackRuns[direction]);

	const auto preferred = findPreferredActionProbabilityRegion(probabilities, cellShadeCenterRuns, cellShadeOpaqueRuns, cellShadeAttackRuns);
	if(preferred.runs)
		drawActionProbabilityOutline(canvas, field.hexPositionLocal(preferred.hex).topLeft(), cellShade->width(), cellShade->height(), *preferred.runs, actionProbabilityOutlinePhase);
	if(preferred.attackTarget)
		drawActionProbabilityOutline(canvas, field.hexPositionLocal(*preferred.attackTarget).topLeft(), cellShade->width(), cellShade->height(), cellShadeOpaqueRuns, actionProbabilityOutlinePhase);
}

void BattleAdvisorField::showActionProbabilityLabels(Canvas & canvas, const BattleActionProbabilities & probabilities)
{
	const auto draw = [this, &canvas](BattleHex hex, float probability, Point offset)
	{
		if(!hex.isAvailable())
			return;
		auto position = field.hexPositionLocal(hex).center() + offset;
		const auto text = std::to_string(std::lround(std::clamp(probability, 0.0f, 1.0f) * 100.0f));
		if(text.size() == 3)
			--position.x;
		canvas.drawText(position + Point(1, 1), FONT_TINY, Colors::BLACK, ETextAlignment::CENTER, text);
		canvas.drawText(position, FONT_TINY, Colors::WHITE, ETextAlignment::CENTER, text);
	};
	static constexpr std::array directionOffsets = {
		Point(-11, -11), Point(11, -11), Point(15, 0),
		Point(9, 13), Point(-9, 13), Point(-15, 0)
	};

	for(const auto & [hex, probability] : probabilities.attackTargets)
		draw(hex, probability, Point(0, 0));
	for(const auto & [hex, probability] : probabilities.moves)
		draw(hex, probability, Point(0, 0));
	for(const auto & [hex, directions] : probabilities.meleeAttacks)
		for(std::size_t direction = 0; direction < directions.size(); ++direction)
			if(directions[direction])
				draw(hex, *directions[direction], directionOffsets[direction]);
}

void BattleAdvisorField::tick(uint32_t msPassed)
{
	const auto revision = battleActionProbabilities().getBattleActionProbabilitiesRevision();
	if(revision != actionProbabilityRevision)
	{
		actionProbabilityRevision = revision;
		if(owner.windowObject)
			owner.windowObject->redraw();
	}

	if(settings["battle"]["showActionProbabilityHeatmap"].Bool() && owner.stacksController && owner.stacksController->getActiveStack())
	{
		actionProbabilityOutlinePhase = std::fmod(actionProbabilityOutlinePhase + static_cast<float>(msPassed) * 0.0005f, 1.0f);
		if(owner.windowObject)
			owner.windowObject->redraw();
	}
}
