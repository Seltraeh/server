#pragma once

/*!
* Server configuration.
*/
struct ServerConfig
{
	/*!
	* Initial level.
	*/
	uint32_t initialLevel;

	/*!
	* Initial ZEL.
	*/
	uint32_t initialZel;

	/*!
	* Initial Karma.
	*/
	uint32_t initialKarma;

	/*!
	* Initial brave coins.
	*/
	uint32_t initialBraveCoins;

	/*!
	* Frame-rate cap delivered to the offline-proxy client at startup. 0
	* disables the client-side cap. Read from plugins[0].config.server.fps_cap
	* in deploy/config.json; defaults to 60 if absent.
	*/
	uint32_t fpsCap;

	/*!
	* When true, Saturday and Sunday open every Vortex weekday dungeon instead
	* of only the weekend one.
	*
	* Defaults to FALSE, which is what the shipped MST describes: exactly one
	* dungeon carries a weekend banner (100300 "Garden of God"), and the
	* Mon-Fri dungeons carry only their own day. Player recollection of the
	* live game is that weekends opened everything; no evidence for that
	* survives in the data we hold, so it is a switch rather than the default.
	* Read from plugins[0].config.server.vortex_weekend_opens_all.
	*/
	bool vortexWeekendOpensAll;

};
