/*
 * BattleAdvisor.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "BattleAdvisor.h"
#include "BattleWindow.h"
#include "BattleInterface.h"
#include "BattleStacksController.h"
#include "StackQueue.h"
#include "../CPlayerInterface.h"
#include "../GameEngine.h"
#include "../gui/WindowHandler.h"
#include "../widgets/Buttons.h"
#include "render/Canvas.h"
#include "../render/Colors.h"
#include "../../lib/CConfigHandler.h"
#include "../../lib/logging/BattleActionProbabilities.h"

namespace
{
	float normalizeBattleValue(float value)
	{
		static constexpr std::array points = {
			std::pair{0.3f, 0.0f},
			std::pair{0.5f, 0.1f},
			std::pair{0.6f, 0.2f},
			std::pair{0.7f, 0.4f},
			std::pair{0.8f, 0.7f},
			std::pair{0.9f, 0.9f},
			std::pair{1.0f, 1.0f}
		};

		const float magnitude = std::abs(value);
		if(magnitude <= points.front().first)
			return 0.0f;
		if(magnitude >= points.back().first)
			return std::copysign(1.0f, value);

		for(std::size_t i = 1; i < points.size(); ++i)
		{
			if(magnitude <= points[i].first)
			{
				const auto [x0, y0] = points[i - 1];
				const auto [x1, y1] = points[i];
				const float t = (magnitude - x0) / (x1 - x0);
				return std::copysign(vstd::lerp(y0, y1, t), value);
			}
		}
		return 0.0f;
	}
}

class BattleAdvisorProbabilityOverlay : public CIntObject
{
	float probability = 0.0f;
	float relativeProbability = 0.0f;
	bool visible = false;
	bool showPercentage = false;
	bool preferred = false;
	float outlinePhase = 0.0f;

public:
	explicit BattleAdvisorProbabilityOverlay(Point dimensions)
		: CIntObject(0)
	{
		pos.w = dimensions.x;
		pos.h = dimensions.y;
		setRedrawParent(true);
		addUsedEvents(TIME);
	}

	void tick(uint32_t msPassed) override
	{
	if(!visible || !preferred)
		return;

	outlinePhase = std::fmod(outlinePhase + msPassed * 0.0005f, 1.0f);
	redraw();
	}

	void setValue(float newProbability, float maximumProbability, bool newVisible, bool newShowPercentage, bool newPreferred)
	{
		probability = std::clamp(newProbability, 0.0f, 1.0f);
		relativeProbability = maximumProbability > 0.0f ? probability / maximumProbability : 0.0f;
		visible = newVisible;
		showPercentage = newShowPercentage;
		preferred = newPreferred;
	}

	void showAll(Canvas & canvas) override
	{
		if(!visible || probability <= 0.0f)
			return;

		canvas.drawColorBlended(pos, battleActionProbabilityColor(relativeProbability));

		if(preferred)
		{
			const int perimeter = 2 * (pos.w + pos.h - 2);

			const auto drawPixel = [&](int x, int y, int i)
			{
				const float position = static_cast<float>(i) / perimeter;

				const auto intensityBehind = [position](float head)
				{
					const float distance = head >= position
						? head - position
						: head + 1.0f - position;
					return std::max(0.0f, 1.0f - distance / 0.22f);
				};

				const float intensity = std::max(
					intensityBehind(outlinePhase),
					intensityBehind(std::fmod(outlinePhase + 0.5f, 1.0f)));

				if(intensity <= 0.0f)
					return;

				canvas.drawColorBlended(
					Rect(pos.x + x, pos.y + y, 1, 1),
					ColorRGBA(
						255,
						static_cast<uint8_t>(std::lround(160.0f + 80.0f * intensity)),
						static_cast<uint8_t>(std::lround(32.0f + 96.0f * intensity)),
						static_cast<uint8_t>(std::lround(255.0f * intensity))));
			};

			int i = 0;
			for(int x = 0; x < pos.w; ++x)
				drawPixel(x, 0, i++);
			for(int y = 1; y < pos.h; ++y)
				drawPixel(pos.w - 1, y, i++);
			for(int x = pos.w - 2; x >= 0; --x)
				drawPixel(x, pos.h - 1, i++);
			for(int y = pos.h - 2; y > 0; --y)
				drawPixel(0, y, i++);
		}

		if(showPercentage)
		{
			const auto text = std::to_string(std::lround(probability * 100.0f));
			canvas.drawText(pos.center() + Point(1, 1), FONT_TINY, Colors::BLACK, ETextAlignment::CENTER, text);
			canvas.drawText(pos.center(), FONT_TINY, Colors::WHITE, ETextAlignment::CENTER, text);
		}
	}
};

class BattleAdvisorValueOverlay : public CIntObject
{
	float value = 0.0f;
	bool visible = false;
	bool showValue = false;
	bool positiveToRight = true;

public:
	explicit BattleAdvisorValueOverlay(Rect area)
		: CIntObject(0)
	{
		pos = area;
		setRedrawParent(true);
	}

	void setValue(float newValue, bool newVisible, bool newShowValue, bool newPositiveToRight)
	{
		value = newValue;
		visible = newVisible;
		showValue = newShowValue;
		positiveToRight = newPositiveToRight;
	}

	void show(Canvas & canvas) override
	{
		showAll(canvas);
		CIntObject::show(canvas);
	}

	void showAll(Canvas & canvas) override
	{
		if(!visible)
			return;

		canvas.drawColorBlended(pos, ColorRGBA(0, 0, 0, 190));
		canvas.drawBorder(pos, ColorRGBA(198, 164, 104), 1);

		const float normalized = normalizeBattleValue(value);
		const int centerX = pos.center().x;
		const int halfWidth = (pos.w / 2) - 2;
		const int fillWidth = static_cast<int>(std::lround(std::abs(normalized) * static_cast<float>(halfWidth)));
		const bool fillToRight = (value >= 0.0f) == positiveToRight;
		if(fillWidth > 0)
		{
			const Rect fill = fillToRight
				? Rect(centerX + 1, pos.y + 2, fillWidth, pos.h - 4)
				: Rect(centerX - fillWidth, pos.y + 2, fillWidth, pos.h - 4);
			const auto color = normalized > 0.0f ? ColorRGBA(32, 224, 64, 210) : ColorRGBA(240, 48, 32, 210);
			canvas.drawColorBlended(fill, color);
		}
		canvas.drawLine(Point(centerX, pos.y + 1), Point(centerX, pos.bottom() - 2), Colors::WHITE, Colors::WHITE);

		if(showValue)
		{
			const auto text = boost::str(boost::format("%+.2f") % value);
			Point textPosition;
			ETextAlignment alignment;
			if(std::abs(normalized) >= 0.5f)
			{
				textPosition = Point(fillToRight ? centerX + 1 + (fillWidth / 2) : centerX - (fillWidth / 2), pos.center().y - 7);
			}
			else
			{
				textPosition = Point(fillToRight ? centerX + fillWidth + 6 : centerX - fillWidth - 6, pos.center().y - 7);
			}
			alignment = fillToRight ? ETextAlignment::CENTERLEFT : ETextAlignment::CENTERRIGHT;
			canvas.drawText(textPosition + Point(1, 1), FONT_TINY, Colors::BLACK, alignment, text);
			canvas.drawText(textPosition, FONT_TINY, Colors::WHITE, alignment, text);
		}
	}
};

BattleAdvisor::BattleAdvisor(BattleWindow & Window, BattleInterface & Owner)
	: window(Window), owner(Owner)
{
	battleActionProbabilities().clearBattleActionProbabilities();
	window.addCallback("actionProbabilityHeatmapChanged", [this](bool)
	{
		const bool heatmapVisible = settings["battle"]["showActionProbabilityHeatmap"].Bool();
		const bool percentagesVisible = settings["battle"]["showActionProbabilityPercentages"].Bool();
		const bool newHeatmapVisible = !heatmapVisible || !percentagesVisible;
		const bool newPercentagesVisible = heatmapVisible && !percentagesVisible;

		Settings heatmapSetting = settings.write["battle"]["showActionProbabilityHeatmap"];
		Settings percentagesSetting = settings.write["battle"]["showActionProbabilityPercentages"];
		heatmapSetting->Bool() = newHeatmapVisible;
		percentagesSetting->Bool() = newPercentagesVisible;
		window.widget<CToggleButton>("actionProbabilityHeatmap")->setSelectedSilent(newHeatmapVisible);
		if(newHeatmapVisible)
			owner.getCurrentPlayerInterface()->updateBattleActionHint();
		owner.redrawBattlefield();
		ENGINE->windows().totalRedraw();
	});
}

void BattleAdvisor::attachOverlays()
{
	auto heatmapButton = window.widget<CToggleButton>("actionProbabilityHeatmap");
	heatmapButton->setSelectedSilent(settings["battle"]["showActionProbabilityHeatmap"].Bool());

	auto waitButton = window.widget<CButton>("wait");
	waitProbabilityOverlay = std::make_shared<BattleAdvisorProbabilityOverlay>(waitButton->pos.dimensions() - Point(6, 6));
	waitButton->setOverlay(waitProbabilityOverlay);
	auto defendButton = window.widget<CButton>("defence");
	defendProbabilityOverlay = std::make_shared<BattleAdvisorProbabilityOverlay>(defendButton->pos.dimensions() - Point(6, 6));
	defendButton->setOverlay(defendProbabilityOverlay);
}

void BattleAdvisor::createValueOverlay()
{
	battleValueOverlay = std::make_shared<BattleAdvisorValueOverlay>(Rect(0, 0, 120, 12));
	window.addChild(battleValueOverlay.get());
	updateValuePosition();
	refresh();
}

void BattleAdvisor::refresh()
{
	std::optional<BattleActionProbabilities> probabilities;
	const auto revision = battleActionProbabilities().getBattleActionProbabilities(probabilities);
	const bool heatmapVisible = settings["battle"]["showActionProbabilityHeatmap"].Bool() && owner.stacksController && owner.stacksController->getActiveStack();
	const bool percentagesVisible = settings["battle"]["showActionProbabilityPercentages"].Bool();
	if(revision == actionProbabilityRevision && heatmapVisible == actionProbabilityHeatmapVisible && percentagesVisible == actionProbabilityPercentagesVisible)
		return;

	actionProbabilityRevision = revision;
	actionProbabilityHeatmapVisible = heatmapVisible;
	actionProbabilityPercentagesVisible = percentagesVisible;
	const float wait = probabilities ? probabilities->wait : 0.0f;
	const float defend = probabilities ? probabilities->defend : 0.0f;
	const float maximumProbability = probabilities ? probabilities->maximum() : 0.0f;
	waitProbabilityOverlay->setValue(wait, maximumProbability, heatmapVisible, percentagesVisible, wait > 0.0f && wait == maximumProbability);
	defendProbabilityOverlay->setValue(defend, maximumProbability, heatmapVisible, percentagesVisible, defend > 0.0f && defend == maximumProbability);
	const bool positiveToRight = probabilities && probabilities->side == BattleSide::DEFENDER;
	battleValueOverlay->setValue(probabilities ? probabilities->value : 0.0f, heatmapVisible && probabilities.has_value(), percentagesVisible, positiveToRight);
}

void BattleAdvisor::updateValuePosition()
{
	if(!battleValueOverlay || !window.queue)
		return;
	battleValueOverlay->moveTo(Point(window.queue->pos.center().x - (battleValueOverlay->pos.w / 2), window.queue->pos.bottom() + 3));
}

void BattleAdvisor::setTacticsMode(bool tactics)
{
	auto button = window.widget<CIntObject>("actionProbabilityHeatmap");
	if(tactics)
		button->disable();
	else
		button->enable();
}

ColorRGBA battleActionProbabilityColor(float probability)
{
	const auto value = std::clamp(probability, 0.0f, 1.0f);
	static constexpr std::array stops = {
		ColorRGBA(0, 64, 255, 112),
		ColorRGBA(0, 224, 255, 112),
		ColorRGBA(255, 224, 0, 112),
		ColorRGBA(255, 32, 0, 112)
	};

	const auto position = value * static_cast<float>(stops.size() - 1);
	const auto index = std::min(static_cast<std::size_t>(position), stops.size() - 2);
	return vstd::lerp(stops[index], stops[index + 1], position - static_cast<float>(index));
}

