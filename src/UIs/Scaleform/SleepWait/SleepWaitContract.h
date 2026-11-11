#pragma once

//The wait menu (Interface/sleepwaitmenu.swf, the engine's "Sleep/Wait Menu") and its weather picture.
//
//The movie's SleepWaitMenu_mc holds a clip, WeatherBG, with one picture per weather classification on frames 2 to 5
//and nothing on frame 1. It stays hidden until SetWeather(n) is called, then shows picture n and fades it in.
//A value outside 0..3 hides it again.
namespace SleepWait::SleepWaitContract {
	namespace AsPath {
		inline constexpr const char* WeatherClip = "_root.SleepWaitMenu_mc.WeatherBG";
		inline constexpr const char* SetWeather = "_root.SleepWaitMenu_mc.WeatherBG.SetWeather";
	}

	//SetWeather's argument, numbered as Papyrus Weather.GetClassification numbers them.
	enum class Art : int {
		Clear = 0,   //TESWeather pleasant
		Cloudy = 1,
		Rain = 2,
		Snow = 3,
	};
}
