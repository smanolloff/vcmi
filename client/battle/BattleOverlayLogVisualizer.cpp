/*
 * BattleOverlayLogVisualizer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "BattleOverlayLogVisualizer.h"
#include "BattleInterface.h"
#include "BattleFieldController.h"

#include "render/Canvas.h"
#include "render/Colors.h"
#include "render/EFont.h"
#include "render/IFont.h"
#include "render/IRenderHandler.h"
#include "../gui/TextAlignment.h"
#include "../GameEngine.h"
#include "render/Graphics.h"

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

BattleOverlayLogVisualizer::BattleOverlayLogVisualizer(
	BattleRenderer::RendererRef & target,
	BattleInterface & owner)
	: target(target), owner(owner)
{
}

void BattleOverlayLogVisualizer::drawText(const BattleHex & hex, int lineNumber, const std::string & text)
{
	Point offset = owner.fieldController->hexPositionLocal(hex).topLeft() + Point(20, 20);
	const auto & font = ENGINE->renderHandler().loadFont(FONT_TINY);
	int h = font->getLineHeight();

	offset.y += h * lineNumber;

	target.drawText(offset, EFonts::FONT_TINY, Colors::YELLOW, ETextAlignment::TOPCENTER, text);
}
