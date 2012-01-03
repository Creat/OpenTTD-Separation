/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_gui.cpp GUIs for a number of misc windows. */

#include "stdafx.h"
#include "debug.h"
#include "landscape.h"
#include "error.h"
#include "gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "command_func.h"
#include "company_func.h"
#include "town.h"
#include "string_func.h"
#include "company_base.h"
#include "texteff.hpp"
#include "strings_func.h"
#include "window_func.h"
#include "querystring_gui.h"
#include "core/geometry_func.hpp"
#include "newgrf_debug.h"

#include "widgets/misc_widget.h"

#include "table/strings.h"

/**
 * Try to retrive the current clipboard contents.
 *
 * @note OS-specific funtion.
 * @return True if some text could be retrived.
 */
bool GetClipboardContents(char *buffer, size_t buff_len);

int _caret_timer;

static const NWidgetPart _nested_land_info_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_LAND_AREA_INFORMATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEBUGBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_LI_BACKGROUND), EndContainer(),
};

static const WindowDesc _land_info_desc(
	WDP_AUTO, 0, 0,
	WC_LAND_INFO, WC_NONE,
	0,
	_nested_land_info_widgets, lengthof(_nested_land_info_widgets)
);

class LandInfoWindow : public Window {
	enum LandInfoLines {
		LAND_INFO_CENTERED_LINES   = 12,                       ///< Up to 12 centered lines
		LAND_INFO_MULTICENTER_LINE = LAND_INFO_CENTERED_LINES, ///< One multicenter line
		LAND_INFO_LINE_END,
	};

	static const uint LAND_INFO_LINE_BUFF_SIZE = 512;

public:
	char landinfo_data[LAND_INFO_LINE_END][LAND_INFO_LINE_BUFF_SIZE];
	TileIndex tile;

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_LI_BACKGROUND) return;

		uint y = r.top + WD_TEXTPANEL_TOP;
		for (uint i = 0; i < LAND_INFO_CENTERED_LINES; i++) {
			if (StrEmpty(this->landinfo_data[i])) break;

			DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, this->landinfo_data[i], i == 0 ? TC_LIGHT_BLUE : TC_FROMSTRING, SA_HOR_CENTER);
			y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			if (i == 0) y += 4;
		}

		if (!StrEmpty(this->landinfo_data[LAND_INFO_MULTICENTER_LINE])) {
			SetDParamStr(0, this->landinfo_data[LAND_INFO_MULTICENTER_LINE]);
			DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, r.bottom - WD_TEXTPANEL_BOTTOM, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_LI_BACKGROUND) return;

		size->height = WD_TEXTPANEL_TOP + WD_TEXTPANEL_BOTTOM;
		for (uint i = 0; i < LAND_INFO_CENTERED_LINES; i++) {
			if (StrEmpty(this->landinfo_data[i])) break;

			uint width = GetStringBoundingBox(this->landinfo_data[i]).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
			size->width = max(size->width, width);

			size->height += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			if (i == 0) size->height += 4;
		}

		if (!StrEmpty(this->landinfo_data[LAND_INFO_MULTICENTER_LINE])) {
			uint width = GetStringBoundingBox(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
			size->width = max(size->width, min(300u, width));
			SetDParamStr(0, this->landinfo_data[LAND_INFO_MULTICENTER_LINE]);
			size->height += GetStringHeight(STR_JUST_RAW_STRING, size->width - WD_FRAMETEXT_LEFT - WD_FRAMETEXT_RIGHT);
		}
	}

	LandInfoWindow(TileIndex tile) : Window(), tile(tile)
	{
		this->InitNested(&_land_info_desc);

#if defined(_DEBUG)
#	define LANDINFOD_LEVEL 0
#else
#	define LANDINFOD_LEVEL 1
#endif
		DEBUG(misc, LANDINFOD_LEVEL, "TILE: %#x (%i,%i)", tile, TileX(tile), TileY(tile));
		DEBUG(misc, LANDINFOD_LEVEL, "type_height  = %#x", _m[tile].type_height);
		DEBUG(misc, LANDINFOD_LEVEL, "m1           = %#x", _m[tile].m1);
		DEBUG(misc, LANDINFOD_LEVEL, "m2           = %#x", _m[tile].m2);
		DEBUG(misc, LANDINFOD_LEVEL, "m3           = %#x", _m[tile].m3);
		DEBUG(misc, LANDINFOD_LEVEL, "m4           = %#x", _m[tile].m4);
		DEBUG(misc, LANDINFOD_LEVEL, "m5           = %#x", _m[tile].m5);
		DEBUG(misc, LANDINFOD_LEVEL, "m6           = %#x", _m[tile].m6);
		DEBUG(misc, LANDINFOD_LEVEL, "m7           = %#x", _me[tile].m7);
#undef LANDINFOD_LEVEL
	}

	virtual void OnInit()
	{
		Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

		/* Because build_date is not set yet in every TileDesc, we make sure it is empty */
		TileDesc td;

		td.build_date = INVALID_DATE;

		/* Most tiles have only one owner, but
		 *  - drivethrough roadstops can be build on town owned roads (up to 2 owners) and
		 *  - roads can have up to four owners (railroad, road, tram, 3rd-roadtype "highway").
		 */
		td.owner_type[0] = STR_LAND_AREA_INFORMATION_OWNER; // At least one owner is displayed, though it might be "N/A".
		td.owner_type[1] = STR_NULL;       // STR_NULL results in skipping the owner
		td.owner_type[2] = STR_NULL;
		td.owner_type[3] = STR_NULL;
		td.owner[0] = OWNER_NONE;
		td.owner[1] = OWNER_NONE;
		td.owner[2] = OWNER_NONE;
		td.owner[3] = OWNER_NONE;

		td.station_class = STR_NULL;
		td.station_name = STR_NULL;
		td.airport_class = STR_NULL;
		td.airport_name = STR_NULL;
		td.airport_tile_name = STR_NULL;
		td.rail_speed = 0;

		td.grf = NULL;

		CargoArray acceptance;
		AddAcceptedCargo(tile, acceptance, NULL);
		GetTileDesc(tile, &td);

		uint line_nr = 0;

		/* Tiletype */
		SetDParam(0, td.dparam[0]);
		GetString(this->landinfo_data[line_nr], td.str, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Up to four owners */
		for (uint i = 0; i < 4; i++) {
			if (td.owner_type[i] == STR_NULL) continue;

			SetDParam(0, STR_LAND_AREA_INFORMATION_OWNER_N_A);
			if (td.owner[i] != OWNER_NONE && td.owner[i] != OWNER_WATER) GetNameOfOwner(td.owner[i], tile);
			GetString(this->landinfo_data[line_nr], td.owner_type[i], lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Cost to clear/revenue when cleared */
		StringID str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A;
		Company *c = Company::GetIfValid(_local_company);
		if (c != NULL) {
			Money old_money = c->money;
			c->money = INT64_MAX;
			assert(_current_company == _local_company);
			CommandCost costclear = DoCommand(tile, 0, 0, DC_NONE, CMD_LANDSCAPE_CLEAR);
			c->money = old_money;
			if (costclear.Succeeded()) {
				Money cost = costclear.GetCost();
				if (cost < 0) {
					cost = -cost; // Negate negative cost to a positive revenue
					str = STR_LAND_AREA_INFORMATION_REVENUE_WHEN_CLEARED;
				} else {
					str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR;
				}
				SetDParam(0, cost);
			}
		}
		GetString(this->landinfo_data[line_nr], str, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Location */
		char tmp[16];
		snprintf(tmp, lengthof(tmp), "0x%.4X", tile);
		SetDParam(0, TileX(tile));
		SetDParam(1, TileY(tile));
		SetDParam(2, GetTileZ(tile));
		SetDParamStr(3, tmp);
		GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_LANDINFO_COORDS, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Local authority */
		SetDParam(0, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE);
		if (t != NULL) {
			SetDParam(0, STR_TOWN_NAME);
			SetDParam(1, t->index);
		}
		GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Build date */
		if (td.build_date != INVALID_DATE) {
			SetDParam(0, td.build_date);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_BUILD_DATE, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Station class */
		if (td.station_class != STR_NULL) {
			SetDParam(0, td.station_class);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_STATION_CLASS, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Station type name */
		if (td.station_name != STR_NULL) {
			SetDParam(0, td.station_name);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_STATION_TYPE, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Airport class */
		if (td.airport_class != STR_NULL) {
			SetDParam(0, td.airport_class);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORT_CLASS, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Airport name */
		if (td.airport_name != STR_NULL) {
			SetDParam(0, td.airport_name);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORT_NAME, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Airport tile name */
		if (td.airport_tile_name != STR_NULL) {
			SetDParam(0, td.airport_tile_name);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORTTILE_NAME, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Rail speed limit */
		if (td.rail_speed != 0) {
			SetDParam(0, td.rail_speed);
			GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_RAIL_SPEED_LIMIT, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* NewGRF name */
		if (td.grf != NULL) {
			SetDParamStr(0, td.grf);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_NEWGRF_NAME, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		assert(line_nr < LAND_INFO_CENTERED_LINES);

		/* Mark last line empty */
		this->landinfo_data[line_nr][0] = '\0';

		/* Cargo acceptance is displayed in a extra multiline */
		char *strp = GetString(this->landinfo_data[LAND_INFO_MULTICENTER_LINE], STR_LAND_AREA_INFORMATION_CARGO_ACCEPTED, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
		bool found = false;

		for (CargoID i = 0; i < NUM_CARGO; ++i) {
			if (acceptance[i] > 0) {
				/* Add a comma between each item. */
				if (found) {
					*strp++ = ',';
					*strp++ = ' ';
				}
				found = true;

				/* If the accepted value is less than 8, show it in 1/8:ths */
				if (acceptance[i] < 8) {
					SetDParam(0, acceptance[i]);
					SetDParam(1, CargoSpec::Get(i)->name);
					strp = GetString(strp, STR_LAND_AREA_INFORMATION_CARGO_EIGHTS, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
				} else {
					strp = GetString(strp, CargoSpec::Get(i)->name, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
				}
			}
		}
		if (!found) this->landinfo_data[LAND_INFO_MULTICENTER_LINE][0] = '\0';
	}

	virtual bool IsNewGRFInspectable() const
	{
		return ::IsNewGRFInspectable(GetGrfSpecFeature(this->tile), this->tile);
	}

	virtual void ShowNewGRFInspectWindow() const
	{
		::ShowNewGRFInspectWindow(GetGrfSpecFeature(this->tile), this->tile);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		switch (data) {
			case 1:
				/* ReInit, "debug" sprite might have changed */
				this->ReInit();
				break;
		}
	}
};

/**
 * Show land information window.
 * @param tile The tile to show information about.
 */
void ShowLandInfo(TileIndex tile)
{
	DeleteWindowById(WC_LAND_INFO, 0);
	new LandInfoWindow(tile);
}

static const NWidgetPart _nested_about_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_ABOUT_OPENTTD, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetPIP(4, 2, 4),
		NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_ABOUT_ORIGINAL_COPYRIGHT, STR_NULL),
		NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_ABOUT_VERSION, STR_NULL),
		NWidget(WWT_FRAME, COLOUR_GREY), SetPadding(0, 5, 1, 5),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_A_SCROLLING_TEXT),
		EndContainer(),
		NWidget(WWT_LABEL, COLOUR_GREY, WID_A_WEBSITE), SetDataTip(STR_BLACK_RAW_STRING, STR_NULL),
		NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_ABOUT_COPYRIGHT_OPENTTD, STR_NULL),
	EndContainer(),
};

static const WindowDesc _about_desc(
	WDP_CENTER, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	_nested_about_widgets, lengthof(_nested_about_widgets)
);

static const char * const _credits[] = {
	"Original design by Chris Sawyer",
	"Original graphics by Simon Foster",
	"",
	"The OpenTTD team (in alphabetical order):",
	"  Albert Hofkamp (Alberth) - GUI expert",
	"  Jean-Fran\xC3\xA7ois Claeys (Belugas) - GUI, newindustries and more",
	"  Matthijs Kooijman (blathijs) - Pathfinder-guru, pool rework",
	"  Christoph Elsenhans (frosch) - General coding",
	"  Lo\xC3\xAF""c Guilloux (glx) - Windows Expert",
	"  Michael Lutz (michi_cc) - Path based signals",
	"  Owen Rudge (orudge) - Forum host, OS/2 port",
	"  Peter Nelson (peter1138) - Spiritual descendant from NewGRF gods",
	"  Ingo von Borstel (planetmaker) - Support",
	"  Remko Bijker (Rubidium) - Lead coder and way more",
	"  Zden\xC4\x9Bk Sojka (SmatZ) - Bug finder and fixer",
	"  Jos\xC3\xA9 Soler (Terkhen) - General coding",
	"  Thijs Marinussen (Yexo) - AI Framework",
	"",
	"Inactive Developers:",
	"  Bjarni Corfitzen (Bjarni) - MacOSX port, coder and vehicles",
	"  Victor Fischer (Celestar) - Programming everywhere you need him to",
	"  Tam\xC3\xA1s Farag\xC3\xB3 (Darkvater) - Ex-Lead coder",
	"  Jaroslav Mazanec (KUDr) - YAPG (Yet Another Pathfinder God) ;)",
	"  Jonathan Coome (Maedhros) - High priest of the NewGRF Temple",
	"  Attila B\xC3\xA1n (MiHaMiX) - Developer WebTranslator 1 and 2",
	"  Christoph Mallon (Tron) - Programmer, code correctness police",
	"",
	"Retired Developers:",
	"  Ludvig Strigeus (ludde) - OpenTTD author, main coder (0.1 - 0.3.3)",
	"  Serge Paquet (vurlix) - Assistant project manager, coder (0.1 - 0.3.3)",
	"  Dominik Scherer (dominik81) - Lead programmer, GUI expert (0.3.0 - 0.3.6)",
	"  Benedikt Br\xC3\xBCggemeier (skidd13) - Bug fixer and code reworker",
	"  Patric Stout (TrueBrain) - Programmer (0.3 - pre0.7), sys op (active)",
	"",
	"Special thanks go out to:",
	"  Josef Drexler - For his great work on TTDPatch",
	"  Marcin Grzegorczyk - For describing Transport Tycoon Deluxe internals",
	"  Petr Baudi\xC5\xA1 (pasky) - Many patches, newGRF support",
	"  Stefan Mei\xC3\x9Fner (sign_de) - For his work on the console",
	"  Simon Sasburg (HackyKid) - Many bugfixes he has blessed us with",
	"  Cian Duffy (MYOB) - BeOS port / manual writing",
	"  Christian Rosentreter (tokai) - MorphOS / AmigaOS port",
	"  Richard Kempton (richK) - additional airports, initial TGP implementation",
	"",
	"  Alberto Demichelis - Squirrel scripting language \xC2\xA9 2003-2008",
	"  L. Peter Deutsch - MD5 implementation \xC2\xA9 1999, 2000, 2002",
	"  Michael Blunck - Pre-Signals and Semaphores \xC2\xA9 2003",
	"  George - Canal/Lock graphics \xC2\xA9 2003-2004",
	"  Andrew Parkhouse - River graphics",
	"  David Dallaston - Tram tracks",
	"  Marcin Grzegorczyk - Foundations for Tracks on Slopes",
	"  All Translators - Who made OpenTTD a truly international game",
	"  Bug Reporters - Without whom OpenTTD would still be full of bugs!",
	"",
	"",
	"And last but not least:",
	"  Chris Sawyer - For an amazing game!"
};

struct AboutWindow : public Window {
	int text_position;                       ///< The top of the scrolling text
	byte counter;                            ///< Used to scroll the text every 5 ticks
	int line_height;                         ///< The height of a single line
	static const int num_visible_lines = 19; ///< The number of lines visible simultaneously

	AboutWindow() : Window()
	{
		this->InitNested(&_about_desc, WN_GAME_OPTIONS_ABOUT);

		this->counter = 5;
		this->text_position = this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y + this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->current_y;
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_A_WEBSITE) SetDParamStr(0, "Website: http://www.openttd.org");
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_A_SCROLLING_TEXT) return;

		this->line_height = FONT_HEIGHT_NORMAL;

		Dimension d;
		d.height = this->line_height * num_visible_lines;

		d.width = 0;
		for (uint i = 0; i < lengthof(_credits); i++) {
			d.width = max(d.width, GetStringBoundingBox(_credits[i]).width);
		}
		*size = maxdim(*size, d);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_A_SCROLLING_TEXT) return;

		int y = this->text_position;

		/* Show all scrolling _credits */
		for (uint i = 0; i < lengthof(_credits); i++) {
			if (y >= r.top + 7 && y < r.bottom - this->line_height) {
				DrawString(r.left, r.right, y, _credits[i], TC_BLACK, SA_LEFT | SA_FORCE);
			}
			y += this->line_height;
		}
	}

	virtual void OnTick()
	{
		if (--this->counter == 0) {
			this->counter = 5;
			this->text_position--;
			/* If the last text has scrolled start a new from the start */
			if (this->text_position < (int)(this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y - lengthof(_credits) * this->line_height)) {
				this->text_position = this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y + this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->current_y;
			}
			this->SetDirty();
		}
	}
};

void ShowAboutWindow()
{
	DeleteWindowByClass(WC_GAME_OPTIONS);
	new AboutWindow();
}

/**
 * Display estimated costs.
 * @param cost Estimated cost (or income if negative).
 * @param x    X position of the notification window.
 * @param y    Y position of the notification window.
 */
void ShowEstimatedCostOrIncome(Money cost, int x, int y)
{
	StringID msg = STR_MESSAGE_ESTIMATED_COST;

	if (cost < 0) {
		cost = -cost;
		msg = STR_MESSAGE_ESTIMATED_INCOME;
	}
	SetDParam(0, cost);
	ShowErrorMessage(msg, INVALID_STRING_ID, WL_INFO, x, y);
}

/**
 * Display animated income or costs on the map.
 * @param x    World X position of the animation location.
 * @param y    World Y position of the animation location.
 * @param z    World Z position of the animation location.
 * @param cost Estimated cost (or income if negative).
 */
void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost)
{
	Point pt = RemapCoords(x, y, z);
	StringID msg = STR_INCOME_FLOAT_COST;

	if (cost < 0) {
		cost = -cost;
		msg = STR_INCOME_FLOAT_INCOME;
	}
	SetDParam(0, cost);
	AddTextEffect(msg, pt.x, pt.y, DAY_TICKS, TE_RISING);
}

/**
 * Display animated feeder income.
 * @param x    World X position of the animation location.
 * @param y    World Y position of the animation location.
 * @param z    World Z position of the animation location.
 * @param cost Estimated feeder income.
 */
void ShowFeederIncomeAnimation(int x, int y, int z, Money cost)
{
	Point pt = RemapCoords(x, y, z);

	SetDParam(0, cost);
	AddTextEffect(STR_FEEDER, pt.x, pt.y, DAY_TICKS, TE_RISING);
}

/**
 * Display vehicle loading indicators.
 * @param x       World X position of the animation location.
 * @param y       World Y position of the animation location.
 * @param z       World Z position of the animation location.
 * @param percent Estimated feeder income.
 * @param string  String which is drawn on the map.
 * @return        TextEffectID to be used for future updates of the loading indicators.
 */
TextEffectID ShowFillingPercent(int x, int y, int z, uint8 percent, StringID string)
{
	Point pt = RemapCoords(x, y, z);

	assert(string != STR_NULL);

	SetDParam(0, percent);
	return AddTextEffect(string, pt.x, pt.y, 0, TE_STATIC);
}

/**
 * Update vehicle loading indicators.
 * @param te_id   TextEffectID to be updated.
 * @param string  String wich is printed.
 */
void UpdateFillingPercent(TextEffectID te_id, uint8 percent, StringID string)
{
	assert(string != STR_NULL);

	SetDParam(0, percent);
	UpdateTextEffect(te_id, string);
}

/**
 * Hide vehicle loading indicators.
 * @param *te_id TextEffectID which is supposed to be hidden.
 */
void HideFillingPercent(TextEffectID *te_id)
{
	if (*te_id == INVALID_TE_ID) return;

	RemoveTextEffect(*te_id);
	*te_id = INVALID_TE_ID;
}

static const NWidgetPart _nested_tooltips_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_GREY, WID_TT_BACKGROUND), SetMinimalSize(200, 32), EndContainer(),
};

static const WindowDesc _tool_tips_desc(
	WDP_MANUAL, 0, 0, // Coordinates and sizes are not used,
	WC_TOOLTIPS, WC_NONE,
	0,
	_nested_tooltips_widgets, lengthof(_nested_tooltips_widgets)
);

/** Window for displaying a tooltip. */
struct TooltipsWindow : public Window
{
	StringID string_id;               ///< String to display as tooltip.
	byte paramcount;                  ///< Number of string parameters in #string_id.
	uint64 params[5];                 ///< The string parameters.
	TooltipCloseCondition close_cond; ///< Condition for closing the window.

	TooltipsWindow(Window *parent, StringID str, uint paramcount, const uint64 params[], TooltipCloseCondition close_tooltip) : Window()
	{
		this->parent = parent;
		this->string_id = str;
		assert_compile(sizeof(this->params[0]) == sizeof(params[0]));
		assert(paramcount <= lengthof(this->params));
		memcpy(this->params, params, sizeof(this->params[0]) * paramcount);
		this->paramcount = paramcount;
		this->close_cond = close_tooltip;

		this->InitNested(&_tool_tips_desc);

		CLRBITS(this->flags, WF_WHITE_BORDER);
	}

	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		/* Find the free screen space between the main toolbar at the top, and the statusbar at the bottom.
		 * Add a fixed distance 2 so the tooltip floats free from both bars.
		 */
		int scr_top = GetMainViewTop() + 2;
		int scr_bot = GetMainViewBottom() - 2;

		Point pt;

		/* Correctly position the tooltip position, watch out for window and cursor size
		 * Clamp value to below main toolbar and above statusbar. If tooltip would
		 * go below window, flip it so it is shown above the cursor */
		pt.y = Clamp(_cursor.pos.y + _cursor.size.y + _cursor.offs.y + 5, scr_top, scr_bot);
		if (pt.y + sm_height > scr_bot) pt.y = min(_cursor.pos.y + _cursor.offs.y - 5, scr_bot) - sm_height;
		pt.x = sm_width >= _screen.width ? 0 : Clamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);

		return pt;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		/* There is only one widget. */
		for (uint i = 0; i != this->paramcount; i++) SetDParam(i, this->params[i]);

		size->width  = min(GetStringBoundingBox(this->string_id).width, 194);
		size->height = GetStringHeight(this->string_id, size->width);

		/* Increase slightly to have some space around the box. */
		size->width  += 2 + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
		size->height += 2 + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		/* There is only one widget. */
		GfxFillRect(r.left, r.top, r.right, r.bottom, PC_BLACK);
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, PC_LIGHT_YELLOW);

		for (uint arg = 0; arg < this->paramcount; arg++) {
			SetDParam(arg, this->params[arg]);
		}
		DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM, this->string_id, TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnMouseLoop()
	{
		/* Always close tooltips when the cursor is not in our window. */
		if (!_cursor.in_window) {
			delete this;
			return;
		}

		/* We can show tooltips while dragging tools. These are shown as long as
		 * we are dragging the tool. Normal tooltips work with hover or rmb. */
		switch (this->close_cond) {
			case TCC_RIGHT_CLICK: if (!_right_button_down) delete this; break;
			case TCC_LEFT_CLICK: if (!_left_button_down) delete this; break;
			case TCC_HOVER: if (!_mouse_hovering) delete this; break;
		}
	}
};

/**
 * Shows a tooltip
 * @param parent The window this tooltip is related to.
 * @param str String to be displayed
 * @param paramcount number of params to deal with
 * @param params (optional) up to 5 pieces of additional information that may be added to a tooltip
 * @param use_left_mouse_button close the tooltip when the left (true) or right (false) mousebutton is released
 */
void GuiShowTooltips(Window *parent, StringID str, uint paramcount, const uint64 params[], TooltipCloseCondition close_tooltip)
{
	DeleteWindowById(WC_TOOLTIPS, 0);

	if (str == STR_NULL) return;

	new TooltipsWindow(parent, str, paramcount, params, close_tooltip);
}

/* Delete a character at the caret position in a text buf.
 * If backspace is set, delete the character before the caret,
 * else delete the character after it. */
static void DelChar(Textbuf *tb, bool backspace)
{
	WChar c;
	char *s = tb->buf + tb->caretpos;

	if (backspace) s = Utf8PrevChar(s);

	uint16 len = (uint16)Utf8Decode(&c, s);
	uint width = GetCharacterWidth(FS_NORMAL, c);

	tb->pixels -= width;
	if (backspace) {
		tb->caretpos   -= len;
		tb->caretxoffs -= width;
	}

	/* Move the remaining characters over the marker */
	memmove(s, s + len, tb->bytes - (s - tb->buf) - len);
	tb->bytes -= len;
	tb->chars--;
}

/**
 * Delete a character from a textbuffer, either with 'Delete' or 'Backspace'
 * The character is delete from the position the caret is at
 * @param tb Textbuf type to be changed
 * @param delmode Type of deletion, either WKC_BACKSPACE or WKC_DELETE
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool DeleteTextBufferChar(Textbuf *tb, int delmode)
{
	if (delmode == WKC_BACKSPACE && tb->caretpos != 0) {
		DelChar(tb, true);
		return true;
	} else if (delmode == WKC_DELETE && tb->caretpos < tb->bytes - 1) {
		DelChar(tb, false);
		return true;
	}

	return false;
}

/**
 * Delete every character in the textbuffer
 * @param tb Textbuf buffer to be emptied
 */
void DeleteTextBufferAll(Textbuf *tb)
{
	memset(tb->buf, 0, tb->max_bytes);
	tb->bytes = tb->chars = 1;
	tb->pixels = tb->caretpos = tb->caretxoffs = 0;
}

/**
 * Insert a character to a textbuffer. If maxwidth of the Textbuf is zero,
 * we don't care about the visual-length but only about the physical
 * length of the string
 * @param tb Textbuf type to be changed
 * @param key Character to be inserted
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool InsertTextBufferChar(Textbuf *tb, WChar key)
{
	const byte charwidth = GetCharacterWidth(FS_NORMAL, key);
	uint16 len = (uint16)Utf8CharLen(key);
	if (tb->bytes + len <= tb->max_bytes && tb->chars + 1 <= tb->max_chars) {
		memmove(tb->buf + tb->caretpos + len, tb->buf + tb->caretpos, tb->bytes - tb->caretpos);
		Utf8Encode(tb->buf + tb->caretpos, key);
		tb->chars++;
		tb->bytes  += len;
		tb->pixels += charwidth;

		tb->caretpos   += len;
		tb->caretxoffs += charwidth;
		return true;
	}
	return false;
}

/**
 * Insert a chunk of text from the clipboard onto the textbuffer. Get TEXT clipboard
 * and append this up to the maximum length (either absolute or screenlength). If maxlength
 * is zero, we don't care about the screenlength but only about the physical length of the string
 * @param tb Textbuf type to be changed
 * @return true on successful change of Textbuf, or false otherwise
 */
bool InsertTextBufferClipboard(Textbuf *tb)
{
	char utf8_buf[512];

	if (!GetClipboardContents(utf8_buf, lengthof(utf8_buf))) return false;

	uint16 pixels = 0, bytes = 0, chars = 0;
	WChar c;
	for (const char *ptr = utf8_buf; (c = Utf8Consume(&ptr)) != '\0';) {
		if (!IsPrintable(c)) break;

		byte len = Utf8CharLen(c);
		if (tb->bytes + bytes + len > tb->max_bytes) break;
		if (tb->chars + chars + 1   > tb->max_chars) break;

		byte char_pixels = GetCharacterWidth(FS_NORMAL, c);

		pixels += char_pixels;
		bytes += len;
		chars++;
	}

	if (bytes == 0) return false;

	memmove(tb->buf + tb->caretpos + bytes, tb->buf + tb->caretpos, tb->bytes - tb->caretpos);
	memcpy(tb->buf + tb->caretpos, utf8_buf, bytes);
	tb->pixels += pixels;
	tb->caretxoffs += pixels;

	tb->bytes += bytes;
	tb->chars += chars;
	tb->caretpos += bytes;
	assert(tb->bytes <= tb->max_bytes);
	assert(tb->chars <= tb->max_chars);
	tb->buf[tb->bytes - 1] = '\0'; // terminating zero

	return true;
}

/**
 * Handle text navigation with arrow keys left/right.
 * This defines where the caret will blink and the next characer interaction will occur
 * @param tb Textbuf type where navigation occurs
 * @param navmode Direction in which navigation occurs WKC_LEFT, WKC_RIGHT, WKC_END, WKC_HOME
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool MoveTextBufferPos(Textbuf *tb, int navmode)
{
	switch (navmode) {
		case WKC_LEFT:
			if (tb->caretpos != 0) {
				WChar c;
				const char *s = Utf8PrevChar(tb->buf + tb->caretpos);
				Utf8Decode(&c, s);
				tb->caretpos    = s - tb->buf; // -= (tb->buf + tb->caretpos - s)
				tb->caretxoffs -= GetCharacterWidth(FS_NORMAL, c);

				return true;
			}
			break;

		case WKC_RIGHT:
			if (tb->caretpos < tb->bytes - 1) {
				WChar c;

				tb->caretpos   += (uint16)Utf8Decode(&c, tb->buf + tb->caretpos);
				tb->caretxoffs += GetCharacterWidth(FS_NORMAL, c);

				return true;
			}
			break;

		case WKC_HOME:
			tb->caretpos = 0;
			tb->caretxoffs = 0;
			return true;

		case WKC_END:
			tb->caretpos = tb->bytes - 1;
			tb->caretxoffs = tb->pixels;
			return true;

		default:
			break;
	}

	return false;
}

/**
 * Initialize the textbuffer by supplying it the buffer to write into
 * and the maximum length of this buffer
 * @param tb Textbuf type which is getting initialized
 * @param buf the buffer that will be holding the data for input
 * @param max_bytes maximum size in bytes, including terminating '\0'
 */
void InitializeTextBuffer(Textbuf *tb, char *buf, uint16 max_bytes)
{
	InitializeTextBuffer(tb, buf, max_bytes, max_bytes);
}

/**
 * Initialize the textbuffer by supplying it the buffer to write into
 * and the maximum length of this buffer
 * @param tb Textbuf type which is getting initialized
 * @param buf the buffer that will be holding the data for input
 * @param max_bytes maximum size in bytes, including terminating '\0'
 * @param max_chars maximum size in chars, including terminating '\0'
 */
void InitializeTextBuffer(Textbuf *tb, char *buf, uint16 max_bytes, uint16 max_chars)
{
	assert(max_bytes != 0);
	assert(max_chars != 0);

	tb->buf        = buf;
	tb->max_bytes  = max_bytes;
	tb->max_chars  = max_chars;
	tb->caret      = true;
	UpdateTextBufferSize(tb);
}

/**
 * Update Textbuf type with its actual physical character and screenlength
 * Get the count of characters in the string as well as the width in pixels.
 * Useful when copying in a larger amount of text at once
 * @param tb Textbuf type which length is calculated
 */
void UpdateTextBufferSize(Textbuf *tb)
{
	const char *buf = tb->buf;

	tb->pixels = 0;
	tb->chars = tb->bytes = 1; // terminating zero

	WChar c;
	while ((c = Utf8Consume(&buf)) != '\0') {
		tb->pixels += GetCharacterWidth(FS_NORMAL, c);
		tb->bytes += Utf8CharLen(c);
		tb->chars++;
	}

	assert(tb->bytes <= tb->max_bytes);
	assert(tb->chars <= tb->max_chars);

	tb->caretpos = tb->bytes - 1;
	tb->caretxoffs = tb->pixels;
}

/**
 * Handle the flashing of the caret.
 * @param tb The text buffer to handle the caret of.
 * @return True if the caret state changes.
 */
bool HandleCaret(Textbuf *tb)
{
	/* caret changed? */
	bool b = !!(_caret_timer & 0x20);

	if (b != tb->caret) {
		tb->caret = b;
		return true;
	}
	return false;
}

bool QueryString::HasEditBoxFocus(const Window *w, int wid) const
{
	if (w->IsWidgetGloballyFocused(wid)) return true;
	if (w->window_class != WC_OSK || _focused_window != w->parent) return false;
	return w->parent->nested_focus != NULL && w->parent->nested_focus->type == WWT_EDITBOX;
}

HandleEditBoxResult QueryString::HandleEditBoxKey(Window *w, int wid, uint16 key, uint16 keycode, EventState &state)
{
	if (!QueryString::HasEditBoxFocus(w, wid)) return HEBR_NOT_FOCUSED;

	state = ES_HANDLED;

	switch (keycode) {
		case WKC_ESC: return HEBR_CANCEL;

		case WKC_RETURN: case WKC_NUM_ENTER: return HEBR_CONFIRM;

#ifdef WITH_COCOA
		case (WKC_META | 'V'):
#endif
		case (WKC_CTRL | 'V'):
			if (InsertTextBufferClipboard(&this->text)) w->SetWidgetDirty(wid);
			break;

#ifdef WITH_COCOA
		case (WKC_META | 'U'):
#endif
		case (WKC_CTRL | 'U'):
			DeleteTextBufferAll(&this->text);
			w->SetWidgetDirty(wid);
			break;

		case WKC_BACKSPACE: case WKC_DELETE:
			if (DeleteTextBufferChar(&this->text, keycode)) w->SetWidgetDirty(wid);
			break;

		case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
			if (MoveTextBufferPos(&this->text, keycode)) w->SetWidgetDirty(wid);
			break;

		default:
			if (IsValidChar(key, this->afilter)) {
				if (InsertTextBufferChar(&this->text, key)) w->SetWidgetDirty(wid);
			} else {
				state = ES_NOT_HANDLED;
			}
	}

	return HEBR_EDITING;
}

void QueryString::HandleEditBox(Window *w, int wid)
{
	if (HasEditBoxFocus(w, wid) && HandleCaret(&this->text)) {
		w->SetWidgetDirty(wid);
		/* When we're not the OSK, notify 'our' OSK to redraw the widget,
		 * so the caret changes appropriately. */
		if (w->window_class != WC_OSK) {
			Window *w_osk = FindWindowById(WC_OSK, 0);
			if (w_osk != NULL && w_osk->parent == w) w_osk->InvalidateData();
		}
	}
}

void QueryString::DrawEditBox(Window *w, int wid)
{
	const NWidgetBase *wi = w->GetWidget<NWidgetBase>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);
	int left   = wi->pos_x;
	int right  = wi->pos_x + wi->current_x - 1;
	int top    = wi->pos_y;
	int bottom = wi->pos_y + wi->current_y - 1;

	GfxFillRect(left + 1, top + 1, right - 1, bottom - 1, PC_BLACK);

	/* Limit the drawing of the string inside the widget boundaries */
	DrawPixelInfo dpi;
	if (!FillDrawPixelInfo(&dpi, left + WD_FRAMERECT_LEFT, top + WD_FRAMERECT_TOP, right - left - WD_FRAMERECT_RIGHT, bottom - top - WD_FRAMERECT_BOTTOM)) return;

	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &dpi;

	/* We will take the current widget length as maximum width, with a small
	 * space reserved at the end for the caret to show */
	const Textbuf *tb = &this->text;
	int delta = min(0, (right - left) - tb->pixels - 10);

	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	DrawString(delta, tb->pixels, 0, tb->buf, TC_YELLOW);
	if (HasEditBoxFocus(w, wid) && tb->caret) {
		int caret_width = GetStringBoundingBox("_").width;
		DrawString(tb->caretxoffs + delta, tb->caretxoffs + delta + caret_width, 0, "_", TC_WHITE);
	}

	_cur_dpi = old_dpi;
}

HandleEditBoxResult QueryStringBaseWindow::HandleEditBoxKey(int wid, uint16 key, uint16 keycode, EventState &state)
{
	return this->QueryString::HandleEditBoxKey(this, wid, key, keycode, state);
}

void QueryStringBaseWindow::HandleEditBox(int wid)
{
	this->QueryString::HandleEditBox(this, wid);
}

void QueryStringBaseWindow::DrawEditBox(int wid)
{
	this->QueryString::DrawEditBox(this, wid);
}

void QueryStringBaseWindow::OnOpenOSKWindow(int wid)
{
	ShowOnScreenKeyboard(this, wid, 0, 0);
}

/** Class for the string query window. */
struct QueryStringWindow : public QueryStringBaseWindow
{
	QueryStringFlags flags; ///< Flags controlling behaviour of the window.

	QueryStringWindow(StringID str, StringID caption, uint max_bytes, uint max_chars, const WindowDesc *desc, Window *parent, CharSetFilter afilter, QueryStringFlags flags) :
			QueryStringBaseWindow(max_bytes, max_chars)
	{
		GetString(this->edit_str_buf, str, &this->edit_str_buf[max_bytes - 1]);
		str_validate(this->edit_str_buf, &this->edit_str_buf[max_bytes - 1], SVS_NONE);

		/* Make sure the name isn't too long for the text buffer in the number of
		 * characters (not bytes). max_chars also counts the '\0' characters. */
		while (Utf8StringLength(this->edit_str_buf) + 1 > max_chars) {
			*Utf8PrevChar(this->edit_str_buf + strlen(this->edit_str_buf)) = '\0';
		}

		if ((flags & QSF_ACCEPT_UNCHANGED) == 0) this->orig = strdup(this->edit_str_buf);

		this->caption = caption;
		this->afilter = afilter;
		this->flags = flags;
		InitializeTextBuffer(&this->text, this->edit_str_buf, max_bytes, max_chars);

		this->InitNested(desc, WN_QUERY_STRING);

		this->parent = parent;

		this->SetFocusedWidget(WID_QS_TEXT);
		this->LowerWidget(WID_QS_TEXT);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_QS_DEFAULT && (this->flags & QSF_ENABLE_DEFAULT) == 0) {
			/* We don't want this widget to show! */
			fill->width = 0;
			resize->width = 0;
			size->width = 0;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		this->DrawEditBox(WID_QS_TEXT);
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_QS_CAPTION) SetDParam(0, this->caption);
	}

	void OnOk()
	{
		if (this->orig == NULL || strcmp(this->text.buf, this->orig) != 0) {
			/* If the parent is NULL, the editbox is handled by general function
			 * HandleOnEditText */
			if (this->parent != NULL) {
				this->parent->OnQueryTextFinished(this->text.buf);
			} else {
				HandleOnEditText(this->text.buf);
			}
			this->handled = true;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_QS_DEFAULT:
				this->text.buf[0] = '\0';
				/* FALL THROUGH */
			case WID_QS_OK:
				this->OnOk();
				/* FALL THROUGH */
			case WID_QS_CANCEL:
				delete this;
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(WID_QS_TEXT);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		switch (this->HandleEditBoxKey(WID_QS_TEXT, key, keycode, state)) {
			default: NOT_REACHED();
			case HEBR_EDITING: {
				Window *osk = FindWindowById(WC_OSK, 0);
				if (osk != NULL && osk->parent == this) osk->InvalidateData();
				break;
			}
			case HEBR_CONFIRM: this->OnOk();
				/* FALL THROUGH */
			case HEBR_CANCEL: delete this; break; // close window, abandon changes
			case HEBR_NOT_FOCUSED: break;
		}
		return state;
	}

	virtual void OnOpenOSKWindow(int wid)
	{
		ShowOnScreenKeyboard(this, wid, WID_QS_CANCEL, WID_QS_OK);
	}

	~QueryStringWindow()
	{
		if (!this->handled && this->parent != NULL) {
			Window *parent = this->parent;
			this->parent = NULL; // so parent doesn't try to delete us again
			parent->OnQueryTextFinished(NULL);
		}
	}
};

static const NWidgetPart _nested_query_string_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_QS_CAPTION), SetDataTip(STR_WHITE_STRING, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_QS_TEXT), SetMinimalSize(256, 12), SetFill(1, 1), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_DEFAULT), SetMinimalSize(87, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_DEFAULT, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_CANCEL), SetMinimalSize(86, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_OK), SetMinimalSize(87, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_OK, STR_NULL),
	EndContainer(),
};

static const WindowDesc _query_string_desc(
	WDP_CENTER, 0, 0,
	WC_QUERY_STRING, WC_NONE,
	0,
	_nested_query_string_widgets, lengthof(_nested_query_string_widgets)
);

/**
 * Show a query popup window with a textbox in it.
 * @param str StringID for the text shown in the textbox
 * @param caption StringID of text shown in caption of querywindow
 * @param maxsize maximum size in bytes or characters (including terminating '\0') depending on flags
 * @param parent pointer to a Window that will handle the events (ok/cancel) of this
 *        window. If NULL, results are handled by global function HandleOnEditText
 * @param afilter filters out unwanted character input
 * @param flags various flags, @see QueryStringFlags
 */
void ShowQueryString(StringID str, StringID caption, uint maxsize, Window *parent, CharSetFilter afilter, QueryStringFlags flags)
{
	DeleteWindowByClass(WC_QUERY_STRING);
	new QueryStringWindow(str, caption, ((flags & QSF_LEN_IN_CHARS) ? MAX_CHAR_LENGTH : 1) * maxsize, maxsize, &_query_string_desc, parent, afilter, flags);
}

/**
 * Window used for asking the user a YES/NO question.
 */
struct QueryWindow : public Window {
	QueryCallbackProc *proc; ///< callback function executed on closing of popup. Window* points to parent, bool is true if 'yes' clicked, false otherwise
	uint64 params[10];       ///< local copy of _decode_parameters
	StringID message;        ///< message shown for query window
	StringID caption;        ///< title of window

	QueryWindow(const WindowDesc *desc, StringID caption, StringID message, Window *parent, QueryCallbackProc *callback) : Window()
	{
		/* Create a backup of the variadic arguments to strings because it will be
		 * overridden pretty often. We will copy these back for drawing */
		CopyOutDParam(this->params, 0, lengthof(this->params));
		this->caption = caption;
		this->message = message;
		this->proc    = callback;

		this->InitNested(desc, WN_CONFIRM_POPUP_QUERY);

		this->parent = parent;
		this->left = parent->left + (parent->width / 2) - (this->width / 2);
		this->top = parent->top + (parent->height / 2) - (this->height / 2);
	}

	~QueryWindow()
	{
		if (this->proc != NULL) this->proc(this->parent, false);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_Q_CAPTION:
				CopyInDParam(1, this->params, lengthof(this->params));
				SetDParam(0, this->caption);
				break;

			case WID_Q_TEXT:
				CopyInDParam(0, this->params, lengthof(this->params));
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_Q_TEXT) return;

		Dimension d = GetStringMultiLineBoundingBox(this->message, *size);
		d.width += padding.width;
		d.height += padding.height;
		*size = d;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_Q_TEXT) return;

		DrawStringMultiLine(r.left, r.right, r.top, r.bottom, this->message, TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_Q_YES: {
				/* in the Generate New World window, clicking 'Yes' causes
				 * DeleteNonVitalWindows() to be called - we shouldn't be in a window then */
				QueryCallbackProc *proc = this->proc;
				Window *parent = this->parent;
				/* Prevent the destructor calling the callback function */
				this->proc = NULL;
				delete this;
				if (proc != NULL) {
					proc(parent, true);
					proc = NULL;
				}
				break;
			}
			case WID_Q_NO:
				delete this;
				break;
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		/* ESC closes the window, Enter confirms the action */
		switch (keycode) {
			case WKC_RETURN:
			case WKC_NUM_ENTER:
				if (this->proc != NULL) {
					this->proc(this->parent, true);
					this->proc = NULL;
				}
				/* FALL THROUGH */
			case WKC_ESC:
				delete this;
				return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}
};

static const NWidgetPart _nested_query_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_Q_CAPTION), SetDataTip(STR_JUST_STRING, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED), SetPIP(8, 15, 8),
		NWidget(WWT_TEXT, COLOUR_RED, WID_Q_TEXT), SetMinimalSize(200, 12),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(20, 29, 20),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_NO), SetMinimalSize(71, 12), SetDataTip(STR_QUIT_NO, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_YES), SetMinimalSize(71, 12), SetDataTip(STR_QUIT_YES, STR_NULL),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _query_desc(
	WDP_CENTER, 0, 0,
	WC_CONFIRM_POPUP_QUERY, WC_NONE,
	WDF_UNCLICK_BUTTONS | WDF_MODAL,
	_nested_query_widgets, lengthof(_nested_query_widgets)
);

/**
 * Show a modal confirmation window with standard 'yes' and 'no' buttons
 * The window is aligned to the centre of its parent.
 * @param caption string shown as window caption
 * @param message string that will be shown for the window
 * @param parent pointer to parent window, if this pointer is NULL the parent becomes
 * the main window WC_MAIN_WINDOW
 * @param callback callback function pointer to set in the window descriptor
 */
void ShowQuery(StringID caption, StringID message, Window *parent, QueryCallbackProc *callback)
{
	if (parent == NULL) parent = FindWindowById(WC_MAIN_WINDOW, 0);

	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class != WC_CONFIRM_POPUP_QUERY) continue;

		const QueryWindow *qw = (const QueryWindow *)w;
		if (qw->parent != parent || qw->proc != callback) continue;

		delete qw;
		break;
	}

	new QueryWindow(&_query_desc, caption, message, parent, callback);
}
