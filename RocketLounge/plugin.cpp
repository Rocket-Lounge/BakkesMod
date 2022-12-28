#include "pch.h"
#include "cvar.h"
#include "match.h"
#include "plugin.h"
#include "global.h"
#include "logging.h"
#include "overlay.h"
#include "timer.h"
#include "version.h"
#include <imgui_stdlib.h> // ImGui::InputText
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;

const char * PluginName = "Rocket Lounge"; // To change DLL filename use <TargetName> in *.vcxproj
constexpr auto PluginVersion = stringify(V_MAJOR) "." stringify(V_MINOR) "." stringify(V_PATCH) "." stringify(V_BUILD);
BAKKESMOD_PLUGIN(RocketLounge, PluginName, PluginVersion, PLUGINTYPE_FREEPLAY);

map<string, string> MapL1, MapL2, MapL3;
map<string, string> MapI1, MapI2, MapI3, MapI4, MapI5, MapI6, MapI7, MapI8, MapI9, MapI10, MapI11, MapI12;
void RocketLounge::onLoad()
{
	Global::GameWrapper = gameWrapper;
	Global::CvarManager = cvarManager;

	Log::SetPrintLevel(Log::Level::Info);
	Log::SetWriteLevel(Log::Level::Info);

	new Cvar("api_host", "http://localhost:8080");

	// Auto connect API if we can...
	int HALF_SECOND = 500; // I really hate magic numbers
	setTimeout([=](){ this->SioConnect(); }, HALF_SECOND);
	
	map<string, void (RocketLounge::*)(ServerWrapper c, void *p, string e)> ListenerMap =
	{
		{ "Function Engine.GameViewportClient.Tick", &RocketLounge::onTick },
	};
	for (auto const& [eventName, eventListener] : ListenerMap)
	{
		gameWrapper->HookEventWithCaller<ServerWrapper>(eventName, bind(
			eventListener, this, placeholders::_1, placeholders::_2, placeholders::_3
		));
	}
}

void RocketLounge::onUnload()
{
	
}

void RocketLounge::RenderSettings()
{

	ImGui::NewLine();
	
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server.IsNull())
	{
		for(auto p : server.GetPlayers())
		{
			auto priw = p.GetPlayerReplicationInfo();
			if (ImGui::Button(priw.GetPlayerName().ToString().c_str()))
			{
				Log::Info("Removing " + priw.GetPlayerName().ToString());
				server.RemovePlayer(p);
			}
		}
		for(auto pri : server.GetPRIs())
		{
			if (ImGui::Button(pri.GetPlayerName().ToString().c_str()))
			{
				Log::Info("Removing " + pri.GetPlayerName().ToString());
				server.RemovePRI(pri);
			}
		}
	}
	if (ImGui::Button(" \n\t\tSpawn Car\t\t\n "))
	{
		gameWrapper->Execute([this](...){
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server.IsNull())
			{
				Log::Info("Spawning car...");
				server.SpawnBot(23, "CatFacts");
			}
			else
			{
				Log::Error("Cannot spawn car without active server");
			}
		});
	}
	if (ImGui::Button(" \n\t\tSpawn Ball\t\t\n "))
	{
		gameWrapper->Execute([this](...){
			ServerWrapper server = gameWrapper->GetGameEventAsServer();
			if (!server.IsNull())
			{
				Log::Info("Spawning ball...");
				server.SpawnBall(Vector{0, 0, 1000}, true, false);
			}
			else
			{
				Log::Error("Cannot spawn ball without active server");
			}
		});
	}
	ImGui::NewLine();

	
	if (this->SioConnected)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,255,0,255));
		ImGui::Text("  API Connected  ");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::Text("\t\t\t\t\t\t\t\t\t");
		ImGui::SameLine();
		ImGui::Text("\t\t\t\t\t\t\t\t\t");
		ImGui::SameLine();
		if (ImGui::Button("   Disconnect   "))
		{
			this->SioDisconnect();
		}
		ImGui::Spacing();
	}
	else
	{
		Cvar::Get("api_host")->RenderLargeInput(" API Host ", 160);
		ImGui::NewLine();
		ImGui::Text("  ");
		ImGui::SameLine();
		if (ImGui::Button("    Connect    "))
		{
			this->SioConnect();
		}
		ImGui::NewLine();
	}

	// Cvar::Get("show_platform")->RenderCheckbox("Show Platform");
	
	// Padding in case things get close to the bottom and become difficult to read...
	ImGui::NewLine();
	ImGui::NewLine();
	ImGui::NewLine();
}
// More settings rendering boilerplate
string RocketLounge::GetPluginName() { return string(PluginName); }
void RocketLounge::SetImGuiContext(uintptr_t ctx) { ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx)); }

void RocketLounge::onTick(ServerWrapper caller, void* params, string eventName)
{
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!server.IsNull())
	{
		auto pris = server.GetPRIs();
		auto myPri = pris.Get(0); if (myPri.IsNull()) return;
		auto myCar = myPri.GetCar(); if (myCar.IsNull()) return;
		auto myInput = myCar.GetInput();
		auto myLocation = myCar.GetLocation();
		string myName = myPri.GetPlayerName().ToString();
		auto platId = myPri.GetUniqueIdWrapper().GetPlatform();
		string myPlatform = platId == OnlinePlatform_Steam ? "steam" : platId == OnlinePlatform_Epic ? "epic" : "console";
		string mySlug = myPlatform + "/" + myPlatform == "steam" ? to_string(myPri.GetUniqueId().ID) : myName;

		sio::message::list self(mySlug);
		self.push(sio::string_message::create(to_string(myLocation.X)));
		self.push(sio::string_message::create(to_string(myLocation.Y)));
		self.push(sio::string_message::create(to_string(myLocation.Z)));
		
		self.push(sio::string_message::create(to_string(myInput.Throttle)));
		self.push(sio::string_message::create(to_string(myInput.Steer)));
		self.push(sio::string_message::create(to_string(myInput.Pitch)));
		self.push(sio::string_message::create(to_string(myInput.Yaw)));
		self.push(sio::string_message::create(to_string(myInput.Roll)));
		self.push(sio::string_message::create(to_string(myInput.DodgeForward)));
		self.push(sio::string_message::create(to_string(myInput.DodgeStrafe)));
		self.push(sio::string_message::create(to_string(myInput.Handbrake)));
		self.push(sio::string_message::create(to_string(myInput.Jump)));
		self.push(sio::string_message::create(to_string(myInput.ActivateBoost)));
		self.push(sio::string_message::create(to_string(myInput.HoldingBoost)));
		self.push(sio::string_message::create(to_string(myInput.Jumped)));

		this->SioEmit("self", self);

		// need to do ball[0] as well

		if (pris.Count() < 2) return;
		auto botPri = pris.Get(1);
		auto botCar = botPri.GetCar();
		if (botCar.IsNull()) return;
		string botName = botPri.GetPlayerName().ToString();
		if (MapL1.count(botName))
		{
			// Log::Info(
			// 	botName + " Location" + "\n" +
			// 	"X: " + MapL1[botName] + "\n" +
			// 	"Y: " + MapL2[botName] + "\n" +
			// 	"Z: " + MapL3[botName] + "\n"
			// );
			botCar.SetLocation({ stof(MapL1[botName]), stof(MapL2[botName]), stof(MapL3[botName]) });
			
			// Log::Info(
			// 	botName + " Input" + "\n" +
			// 	"Throttle: " + MapI1[botName] + "\n" +
			// 	"Steer: " + MapI2[botName] + "\n" +
			// 	"Pitch: " + MapI3[botName] + "\n" +
			// 	"Yaw: " + MapI4[botName] + "\n" +
			// 	"Roll: " + MapI5[botName] + "\n" +
			// 	"DodgeForward: " + MapI6[botName] + "\n" +
			// 	"DodgeStrafe: " + MapI7[botName] + "\n" +
			// 	"Handbrake : " + MapI8[botName] + "\n" +
			// 	"Jump: " + MapI9[botName] + "\n" +
			// 	"ActivateBoost: " + MapI10[botName] + "\n" +
			// 	"HoldingBoost: " + MapI11[botName] + "\n" +
			// 	"Jumped: " + MapI12[botName] + "\n"
			// );
			botCar.SetInput({
				stof(MapI1[botName]),
				stof(MapI2[botName]),
				stof(MapI3[botName]),
				stof(MapI4[botName]),
				stof(MapI5[botName]),
				stof(MapI6[botName]),
				stof(MapI7[botName]),
				stoul(MapI8[botName]),
				stoul(MapI9[botName]),
				stoul(MapI10[botName]),
				1, // stoul(MapI11[botName]),
				1, // stoul(MapI12[botName]),
			});
		}
				
	}
}


void RocketLounge::SioDisconnect()
{
    this->io.close();
}

void RocketLounge::SioConnect()
{
    string apiHost = Cvar::Get("api_host")->toString();
	if (!apiHost.length())
    {
        Global::Notify::Info("Cannot Connect", "API Host is required you dumb shit");
		return;
	}
	// map<string, string> handshake { { "key", apiKey } };
	// this->io.connect(apiHost, handshake);
	this->io.connect(apiHost);
	this->io.set_open_listener([&]() {
		this->SioConnected = true;
        Global::Notify::Success("API Connected", "You're good to go!");
	});
	this->io.set_close_listener([&](sio::client::close_reason const& reason) {
		this->SioConnected = false;
		string msg = reason == sio::client::close_reason::close_reason_normal
			? "Your connection was closed"
			: "Your connection was dropped";
        Global::Notify::Error("API Disconnected", msg);
	});
	this->io.socket()->on("notification", [&](sio::event& ev) {
		Global::Notify::Info("API Notification", ev.get_message()->get_string());
	});
	this->io.socket()->on("player", [&](sio::event& ev) {
		int HALF_SECOND = 500; // I really hate magic numbers
		setTimeout([=](){
			auto foo = ev.get_messages();
			string slug = foo.at(0)->get_string();
			MapL1[slug] = foo.at(1)->get_string();
			MapL2[slug] = foo.at(2)->get_string();
			MapL3[slug] = foo.at(3)->get_string();
			MapI1[slug] = foo.at(4)->get_string();
			MapI2[slug] = foo.at(5)->get_string();
			MapI3[slug] = foo.at(6)->get_string();
			MapI4[slug] = foo.at(7)->get_string();
			MapI5[slug] = foo.at(8)->get_string();
			MapI6[slug] = foo.at(9)->get_string();
			MapI7[slug] = foo.at(10)->get_string();
			MapI8[slug] = foo.at(11)->get_string();
			MapI9[slug] = foo.at(12)->get_string();
			MapI10[slug] = foo.at(13)->get_string();
		}, HALF_SECOND);
	});
}
// Emit in separate thread to reduce performance impact
void RocketLounge::SioEmit(string event) {
	if (!this->SioConnected) return;
	// Log::Info("Socket.io Emission: " + event);
    thread t([=]() { this->io.socket()->emit(event); });
    t.detach();
}
void RocketLounge::SioEmit(string event, string payload) {
	if (!this->SioConnected) return;
	// Log::Info("Socket.io Emission: " + event);
    thread t([=]() { this->io.socket()->emit(event, payload); });
    t.detach();
}
void RocketLounge::SioEmit(string event, sio::message::list const& payload) {
	if (!this->SioConnected) return;
	// Log::Info("Socket.io Emission: " + event);
    thread t([=]() { this->io.socket()->emit(event, payload); });
    t.detach();
}

