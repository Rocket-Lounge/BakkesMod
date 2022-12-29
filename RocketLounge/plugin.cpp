#include "pch.h"
#include "cvar.h"
#include "match.h"
#include "plugin.h"
#include "global.h"
#include "logging.h"
#include "overlay.h"
#include "timer.h"
#include "clone.h"
#include "version.h"
#include <imgui_stdlib.h> // ImGui::InputText
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;

const char * PluginName = "A1 Rocket Lounge"; // To change DLL filename use <TargetName> in *.vcxproj
constexpr auto PluginVersion = stringify(V_MAJOR) "." stringify(V_MINOR) "." stringify(V_PATCH) "." stringify(V_BUILD);
BAKKESMOD_PLUGIN(RocketLounge, PluginName, PluginVersion, PLUGINTYPE_FREEPLAY);

// slug, displayName, lastSeenTime
map<string, bool> SlugSubs = {};
map<string, int> SlugLastSeen = {};
map<string, string> SlugDisplayNames = {};

void RocketLounge::onLoad()
{
	Global::GameWrapper = gameWrapper;
	Global::CvarManager = cvarManager;

	Log::SetPrintLevel(Log::Level::Info);
	Log::SetWriteLevel(Log::Level::Info);

	new Cvar("enable_mpfp", true);
	new Cvar("api_host", "http://rl-data-bus.fly.dev");
	new Cvar("io_threads", false);
	new Cvar("ui_use_slugs", false);

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

void RocketLounge::onTick(ServerWrapper caller, void* params, string eventName)
{
	auto server = gameWrapper->GetGameEventAsServer();
	if (!gameWrapper->IsInFreeplay() || server.IsNull())
	{
		CloneManager::DestroyClones();
		return;
	}

	auto pris = server.GetPRIs();
	auto myPri = pris.Get(0); if (myPri.IsNull()) return;
	auto myCar = myPri.GetCar(); if (myCar.IsNull()) return;

	// bool nearWall = false;
	// if (nearWall)
	// {
	// 	myCar.GetCollisionComponent().SetRBChannel(0);
	// 	myCar.GetCollisionComponent().SetRBCollidesWithChannel(0, 0);
	// 	myCar.GetCollisionComponent().SetBlockRigidBody2(1);
	// }
	// else
	// {
	// 	// disable body collisions
	// 	myCar.GetCollisionComponent().SetBlockRigidBody2(0);
	// 	// disable wheel collisions
	// 	myCar.GetCollisionComponent().SetRBChannel(6);
	// 	myCar.GetCollisionComponent().SetRBCollidesWithChannel(3, 0);
	// }

	auto myInput = myCar.GetInput();
	auto myLocation = myCar.GetLocation();
	string myName = myPri.GetPlayerName().ToString();
	auto platId = myPri.GetUniqueIdWrapper().GetPlatform();

	string myPlatform = platId == OnlinePlatform_Steam ? "steam" : "epic";
	string myPlatformUserId = myPlatform == "steam" ? to_string(myPri.GetUniqueId().ID) : myName;
	string mySlug = myPlatform + "/" + myPlatformUserId;
	auto balls = server.GetGameBalls();
	auto myBall = balls.Get(0); if (myBall.IsNull()) return;
	auto ballLocation = myBall.GetLocation();

	sio::message::list self(mySlug);
	self.push(sio::string_message::create(myName));
	self.push(sio::string_message::create(to_string(ballLocation.X)));
	self.push(sio::string_message::create(to_string(ballLocation.Y)));
	self.push(sio::string_message::create(to_string(ballLocation.Z)));

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
		auto pieces = ev.get_messages();
		string slug = pieces.at(0)->get_string();
		string displayName = pieces.at(1)->get_string();
		SlugLastSeen[slug] = timestamp();
		SlugDisplayNames[slug] = displayName;
		if (!SlugSubs.count(slug)) return;
		if (!Cvar::Get("enable_mpfp")->toBool())
		{
			CloneManager::DestroyClones();
			return;
		}
		CloneManager::UseClone(slug, displayName)->SetBall({
			stof(pieces.at(2)->get_string()),
			stof(pieces.at(3)->get_string()),
			stof(pieces.at(4)->get_string()),
		});
		CloneManager::UseClone(slug, displayName)->SetCar({
			stof(pieces.at(5)->get_string()),
			stof(pieces.at(6)->get_string()),
			stof(pieces.at(7)->get_string()),
		}, {
			stof(pieces.at(8)->get_string()),
			stof(pieces.at(9)->get_string()),
			stof(pieces.at(10)->get_string()),
			stof(pieces.at(11)->get_string()),
			stof(pieces.at(12)->get_string()),
			stof(pieces.at(13)->get_string()),
			stof(pieces.at(14)->get_string()),
			stoul(pieces.at(15)->get_string()),
			stoul(pieces.at(16)->get_string()),
			stoul(pieces.at(17)->get_string()),
			1, // stoul(pieces.at(17)->get_string()), // these are always empty
			1, // stoul(pieces.at(18)->get_string()),
		});
		CloneManager::UseClone(slug, displayName)->ReflectCar();
		CloneManager::UseClone(slug, displayName)->ReflectBall();
	});
}
// Emit in separate thread to reduce performance impact
void RocketLounge::SioEmit(string event) {
	if (!this->SioConnected) return;
	// Log::Info("Socket.io Emission: " + event);
	if (Cvar::Get("io_threads")->toBool()) {
		thread t([=]() { this->io.socket()->emit(event); });
		t.detach();
	} else {
		this->io.socket()->emit(event);
	}
}
void RocketLounge::SioEmit(string event, string payload) {
	if (!this->SioConnected) return;
	// Log::Info("Socket.io Emission: " + event);
	if (Cvar::Get("io_threads")->toBool()) {
		thread t([=]() { this->io.socket()->emit(event, payload); });
		t.detach();
	} else {
		this->io.socket()->emit(event, payload);
	}
}
void RocketLounge::SioEmit(string event, sio::message::list const& payload) {
	if (!this->SioConnected) return;
	// Log::Info("Socket.io Emission: " + event);
	if (Cvar::Get("io_threads")->toBool()) {
		thread t([=]() { this->io.socket()->emit(event, payload); });
		t.detach();
	} else {
		this->io.socket()->emit(event, payload);
	}
}



void RocketLounge::RenderSettings()
{

	ImGui::NewLine();
	ImGui::Columns(2, "split", false);
	
	if (this->SioConnected)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,255,0,255));
		ImGui::Text("  API Connected  ");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::Text("\t\t\t\t\t\t\t");
		ImGui::SameLine();
		ImGui::Text("\t\t\t\t\t\t\t");
		ImGui::SameLine();
		if (ImGui::Button("   Disconnect   "))
		{
			this->SioDisconnect();
		}
		ImGui::Spacing();
		Cvar::Get("io_threads")->RenderCheckbox(" Enable multi-threading ");
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

	
    ImGui::NextColumn();
    ImGui::BeginChild("right");

	if (this->SioConnected)
	{
		ImGui::Columns(2, "split", false);

		Cvar::Get("enable_mpfp")->RenderCheckbox(" Multiplayer freeplay ");
		bool usingSlugs = Cvar::Get("ui_use_slugs")->toBool();

		if (Cvar::Get("enable_mpfp")->toBool())
		{
			ImGui::Spacing();
			ImGui::Text("Available Players");
			for (const auto &[slug, seenTime] : SlugLastSeen)
			{
				if (SlugSubs.count(slug)) continue;
				int secAgo = timestamp() - seenTime;
				if (secAgo > 1) return; // if data is 1s or older its stale
				string label = usingSlugs ? slug : SlugDisplayNames[slug];
				string fullLabel = "   " + label + "   ";
				if (ImGui::Button(fullLabel.c_str()))
				{
					SlugSubs[slug] = true;
				}
			}
		}


    	ImGui::NextColumn();
		
		if (Cvar::Get("enable_mpfp")->toBool())
		{
			string label = usingSlugs ? "names" : "slugs";
			string fullLabel = "   Show player " + label + "   ";
			if (ImGui::Button(fullLabel.c_str()))
			{
				Cvar::Get("ui_use_slugs")->setBool(!usingSlugs);
			}

			ImGui::Spacing();
			ImGui::Text("Players in Session");
			for (const auto &[slug, subbed] : SlugSubs)
			{
				string label = usingSlugs ? slug : SlugDisplayNames[slug];
				string fullLabel = "   " + label + "   ";
				if (ImGui::Button(fullLabel.c_str()))
				{
					CloneManager::DestroyClone(slug);
					SlugSubs.erase(slug);
				}
			}
		}

    	ImGui::Columns();
	}

    ImGui::EndChild();
    ImGui::Columns();
}
// More settings rendering boilerplate
string RocketLounge::GetPluginName() { return string(PluginName); }
void RocketLounge::SetImGuiContext(uintptr_t ctx) { ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx)); }
