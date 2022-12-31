#include "pch.h"
#include "cvar.h"
#include "plugin.h"
#include "global.h"
#include "logging.h"
#include "timer.h"
#include "clone.h"
#include "version.h"
#include <imgui_stdlib.h> // ImGui::InputText
using namespace std;

string defaultApiHost = "http://rocketlounge.gg";
// string defaultApiHost = "http://localhost:8080";
string FilterPlaceholder = "Filter...";

const char * PluginName = "A1 Rocket Lounge"; // To change DLL filename use <TargetName> in *.vcxproj
constexpr auto PluginVersion = stringify(V_MAJOR) "." stringify(V_MINOR) "." stringify(V_PATCH) "." stringify(V_BUILD);
BAKKESMOD_PLUGIN(RocketLounge, PluginName, PluginVersion, PLUGINTYPE_FREEPLAY);

// Changing order will break previous versions of plugin
enum class PlayerData
{
	Slug, // this is contractual
	// Version, // string(PluginVersion)
	DisplayName,
	CarBody,
	CarLocationX,
	CarLocationY,
	CarLocationZ,
	CarVelocityX,
	CarVelocityY,
	CarVelocityZ,
	CarRotationYaw,
	CarRotationRoll,
	CarRotationPitch,
	BallLocationX,
	BallLocationY,
	BallLocationZ,
	BallVelocityX,
	BallVelocityY,
	BallVelocityZ,
	BallRotationYaw,
	BallRotationRoll,
	BallRotationPitch,
	END_PLAYER_DATA // this must remain at end of enum
};

void RocketLounge::onLoad()
{
	Global::GameWrapper = gameWrapper;
	Global::CvarManager = cvarManager;

	Log::SetPrintLevel(Log::Level::Info);
	Log::SetWriteLevel(Log::Level::Info);

	new Cvar("enable_mpfp", true, [=](string name, bool curValue, bool oldValue){
		if (!curValue && oldValue) this->DestroyStuff();
	});
	new Cvar("api_host", defaultApiHost);
	new Cvar("enable_ui", true);
	new Cvar("ui_use_slugs", false);
	new Cvar("enable_collisions", false);
	new Cvar("player_list_filter", FilterPlaceholder);

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
	this->SioDisconnect();
}

void RocketLounge::ToggleRecording()
{
	string url = Cvar::Get("api_host")->toString() + "/mock/record/" + this->MySlug + "/" + to_string(this->MyTickRate);
	HttpGet(url, [=](string status) { this->IsRecording = status == "recording"; });
}

void RocketLounge::ToggleTrimming()
{
	string url = Cvar::Get("api_host")->toString() + "/mock/trim/" + this->MySlug;
	HttpGet(url, [=](string status) { this->IsTrimming = status == "recording"; });
}

bool RocketLounge::DataFlowAllowed()
{
	auto server = gameWrapper->GetGameEventAsServer();
	if (!server.IsNull() && Cvar::Get("enable_mpfp")->toBool())
	{
		if (gameWrapper->IsInFreeplay()) return true;
		if (gameWrapper->IsInCustomTraining()) return true;
	}
	return false;
}

void RocketLounge::DestroyStuff()
{
	
	if (this->DataFlowAllowed()) CloneManager::DestroyClones();
	this->SlugSubs.clear();
	this->SlugLastSeen.clear();
	this->SlugDisplayNames.clear();
}

int measuredTicks = 0;
int lastTickRateMeasurement = 0;
void RocketLounge::onTick(ServerWrapper caller, void* params, string eventName)
{
	if (!lastTickRateMeasurement)
	{
		lastTickRateMeasurement = timestamp();
	}
	else
	{
		if (timestamp() - lastTickRateMeasurement >= 1)
		{
			this->MyTickRate = measuredTicks;
			measuredTicks = 0;
			lastTickRateMeasurement = 0;
		}
		else
		{
			measuredTicks++;
		}
	}
	if (!this->DataFlowAllowed())
	{
		this->DestroyStuff();
		return;
	}

	auto server = gameWrapper->GetGameEventAsServer();
	auto pris = server.GetPRIs();
	auto myPri = pris.Get(0); if (myPri.IsNull()) return;
	auto myCar = myPri.GetCar(); if (myCar.IsNull()) return;
	int myCarBody = myCar.GetLoadoutBody();

	auto myLocation = myCar.GetLocation();
	auto myVelocity = myCar.GetVelocity();
	auto myRotation = myCar.GetRotation();
	string myName = myPri.GetPlayerName().ToString();
	auto platId = myPri.GetUniqueIdWrapper().GetPlatform();

	string myPlatform = platId == OnlinePlatform_Steam ? "steam" : "epic";
	string myPlatformUserId = myPlatform == "steam" ? to_string(myPri.GetUniqueId().ID) : myName;
	this->MySlug = myPlatform + "/" + myPlatformUserId;
	auto balls = server.GetGameBalls();
	auto myBall = balls.Get(0); if (myBall.IsNull()) return;
	auto ballLocation = myBall.GetLocation();
	auto ballVelocity = myBall.GetVelocity();
	auto ballRotation = myBall.GetRotation();

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
	
	CloneManager::ReflectClones();

	sio::message::list self(this->MySlug);
	for(int i = 1; i < (int)PlayerData::END_PLAYER_DATA; i++)
	{
		switch((PlayerData)i)
		{
			case PlayerData::DisplayName: self.push(sio::string_message::create(myName)); break;
			case PlayerData::BallLocationX: self.push(sio::string_message::create(to_string(ballLocation.X))); break;
			case PlayerData::BallLocationY: self.push(sio::string_message::create(to_string(ballLocation.Y))); break;
			case PlayerData::BallLocationZ: self.push(sio::string_message::create(to_string(ballLocation.Z))); break;
			case PlayerData::BallVelocityX: self.push(sio::string_message::create(to_string(ballVelocity.X))); break;
			case PlayerData::BallVelocityY: self.push(sio::string_message::create(to_string(ballVelocity.Y))); break;
			case PlayerData::BallVelocityZ: self.push(sio::string_message::create(to_string(ballVelocity.Z))); break;
			case PlayerData::BallRotationPitch: self.push(sio::string_message::create(to_string(ballRotation.Pitch))); break;
			case PlayerData::BallRotationYaw: self.push(sio::string_message::create(to_string(ballRotation.Yaw))); break;
			case PlayerData::BallRotationRoll: self.push(sio::string_message::create(to_string(ballRotation.Roll))); break;
			case PlayerData::CarBody: self.push(sio::string_message::create(to_string(myCarBody))); break;
			case PlayerData::CarLocationX: self.push(sio::string_message::create(to_string(myLocation.X))); break;
			case PlayerData::CarLocationY: self.push(sio::string_message::create(to_string(myLocation.Y))); break;
			case PlayerData::CarLocationZ: self.push(sio::string_message::create(to_string(myLocation.Z))); break;
			case PlayerData::CarVelocityX: self.push(sio::string_message::create(to_string(myVelocity.X))); break;
			case PlayerData::CarVelocityY: self.push(sio::string_message::create(to_string(myVelocity.Y))); break;
			case PlayerData::CarVelocityZ: self.push(sio::string_message::create(to_string(myVelocity.Z))); break;
			case PlayerData::CarRotationPitch: self.push(sio::string_message::create(to_string(myRotation.Pitch))); break;
			case PlayerData::CarRotationYaw: self.push(sio::string_message::create(to_string(myRotation.Yaw))); break;
			case PlayerData::CarRotationRoll: self.push(sio::string_message::create(to_string(myRotation.Roll))); break;
		}
	}
	
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
        Global::Notify::Success("Lounge Connected", string("Successfully connected to " + apiHost));
	});
	this->io.set_close_listener([&](sio::client::close_reason const& reason) {
		this->DestroyStuff();
		this->SioConnected = false;
		string msg = reason == sio::client::close_reason::close_reason_normal ? "closed" : "dropped";
		string fullMsg = "Connection to " + apiHost + " was " + msg;
        Global::Notify::Error("Lounge Disconnected", fullMsg);
	});
	this->io.socket()->on("notification", [&](sio::event& ev) {
		Global::Notify::Info("API Notification", ev.get_message()->get_string());
	});
	this->io.socket()->on("player", [&](sio::event& ev) {
		if (!this->DataFlowAllowed()) return;
		auto pieces = ev.get_messages();
		if (pieces.size() != (int)PlayerData::END_PLAYER_DATA) return;
		string slug = pieces.at((int)PlayerData::Slug)->get_string();
		string displayName = pieces.at((int)PlayerData::DisplayName)->get_string();
		string carBodyStr = pieces.at((int)PlayerData::CarBody)->get_string();
		if (!this->SlugLastSeen.count(slug))
		{
			Global::Notify::Success("New player available", "You can now add " + displayName + " to your session");
		}
		this->SlugLastSeen[slug] = timestamp();
		this->SlugDisplayNames[slug] = displayName;
		if (!this->SlugSubs.count(slug)) return;
		CloneManager::UseClone(slug, displayName, stoi(carBodyStr)).SetBall({
			stof(pieces.at((int)PlayerData::BallLocationX)->get_string()),
			stof(pieces.at((int)PlayerData::BallLocationY)->get_string()),
			stof(pieces.at((int)PlayerData::BallLocationZ)->get_string()),
		}, {
			stof(pieces.at((int)PlayerData::BallVelocityX)->get_string()),
			stof(pieces.at((int)PlayerData::BallVelocityY)->get_string()),
			stof(pieces.at((int)PlayerData::BallVelocityZ)->get_string()),
		}, {
			stoi(pieces.at((int)PlayerData::BallRotationPitch)->get_string()),
			stoi(pieces.at((int)PlayerData::BallRotationYaw)->get_string()),
			stoi(pieces.at((int)PlayerData::BallRotationRoll)->get_string()),
		});
		CloneManager::UseClone(slug, displayName, stoi(carBodyStr)).SetCar({
			stof(pieces.at((int)PlayerData::CarLocationX)->get_string()),
			stof(pieces.at((int)PlayerData::CarLocationY)->get_string()),
			stof(pieces.at((int)PlayerData::CarLocationZ)->get_string()),
		}, {
			stof(pieces.at((int)PlayerData::CarVelocityX)->get_string()),
			stof(pieces.at((int)PlayerData::CarVelocityY)->get_string()),
			stof(pieces.at((int)PlayerData::CarVelocityZ)->get_string()),
		}, {
			stoi(pieces.at((int)PlayerData::CarRotationPitch)->get_string()),
			stoi(pieces.at((int)PlayerData::CarRotationYaw)->get_string()),
			stoi(pieces.at((int)PlayerData::CarRotationRoll)->get_string()),
		});
	});
}
// Emit in separate thread to reduce performance impact
void RocketLounge::SioEmit(string event) {  this->io.socket()->emit(event); }
void RocketLounge::SioEmit(string event, string payload) { this->io.socket()->emit(event, payload); }
void RocketLounge::SioEmit(string event, sio::message::list const& payload) { this->io.socket()->emit(event, payload); }

void RocketLounge::RenderSettings()
{

	ImGui::NewLine();
	Cvar::Get("enable_ui")->RenderCheckbox(" Enable UI ");
	if (!Cvar::Get("enable_ui")->toBool()) return;
	ImGui::SameLine();
	Cvar::Get("enable_collisions")->RenderCheckbox(" Enable Collisions ");
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
		string recordLabel = "   " + string(this->IsRecording ? "Save Recording" : "Start Recording") + "   ";
		string trimLabel = "   " + string(this->IsTrimming ? "Save Trimming" : "Start Trimming") + "   ";
		if (ImGui::Button(recordLabel.c_str())) this->ToggleRecording();
		ImGui::SameLine();
		if (ImGui::Button(trimLabel.c_str())) this->ToggleTrimming();
		ImGui::SameLine();
		string tickRateLabel = "   Tick Rate: " + to_string(this->MyTickRate);
		ImGui::Text(tickRateLabel.c_str());
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

		if (this->DataFlowAllowed())
		{
			ImGui::Spacing();
			ImGui::Text("Available Players     ");
			ImGui::SameLine();
			Cvar::Get("player_list_filter")->RenderSmallInput("", 96);
			for (const auto &[slug, seenTime] : this->SlugLastSeen)
			{
				if (slug == this->MySlug) continue;
				if (this->SlugSubs.count(slug)) continue;
				int secAgo = timestamp() - seenTime;
				if (secAgo > 1) return; // if data is 1s or older its stale
				string label = usingSlugs ? slug : this->SlugDisplayNames[slug];

				string filter = Cvar::Get("player_list_filter")->toString();
				if (filter.length() && filter != FilterPlaceholder)
				{
					if (label.find(filter) == string::npos)
					{
						continue;
					}
				}
				string fullLabel = "   " + label + "   ";
				if (ImGui::Button(fullLabel.c_str()))
				{
					this->SlugSubs[slug] = true;
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
			for (const auto &[slug, subbed] : this->SlugSubs)
			{
				string label = usingSlugs ? slug : this->SlugDisplayNames[slug];
				string fullLabel = "   " + label + "   ";
				if (ImGui::Button(fullLabel.c_str()))
				{
					CloneManager::DestroyClone(slug);
					this->SlugSubs.erase(slug);
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
