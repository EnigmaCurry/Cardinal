/*
 * DISTRHO Cardinal Plugin
 * Copyright (C) 2021-2024 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

/**
 * This file is an edited version of VCVRack's app/MenuBar.cpp
 * Copyright (C) 2016-2023 VCV.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 */

#include <thread>
#include <utility>

#include <app/MenuBar.hpp>
#include <app/TipWindow.hpp>
#include <widget/OpaqueWidget.hpp>
#include <ui/Button.hpp>
#include <ui/MenuItem.hpp>
#include <ui/MenuSeparator.hpp>
#include <ui/SequentialLayout.hpp>
#include <ui/Slider.hpp>
#include <ui/TextField.hpp>
#include <ui/ProgressBar.hpp>
#include <ui/Label.hpp>
#include <engine/Engine.hpp>
#include <widget/FramebufferWidget.hpp>
#include <window/Window.hpp>
#include <asset.hpp>
#include <context.hpp>
#include <settings.hpp>
#include <helpers.hpp>
#include <system.hpp>
#include <plugin.hpp>
#include <patch.hpp>
#include <library.hpp>

#include "../CardinalCommon.hpp"
#include "../CardinalRemote.hpp"
#include "../CardinalPluginContext.hpp"
#include "DistrhoPlugin.hpp"
#include "DistrhoStandaloneUtils.hpp"

#ifdef DISTRHO_OS_WASM
# include <emscripten/emscripten.h>
# undef HAVE_LIBLO
#endif

#ifdef HAVE_LIBLO
# include <lo/lo.h>
#endif

namespace rack {
namespace asset {
std::string patchesPath();
}
namespace engine {
void Engine_setRemoteDetails(Engine*, remoteUtils::RemoteDetails*);
}

namespace app {
namespace menuBar {


struct MenuButton : ui::Button {
	void step() override {
		box.size.x = bndLabelWidth(APP->window->vg, -1, text.c_str()) + 1.0;
		Widget::step();
	}
	void draw(const DrawArgs& args) override {
		BNDwidgetState state = BND_DEFAULT;
		if (APP->event->hoveredWidget == this)
			state = BND_HOVER;
		if (APP->event->draggedWidget == this)
			state = BND_ACTIVE;
		bndMenuItem(args.vg, 0.0, 0.0, box.size.x, box.size.y, state, -1, text.c_str());
		Widget::draw(args);
	}
};


////////////////////
// File
////////////////////


struct FileButton : MenuButton {
	const bool isStandalone;
	std::vector<std::string> demoPatches;

#ifdef DISTRHO_OS_WASM
	static void WebBrowserDataSaved(const int err)
	{
		err ? async_dialog_message("Error, could not save web browser data!")
		    : async_dialog_message("Web browser data saved!");
	}

	static void wasmSaveAs()
	{
		async_dialog_text_input("Filename", nullptr, [](char* const filename) {
			if (filename == nullptr)
				return;

			APP->patch->path = asset::user("patches");
			system::createDirectories(APP->patch->path);

			APP->patch->path += filename;
			if (rack::system::getExtension(filename) != ".vcv")
				APP->patch->path += ".vcv";

			patchUtils::saveDialog(APP->patch->path);
			std::free(filename);
		});
	}
#endif

	FileButton(const bool standalone)
		: MenuButton(),  isStandalone(standalone)
	{
#if CARDINAL_VARIANT_MINI
		const std::string patchesDir = asset::patchesPath() + DISTRHO_OS_SEP_STR "mini";
#else
		const std::string patchesDir = asset::patchesPath() + DISTRHO_OS_SEP_STR "examples";
#endif

		if (system::isDirectory(patchesDir))
		{
			demoPatches = system::getEntries(patchesDir);
			std::sort(demoPatches.begin(), demoPatches.end(), [](const std::string& a, const std::string& b){
				return string::lowercase(a) < string::lowercase(b);
			});
		}
	}

	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

#ifndef DISTRHO_OS_WASM
		constexpr const char* const NewShortcut = RACK_MOD_CTRL_NAME "+N";
#else
		constexpr const char* const NewShortcut = "";
#endif
		menu->addChild(createMenuItem("New", NewShortcut, []() {
			patchUtils::loadTemplateDialog(false);
		}));

#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
		menu->addChild(createMenuItem("New (factory template)", "", []() {
			patchUtils::loadTemplateDialog(true);
		}));

#ifndef DISTRHO_OS_WASM
		constexpr const char* const OpenName = "Open...";
#else
		constexpr const char* const OpenName = "Import patch...";
#endif
		menu->addChild(createMenuItem(OpenName, RACK_MOD_CTRL_NAME "+O", []() {
			patchUtils::loadDialog();
		}));

		const std::string patchesDir = asset::user("patches");
		const std::vector<std::string> patches = system::isDirectory(patchesDir) ? system::getEntries(patchesDir) : std::vector<std::string>();
		menu->addChild(createSubmenuItem("Open local patch", "", [patches](ui::Menu* menu) {
			for (const std::string& path : patches) {
				std::string name = system::getStem(path);
				menu->addChild(createMenuItem(name, "", [=]() {
					patchUtils::loadPathDialog(path, false);
				}));
			}
		}, patches.empty()));

		menu->addChild(createSubmenuItem("Open recent", "", [](ui::Menu* menu) {
			for (const std::string& path : settings::recentPatchPaths) {
				std::string name = system::getStem(path);
				menu->addChild(createMenuItem(name, "", [=]() {
					patchUtils::loadPathDialog(path, false);
				}));
			}
		}, settings::recentPatchPaths.empty()));
#endif

		if (!demoPatches.empty())
		{
			menu->addChild(createSubmenuItem("Open demo / example project", "", [=](ui::Menu* const menu) {
				for (std::string path : demoPatches) {
					std::string label = system::getStem(path);

					for (size_t i=0, len=label.size(); i<len; ++i) {
						if (label[i] == '_')
							label[i] = ' ';
					}

					menu->addChild(createMenuItem(label, "", [path]() {
						patchUtils::loadPathDialog(path, true);
					}));
				}

				menu->addChild(new ui::MenuSeparator);

				menu->addChild(createMenuItem("Open patchstorage.com for more patches", "", []() {
					patchUtils::openBrowser("https://patchstorage.com/platform/cardinal/");
				}));
			}));
		}

#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
		menu->addChild(createMenuItem("Import selection...", "", [=]() {
			patchUtils::loadSelectionDialog();
		}, false, true));

		menu->addChild(new ui::MenuSeparator);

#ifndef DISTRHO_OS_WASM
		menu->addChild(createMenuItem("Save", RACK_MOD_CTRL_NAME "+S", []() {
			// NOTE: for plugin versions it will do nothing if path is empty, intentionally
			patchUtils::saveDialog(APP->patch->path);
		}, APP->patch->path.empty() && !isStandalone));

		menu->addChild(createMenuItem("Save as / Export...", RACK_MOD_CTRL_NAME "+Shift+S", []() {
			patchUtils::saveAsDialog();
		}));
#else
		menu->addChild(createMenuItem("Save", "", []() {
			if (APP->patch->path.empty())
				wasmSaveAs();
			else
				patchUtils::saveDialog(APP->patch->path);
		}));

		menu->addChild(createMenuItem("Save as...", "", []() {
			wasmSaveAs();
		}));

		menu->addChild(createMenuItem("Save and download compressed", "", []() {
			patchUtils::saveAsDialog();
		}));

		menu->addChild(createMenuItem("Save and download uncompressed", "", []() {
			patchUtils::saveAsDialogUncompressed();
		}));
#endif
#endif

		menu->addChild(createMenuItem("Revert", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+O", []() {
			patchUtils::revertDialog();
		}, APP->patch->path.empty()));

#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
		menu->addChild(createMenuItem("Overwrite template", "", []() {
			patchUtils::saveTemplateDialog();
		}));

#ifdef DISTRHO_OS_WASM
		menu->addChild(new ui::MenuSeparator);

		menu->addChild(createMenuItem("Save persistent browser data", "", []() {
			settings::save();
			EM_ASM({
				Module.FS.syncfs(false, function(err){ dynCall('vi', $0, [!!err]) });
			}, WebBrowserDataSaved);
		}));
#endif
#endif

#if defined(HAVE_LIBLO) || ! DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
#ifdef __MOD_DEVICES__
#define REMOTE_NAME "MOD"
#else
#define REMOTE_NAME "Remote"
#endif
		menu->addChild(new ui::MenuSeparator);

		remoteUtils::RemoteDetails* const remoteDetails = remoteUtils::getRemote();

		if (remoteDetails != nullptr && remoteDetails->connected) {
			menu->addChild(createMenuItem("Deploy to " REMOTE_NAME, "F7", [remoteDetails]() {
				remoteUtils::sendFullPatchToRemote(remoteDetails);
			}));

			menu->addChild(createCheckMenuItem("Auto deploy to " REMOTE_NAME, "",
				[remoteDetails]() {return remoteDetails->autoDeploy;},
				[remoteDetails]() {
					remoteDetails->autoDeploy = !remoteDetails->autoDeploy;
					Engine_setRemoteDetails(APP->engine, remoteDetails->autoDeploy ? remoteDetails : nullptr);
				}
			));
#ifndef __MOD_DEVICES__
		} else {
			menu->addChild(createMenuItem("Connect to " REMOTE_NAME "...", "", [remoteDetails]() {
				const std::string url = remoteDetails != nullptr ? remoteDetails->url : CARDINAL_DEFAULT_REMOTE_URL;
				async_dialog_text_input("Remote:", url.c_str(), [](char* const url) {
					if (url == nullptr)
						return;

					DISTRHO_SAFE_ASSERT(remoteUtils::connectToRemote(url));
					std::free(url);
				});
			}));
#endif
		}
#endif

#ifndef DISTRHO_OS_WASM
		if (isStandalone) {
			menu->addChild(new ui::MenuSeparator);

			menu->addChild(createMenuItem("Quit", RACK_MOD_CTRL_NAME "+Q", []() {
				APP->window->close();
			}));
		}
#endif
	}
};


////////////////////
// Edit
////////////////////


struct EditButton : MenuButton {
	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		struct UndoItem : ui::MenuItem {
			void step() override {
				text = "Undo " + APP->history->getUndoName();
				disabled = !APP->history->canUndo();
				MenuItem::step();
			}
			void onAction(const ActionEvent& e) override {
				APP->history->undo();
			}
		};
		menu->addChild(createMenuItem<UndoItem>("", RACK_MOD_CTRL_NAME "+Z"));

		struct RedoItem : ui::MenuItem {
			void step() override {
				text = "Redo " + APP->history->getRedoName();
				disabled = !APP->history->canRedo();
				MenuItem::step();
			}
			void onAction(const ActionEvent& e) override {
				APP->history->redo();
			}
		};
		menu->addChild(createMenuItem<RedoItem>("", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+Z"));

		menu->addChild(createMenuItem("Clear cables", "", [=]() {
			APP->patch->disconnectDialog();
		}));

		menu->addChild(new ui::MenuSeparator);

		patchUtils::appendSelectionContextMenu(menu);
	}
};


////////////////////
// View
////////////////////


struct ZoomQuantity : Quantity {
	void setValue(float value) override {
		APP->scene->rackScroll->setZoom(std::pow(2.f, value));
	}
	float getValue() override {
		return std::log2(APP->scene->rackScroll->getZoom());
	}
	float getMinValue() override {
		return -2.f;
	}
	float getMaxValue() override {
		return 2.f;
	}
	float getDefaultValue() override {
		return 0.0;
	}
	float getDisplayValue() override {
		return std::round(std::pow(2.f, getValue()) * 100);
	}
	void setDisplayValue(float displayValue) override {
		setValue(std::log2(displayValue / 100));
	}
	std::string getLabel() override {
		return "Zoom";
	}
	std::string getUnit() override {
		return "%";
	}
};
struct ZoomSlider : ui::Slider {
	ZoomSlider() {
		quantity = new ZoomQuantity;
	}
	~ZoomSlider() {
		delete quantity;
	}
};


struct CableOpacityQuantity : Quantity {
	void setValue(float value) override {
		settings::cableOpacity = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::cableOpacity;
	}
	float getDefaultValue() override {
		return 0.5;
	}
	float getDisplayValue() override {
		return getValue() * 100;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}
	std::string getLabel() override {
		return "Cable opacity";
	}
	std::string getUnit() override {
		return "%";
	}
};
struct CableOpacitySlider : ui::Slider {
	CableOpacitySlider() {
		quantity = new CableOpacityQuantity;
	}
	~CableOpacitySlider() {
		delete quantity;
	}
};


struct CableTensionQuantity : Quantity {
	void setValue(float value) override {
		settings::cableTension = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::cableTension;
	}
	float getDefaultValue() override {
		return 0.75;
	}
	float getDisplayValue() override {
		return getValue() * 100;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}
	std::string getLabel() override {
		return "Cable tension";
	}
	std::string getUnit() override {
		return "%";
	}
};
struct CableTensionSlider : ui::Slider {
	CableTensionSlider() {
		quantity = new CableTensionQuantity;
	}
	~CableTensionSlider() {
		delete quantity;
	}
};


struct RackBrightnessQuantity : Quantity {
	void setValue(float value) override {
		settings::rackBrightness = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::rackBrightness;
	}
	float getDefaultValue() override {
		return 1.0;
	}
	float getDisplayValue() override {
		return getValue() * 100;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}
	std::string getUnit() override {
		return "%";
	}
	std::string getLabel() override {
		return "Room brightness";
	}
	int getDisplayPrecision() override {
		return 3;
	}
};
struct RackBrightnessSlider : ui::Slider {
	RackBrightnessSlider() {
		quantity = new RackBrightnessQuantity;
	}
	~RackBrightnessSlider() {
		delete quantity;
	}
};


struct HaloBrightnessQuantity : Quantity {
	void setValue(float value) override {
		settings::haloBrightness = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return settings::haloBrightness;
	}
	float getDefaultValue() override {
		return 0.25;
	}
	float getDisplayValue() override {
		return getValue() * 100;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}
	std::string getUnit() override {
		return "%";
	}
	std::string getLabel() override {
		return "Light bloom";
	}
	int getDisplayPrecision() override {
		return 3;
	}
};
struct HaloBrightnessSlider : ui::Slider {
	HaloBrightnessSlider() {
		quantity = new HaloBrightnessQuantity;
	}
	~HaloBrightnessSlider() {
		delete quantity;
	}
};


struct KnobScrollSensitivityQuantity : Quantity {
	void setValue(float value) override {
		value = math::clamp(value, getMinValue(), getMaxValue());
		settings::knobScrollSensitivity = std::pow(2.f, value);
	}
	float getValue() override {
		return std::log2(settings::knobScrollSensitivity);
	}
	float getMinValue() override {
		return std::log2(1e-4f);
	}
	float getMaxValue() override {
		return std::log2(1e-2f);
	}
	float getDefaultValue() override {
		return std::log2(1e-3f);
	}
	float getDisplayValue() override {
		return std::pow(2.f, getValue() - getDefaultValue());
	}
	void setDisplayValue(float displayValue) override {
		setValue(std::log2(displayValue) + getDefaultValue());
	}
	std::string getLabel() override {
		return "Scroll wheel knob sensitivity";
	}
	int getDisplayPrecision() override {
		return 2;
	}
};
struct KnobScrollSensitivitySlider : ui::Slider {
	KnobScrollSensitivitySlider() {
		quantity = new KnobScrollSensitivityQuantity;
	}
	~KnobScrollSensitivitySlider() {
		delete quantity;
	}
};


// Multi-region indirection helper. Slider setValue() writes go to the active
// region if one exists — writing to legacy scalars gets clobbered next frame
// by RackWidget::step's publish-into-legacy. getValue() still reads the
// legacy scalar because publish-into-legacy keeps it in sync with the active
// region every frame.
static settings::RackRegion* activeRegionOrNull() {
	const int i = settings::rackspaceActiveRegion;
	if (i < 0 || i >= (int) settings::rackspaceRegions.size()) return nullptr;
	return &settings::rackspaceRegions[i];
}

// Slider::onDragMove feeds Quantity::moveValue tiny per-frame deltas which
// are re-read via getValue(). If getValue() returned the int setting we'd
// truncate the sub-integer accumulator every frame and the drag would never
// cross a step. Keep a private float accumulator per-quantity; the int
// setting only latches when the accumulator crosses to a new step.
struct RackspaceRowsQuantity : Quantity {
	float acc;
	RackspaceRowsQuantity() : acc((float) settings::rackspaceRows) {}
	void setValue(float value) override {
		acc = math::clamp(value, getMinValue(), getMaxValue());
		const int v = (int) std::round(acc);
		if (auto* r = activeRegionOrNull()) r->heightRows = v;
		else settings::rackspaceRows = v;
	}
	float getValue() override {
		return acc;
	}
	float getMinValue() override {
		return 1.f;
	}
	float getMaxValue() override {
		return 8.f;
	}
	float getDefaultValue() override {
		return 8.f;
	}
	float getDisplayValue() override {
		return (float) settings::rackspaceRows * 3.f;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 3.f);
	}
	std::string getLabel() override {
		return "Rack height";
	}
	std::string getUnit() override {
		return "U";
	}
	int getDisplayPrecision() override {
		return 2;
	}
};
struct RackspaceRowsSlider : ui::Slider {
	RackspaceRowsSlider() {
		quantity = new RackspaceRowsQuantity;
	}
	~RackspaceRowsSlider() {
		delete quantity;
	}
};


struct RackspaceWidthQuantity : Quantity {
	float acc;
	RackspaceWidthQuantity() : acc((float) settings::rackspaceWidthHP) {}
	void setValue(float value) override {
		acc = math::clamp(value, getMinValue(), getMaxValue());
		const int v = (int) std::round(acc);
		if (auto* r = activeRegionOrNull()) r->widthHP = v;
		else settings::rackspaceWidthHP = v;
	}
	float getValue() override {
		return acc;
	}
	float getMinValue() override {
		return 4.f;
	}
	float getMaxValue() override {
		return 168.f;
	}
	float getDefaultValue() override {
		return 168.f;
	}
	float getDisplayValue() override {
		return (float) settings::rackspaceWidthHP;
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue);
	}
	std::string getLabel() override {
		return "Rack width";
	}
	std::string getUnit() override {
		return " HP";
	}
	int getDisplayPrecision() override {
		return 3;
	}
};
struct RackspaceWidthSlider : ui::Slider {
	RackspaceWidthSlider() {
		quantity = new RackspaceWidthQuantity;
	}
	~RackspaceWidthSlider() {
		delete quantity;
	}
};


struct RackspaceBorderQuantity : Quantity {
	void setValue(float value) override {
		const float v = math::clamp(value, getMinValue(), getMaxValue());
		if (auto* r = activeRegionOrNull()) r->borderU = v;
		else settings::rackspaceBorderU = v;
	}
	float getValue() override {
		return settings::rackspaceBorderU;
	}
	float getMinValue() override {
		return 0.f;
	}
	float getMaxValue() override {
		return 0.5f;
	}
	float getDefaultValue() override {
		return 0.1f;
	}
	float getDisplayValue() override {
		return getValue();
	}
	void setDisplayValue(float displayValue) override {
		setValue(displayValue);
	}
	std::string getLabel() override {
		return "Border width";
	}
	std::string getUnit() override {
		return " U";
	}
	int getDisplayPrecision() override {
		return 2;
	}
};
struct RackspaceBorderSlider : ui::Slider {
	RackspaceBorderSlider() {
		quantity = new RackspaceBorderQuantity;
	}
	~RackspaceBorderSlider() {
		delete quantity;
	}
};


// Offset sliders — only visible when a region is active (they'd be no-ops
// otherwise, since offset is only meaningful for a region view). Range is
// generous so a demo layout can put regions far apart; internal storage
// clamps are wider still.
struct RackspaceOffsetHPQuantity : Quantity {
	float acc;
	RackspaceOffsetHPQuantity() : acc((float) settings::rackspaceOffsetHP) {}
	void setValue(float value) override {
		acc = math::clamp(value, getMinValue(), getMaxValue());
		const int v = (int) std::round(acc);
		if (auto* r = activeRegionOrNull()) r->offsetHP = v;
		else settings::rackspaceOffsetHP = v;
	}
	float getValue() override { return acc; }
	float getMinValue() override { return -400.f; }
	float getMaxValue() override { return  400.f; }
	float getDefaultValue() override { return 0.f; }
	float getDisplayValue() override { return (float) settings::rackspaceOffsetHP; }
	void setDisplayValue(float v) override { setValue(v); }
	std::string getLabel() override { return "Offset X"; }
	std::string getUnit() override { return " HP"; }
	int getDisplayPrecision() override { return 4; }
};
struct RackspaceOffsetHPSlider : ui::Slider {
	RackspaceOffsetHPSlider() { quantity = new RackspaceOffsetHPQuantity; }
	~RackspaceOffsetHPSlider() { delete quantity; }
};


struct RackspaceOffsetRowQuantity : Quantity {
	float acc;
	RackspaceOffsetRowQuantity() : acc((float) settings::rackspaceOffsetRow) {}
	void setValue(float value) override {
		acc = math::clamp(value, getMinValue(), getMaxValue());
		const int v = (int) std::round(acc);
		if (auto* r = activeRegionOrNull()) r->offsetRow = v;
		else settings::rackspaceOffsetRow = v;
	}
	float getValue() override { return acc; }
	float getMinValue() override { return -40.f; }
	float getMaxValue() override { return  40.f; }
	float getDefaultValue() override { return 0.f; }
	float getDisplayValue() override { return (float) settings::rackspaceOffsetRow * 3.f; }
	void setDisplayValue(float v) override { setValue(v / 3.f); }
	std::string getLabel() override { return "Offset Y"; }
	std::string getUnit() override { return " U"; }
	int getDisplayPrecision() override { return 3; }
};
struct RackspaceOffsetRowSlider : ui::Slider {
	RackspaceOffsetRowSlider() { quantity = new RackspaceOffsetRowQuantity; }
	~RackspaceOffsetRowSlider() { delete quantity; }
};


// Create a default region at the current viewport center and set it active.
// Called when the user toggles "Fixed size rack mode" ON with no regions defined,
// or clicks Add region from the menu. Auto-picks the first unused
// "Region N" name so the region list stays scannable.
static void bootstrapDefaultRegion() {
	settings::RackRegion r;
	r.kind       = "atomic";
	// Auto-name: pick the first "Region N" not already in use.
	int n = 1;
	while (true) {
		const std::string candidate = "Region " + std::to_string(n);
		bool taken = false;
		for (const settings::RackRegion& existing : settings::rackspaceRegions) {
			if (existing.name == candidate) { taken = true; break; }
		}
		if (!taken) { r.name = candidate; break; }
		++n;
	}
	// Use current legacy scalars as size hints — reproduces the pre-region
	// look for a user who just enabled the fixed rack for the first time.
	r.widthHP    = settings::rackspaceWidthHP;
	r.heightRows = settings::rackspaceRows;
	r.borderU    = settings::rackspaceBorderU;
	// Center the region on the current viewport: convert scroll grid-offset
	// (which is where the viewport center currently maps into rack coords)
	// into HP/row grid units relative to RACK_OFFSET, then back off half the
	// region size so the region straddles the viewport centre.
	int centerHP  = 0;
	int centerRow = 0;
	if (APP != nullptr && APP->scene != nullptr && APP->scene->rackScroll != nullptr) {
		const math::Vec gridOff = APP->scene->rackScroll->getGridOffset();
		centerHP  = (int) std::round(-gridOff.x / RACK_GRID_WIDTH);
		centerRow = (int) std::round(-gridOff.y / RACK_GRID_HEIGHT);
	}
	r.offsetHP   = centerHP  - r.widthHP    / 2;
	r.offsetRow  = centerRow - r.heightRows / 2;
	r.visible    = true;
	settings::rackspaceRegions.push_back(r);
	settings::rackspaceActiveRegion = (int) settings::rackspaceRegions.size() - 1;
}


#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
static void setAllFramebufferWidgetsDirty(widget::Widget* const widget)
{
	for (widget::Widget* child : widget->children)
	{
		if (widget::FramebufferWidget* const fbw = dynamic_cast<widget::FramebufferWidget*>(child))
			fbw->setDirty();

		setAllFramebufferWidgetsDirty(child);
	}
}
#endif


struct ViewButton : MenuButton {
	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		menu->addChild(createMenuLabel("Appearance"));

#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
		std::string darkModeText;
		if (settings::preferDarkPanels)
			darkModeText = CHECKMARK_STRING;
		menu->addChild(createMenuItem("Dark Mode", darkModeText, []() {
			switchDarkMode(!settings::preferDarkPanels);
			setAllFramebufferWidgetsDirty(APP->scene);
		}));
#endif

		menu->addChild(createBoolPtrMenuItem("Show tooltips", "", &settings::tooltips));

		ZoomSlider* zoomSlider = new ZoomSlider;
		zoomSlider->box.size.x = 250.0;
		menu->addChild(zoomSlider);

		CableOpacitySlider* cableOpacitySlider = new CableOpacitySlider;
		cableOpacitySlider->box.size.x = 250.0;
		menu->addChild(cableOpacitySlider);

		CableTensionSlider* cableTensionSlider = new CableTensionSlider;
		cableTensionSlider->box.size.x = 250.0;
		menu->addChild(cableTensionSlider);

		RackBrightnessSlider* rackBrightnessSlider = new RackBrightnessSlider;
		rackBrightnessSlider->box.size.x = 250.0;
		menu->addChild(rackBrightnessSlider);

		HaloBrightnessSlider* haloBrightnessSlider = new HaloBrightnessSlider;
		haloBrightnessSlider->box.size.x = 250.0;
		menu->addChild(haloBrightnessSlider);

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Rackspace"));

		// Toggling "Fixed size rack mode" ON with no regions defined auto-creates
		// one at the current viewport centre — otherwise the fixed rack has
		// nothing to point at and the user just sees an empty frame at
		// RACK_OFFSET, which is confusing.
		menu->addChild(createCheckMenuItem("Fixed size rack mode", "",
			[]() { return settings::rackspaceFixed; },
			[]() {
				const bool willEnable = !settings::rackspaceFixed;
				settings::rackspaceFixed = willEnable;
				if (willEnable && settings::rackspaceRegions.empty()) {
					bootstrapDefaultRegion();
				}
			}
		));

		// Infinite-mode toggle: hide region overlays + gestures so the
		// infinite grid works as it did before regions existed. Only shown
		// here (in infinite mode) because it's meaningless with the fixed
		// rack enforced.
		if (!settings::rackspaceFixed) {
			menu->addChild(createBoolPtrMenuItem("Show rack regions", "",
				&settings::rackspaceShowRegions));
		}

		if (settings::rackspaceFixed) {
			const int nRegions = (int) settings::rackspaceRegions.size();
			const int activeIdx = settings::rackspaceActiveRegion;

			// Region picker — hidden with only one region (or zero) to keep
			// the menu tidy for single-rack patches.
			if (nRegions > 1) {
				std::string activeName = "(none)";
				if (activeIdx >= 0 && activeIdx < nRegions)
					activeName = settings::rackspaceRegions[activeIdx].name;
				menu->addChild(createSubmenuItem("Region", activeName, [](ui::Menu* subMenu) {
					const int n = (int) settings::rackspaceRegions.size();
					for (int i = 0; i < n; ++i) {
						subMenu->addChild(createCheckMenuItem(
							settings::rackspaceRegions[i].name, "",
							[i]() { return settings::rackspaceActiveRegion == i; },
							[i]() { settings::rackspaceActiveRegion = i; }
						));
					}
				}));
			}

			// Region-management submenu.
			menu->addChild(createSubmenuItem("Regions…", "", [](ui::Menu* subMenu) {
				subMenu->addChild(createMenuItem("Add region", "", []() {
					bootstrapDefaultRegion();
				}));
				subMenu->addChild(createMenuItem("Delete active", "", []() {
					const int i = settings::rackspaceActiveRegion;
					if (i < 0 || i >= (int) settings::rackspaceRegions.size()) return;
					settings::rackspaceRegions.erase(settings::rackspaceRegions.begin() + i);
					if (settings::rackspaceRegions.empty())
						settings::rackspaceActiveRegion = -1;
					else if (settings::rackspaceActiveRegion >= (int) settings::rackspaceRegions.size())
						settings::rackspaceActiveRegion = (int) settings::rackspaceRegions.size() - 1;
				}));
			}));

			// Geometry sliders — setValue routes to the active region if any,
			// so drags Just Work on multi-region patches.
			RackspaceRowsSlider* rowsSlider = new RackspaceRowsSlider;
			rowsSlider->box.size.x = 250.0;
			menu->addChild(rowsSlider);

			RackspaceWidthSlider* widthSlider = new RackspaceWidthSlider;
			widthSlider->box.size.x = 250.0;
			menu->addChild(widthSlider);

			RackspaceBorderSlider* borderSlider = new RackspaceBorderSlider;
			borderSlider->box.size.x = 250.0;
			menu->addChild(borderSlider);

			// Offset sliders — only meaningful when a region is active; hide
			// otherwise so single-region-legacy patches don't get confused UI.
			if (activeIdx >= 0 && activeIdx < nRegions) {
				RackspaceOffsetHPSlider* offXSlider = new RackspaceOffsetHPSlider;
				offXSlider->box.size.x = 250.0;
				menu->addChild(offXSlider);

				RackspaceOffsetRowSlider* offYSlider = new RackspaceOffsetRowSlider;
				offYSlider->box.size.x = 250.0;
				menu->addChild(offYSlider);
			}

		}

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Module dragging"));

		menu->addChild(createBoolPtrMenuItem("Lock module positions", "", &settings::lockModules));

		menu->addChild(createBoolPtrMenuItem("Auto-squeeze modules when dragging", "", &settings::squeezeModules));

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Parameters"));

#ifdef DISTRHO_OS_WASM
		menu->addChild(createBoolPtrMenuItem("Lock cursor while dragging", "", &settings::allowCursorLock));
#endif

		static const std::vector<std::string> knobModeLabels = {
			"Linear",
			"Scaled linear",
			"Absolute rotary",
			"Relative rotary",
		};
		static const std::vector<int> knobModes = {0, 2, 3};
		menu->addChild(createSubmenuItem("Knob mode", knobModeLabels[settings::knobMode], [=](ui::Menu* menu) {
			for (int knobMode : knobModes) {
				menu->addChild(createCheckMenuItem(knobModeLabels[knobMode], "",
					[=]() {return settings::knobMode == knobMode;},
					[=]() {settings::knobMode = (settings::KnobMode) knobMode;}
				));
			}
		}));

		menu->addChild(createBoolPtrMenuItem("Scroll wheel knob control", "", &settings::knobScroll));

		KnobScrollSensitivitySlider* knobScrollSensitivitySlider = new KnobScrollSensitivitySlider;
		knobScrollSensitivitySlider->box.size.x = 250.0;
		menu->addChild(knobScrollSensitivitySlider);

		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("Window"));

#ifdef DISTRHO_OS_WASM
		const bool fullscreen = APP->window->isFullScreen();
		std::string rightText = "F11";
		if (fullscreen)
			rightText += " " CHECKMARK_STRING;
		menu->addChild(createMenuItem("Fullscreen", rightText, [=]() {
			APP->window->setFullScreen(!fullscreen);
		}));
#endif

		menu->addChild(createBoolPtrMenuItem("Invert zoom", "", &settings::invertZoom));

		static const std::vector<std::string> rateLimitLabels = {
			"None",
			"2x",
			"4x",
		};
		static const std::vector<int> rateLimits = {0, 1, 2};
		menu->addChild(createSubmenuItem("Update rate limit", rateLimitLabels[settings::rateLimit], [=](ui::Menu* menu) {
			for (int rateLimit : rateLimits) {
				menu->addChild(createCheckMenuItem(rateLimitLabels[rateLimit], "",
					[=]() {return settings::rateLimit == rateLimit;},
					[=]() {settings::rateLimit = rateLimit;}
				));
			}
		}));
	}
};


////////////////////
// Engine
////////////////////


struct EngineButton : MenuButton {
#ifdef HAVE_LIBLO
	bool remoteServerStarted = false;
#endif

	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		std::string cpuMeterText = "F3";
		if (settings::cpuMeter)
			cpuMeterText += " " CHECKMARK_STRING;
		menu->addChild(createMenuItem("Performance meters", cpuMeterText, [=]() {
			settings::cpuMeter ^= true;
		}));

#ifdef HAVE_LIBLO
		if (isStandalone()) {
			CardinalPluginContext* const context = static_cast<CardinalPluginContext*>(APP);
			CardinalBasePlugin* const plugin = static_cast<CardinalBasePlugin*>(context->plugin);

			// const bool remoteServerStarted = this->remoteServerStarted;
			const std::string remoteControlText = remoteServerStarted ? " " CHECKMARK_STRING : "";

			menu->addChild(createMenuItem("Enable OSC remote control", remoteControlText, [=]() {
				if (remoteServerStarted) {
					remoteServerStarted = false;
					plugin->stopRemoteServer();
					return;
				}

				async_dialog_text_input("OSC network port", CARDINAL_DEFAULT_REMOTE_PORT, [=](char* const port) {
					if (port == nullptr)
						return;

					if (plugin->startRemoteServer(port))
						remoteServerStarted = true;

					std::free(port);
				});
			}));
		}
#endif

		if (isUsingNativeAudio()) {
			if (supportsAudioInput()) {
				const bool enabled = isAudioInputEnabled();
				std::string rightText;
				if (enabled)
					rightText = CHECKMARK_STRING;
				menu->addChild(createMenuItem("Enable Audio Input", rightText, [enabled]() {
					if (!enabled)
						requestAudioInput();
				}));
			}

			if (supportsMIDI()) {
				std::string rightText;
				if (isMIDIEnabled())
					rightText = CHECKMARK_STRING;
				menu->addChild(createMenuItem("Enable/Reconnect MIDI", rightText, []() {
					requestMIDI();
				}));
			}

			if (supportsBufferSizeChanges()) {
				static const std::vector<uint32_t> bufferSizes = {
					#ifdef DISTRHO_OS_WASM
					256, 512, 1024, 2048, 4096, 8192, 16384
					#else
					128, 256, 512, 1024, 2048, 4096, 8192
					#endif
				};
				const uint32_t currentBufferSize = getBufferSize();
				menu->addChild(createSubmenuItem("Buffer Size", std::to_string(currentBufferSize), [=](ui::Menu* menu) {
					for (uint32_t bufferSize : bufferSizes) {
						menu->addChild(createCheckMenuItem(std::to_string(bufferSize), "",
							[=]() {return currentBufferSize == bufferSize;},
							[=]() {requestBufferSizeChange(bufferSize);}
						));
					}
				}));
			}
		}
	}

#ifdef HAVE_LIBLO
	void step() override {
		MenuButton::step();

		if (remoteServerStarted) {
			CardinalPluginContext* const context = static_cast<CardinalPluginContext*>(APP);
			CardinalBasePlugin* const plugin = static_cast<CardinalBasePlugin*>(context->plugin);
			plugin->stepRemoteServer();
		}
	}
#endif
};


////////////////////
// Help
////////////////////


struct HelpButton : MenuButton {
	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		menu->addChild(createMenuItem("Rack User manual", "F1", [=]() {
			patchUtils::openBrowser("https://vcvrack.com/manual");
		}));

		menu->addChild(createMenuItem("Cardinal project page", "", [=]() {
			patchUtils::openBrowser("https://github.com/DISTRHO/Cardinal/");
		}));

		menu->addChild(new ui::MenuSeparator);

#ifndef DISTRHO_OS_WASM
		menu->addChild(createMenuItem("Open user folder", "", [=]() {
			system::openDirectory(asset::user(""));
		}));

		menu->addChild(new ui::MenuSeparator);
#endif

		menu->addChild(createMenuLabel("Rack " + APP_VERSION + " Compatible"));
	}
};


////////////////////
// MenuBar
////////////////////


struct InfoLabel : ui::Label {
	int frameCount = 0;
	double frameDurationTotal = 0.0;
	double frameDurationAvg = NAN;
	// double uiLastTime = 0.0;
	// double uiLastThreadTime = 0.0;
	// double uiFrac = 0.0;

	void step() override {
		// Compute frame rate
		double frameDuration = APP->window->getLastFrameDuration();
		if (std::isfinite(frameDuration)) {
			frameDurationTotal += frameDuration;
			frameCount++;
		}
		if (frameDurationTotal >= 1.0) {
			frameDurationAvg = frameDurationTotal / frameCount;
			frameDurationTotal = 0.0;
			frameCount = 0;
		}

		// Compute UI thread CPU
		// double time = system::getTime();
		// double uiDuration = time - uiLastTime;
		// if (uiDuration >= 1.0) {
		// 	double threadTime = system::getThreadTime();
		// 	uiFrac = (threadTime - uiLastThreadTime) / uiDuration;
		// 	uiLastThreadTime = threadTime;
		// 	uiLastTime = time;
		// }

		text = "";

		if (box.size.x >= 400) {
			double fps = std::isfinite(frameDurationAvg) ? 1.0 / frameDurationAvg : 0.0;
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
			double meterAverage = APP->engine->getMeterAverage();
			double meterMax = APP->engine->getMeterMax();
			text = string::f("%.1f fps  %.1f%% avg  %.1f%% max", fps, meterAverage * 100, meterMax * 100);
#else
			text = string::f("%.1f fps", fps);
#endif
			text += "     ";
		}

		text += "Cardinal " + APP_EDITION + " " + CARDINAL_VERSION;

		Label::step();
	}
};


struct MenuBar : widget::OpaqueWidget {
	InfoLabel* infoLabel;

	MenuBar(const bool isStandalone)
		: widget::OpaqueWidget()
	{
		const float margin = 5;
		box.size.y = BND_WIDGET_HEIGHT + 2 * margin;

		ui::SequentialLayout* layout = new ui::SequentialLayout;
		layout->margin = math::Vec(margin, margin);
		layout->spacing = math::Vec(0, 0);
		addChild(layout);

		FileButton* fileButton = new FileButton(isStandalone);
		fileButton->text = "File";
		layout->addChild(fileButton);

		EditButton* editButton = new EditButton;
		editButton->text = "Edit";
		layout->addChild(editButton);

		ViewButton* viewButton = new ViewButton;
		viewButton->text = "View";
		layout->addChild(viewButton);

#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
		EngineButton* engineButton = new EngineButton;
		engineButton->text = "Engine";
		layout->addChild(engineButton);
#endif

		HelpButton* helpButton = new HelpButton;
		helpButton->text = "Help";
		layout->addChild(helpButton);

		infoLabel = new InfoLabel;
		infoLabel->box.size.x = 600;
		infoLabel->alignment = ui::Label::RIGHT_ALIGNMENT;
		layout->addChild(infoLabel);
	}

	void draw(const DrawArgs& args) override {
		bndMenuBackground(args.vg, 0.0, 0.0, box.size.x, box.size.y, BND_CORNER_ALL);
		bndBevel(args.vg, 0.0, 0.0, box.size.x, box.size.y);

		Widget::draw(args);
	}

	void step() override {
		Widget::step();
		infoLabel->box.size.x = box.size.x - infoLabel->box.pos.x - 5;
		// Setting 50% alpha prevents Label from using the default UI theme color, so set the color manually here.
		infoLabel->color = color::alpha(bndGetTheme()->regularTheme.textColor, 0.5);
	}
};


} // namespace menuBar


widget::Widget* createMenuBar() {
	menuBar::MenuBar* menuBar = new menuBar::MenuBar(isStandalone());
	return menuBar;
}


} // namespace app
} // namespace rack
