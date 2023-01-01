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

	new Cvar("api_host", defaultApiHost);
	new Cvar("ui_use_slugs", false);
	new Cvar("enable_collisions", false);
	new Cvar("player_list_filter", FilterPlaceholder);
	new Cvar("chat_input", "");

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
	if (this->SioConnected)
	{
		if (gameWrapper->IsInFreeplay()) return true;
		if (gameWrapper->IsInCustomTraining()) return true;
	}
	return false;
}

void RocketLounge::DestroyStuff()
{
	CloneManager::DestroyClones();
	this->SlugSubs.clear();
	this->SlugLastSeen.clear();
	this->SlugDisplayNames.clear();
}

void RocketLounge::onTick(ServerWrapper caller, void* params, string eventName)
{
	if (!this->DataFlowAllowed()) return this->DestroyStuff();
	this->MeasureTickRate();

	auto server = gameWrapper->GetCurrentGameState(); if (server.IsNull()) return this->DestroyStuff();
	auto player = gameWrapper->GetPlayerController(); if (player.IsNull()) return this->DestroyStuff();
	auto pri = player.GetPRI(); if (pri.IsNull()) return this->DestroyStuff();

	this->MyDisplayName = pri.GetPlayerName().ToString();
	string platform = pri.GetUniqueIdWrapper().GetPlatform() == OnlinePlatform_Steam ? "steam" : "epic";
	string platformUserId = platform == "steam" ? to_string(pri.GetUniqueId().ID) : this->MyDisplayName;
	this->MySlug = platform + "/" + platformUserId;

	auto car = pri.GetCar(); if (car.IsNull()) return this->DestroyStuff();
	int carBody = car.GetLoadoutBody();
	auto carLocation = car.GetLocation();
	auto carVelocity = car.GetVelocity();
	auto carRotation = car.GetRotation();

	auto balls = server.GetGameBalls(); if (!balls.Count()) return this->DestroyStuff();
	auto ball = balls.Get(0); if (ball.IsNull()) return this->DestroyStuff();
	auto ballLocation = ball.GetLocation();
	auto ballVelocity = ball.GetVelocity();
	auto ballRotation = ball.GetRotation();
	
	CloneManager::ReflectClones();

	sio::message::list self(this->MySlug);
	for(int i = 1; i < (int)PlayerData::END_PLAYER_DATA; i++)
	{
		switch((PlayerData)i)
		{
			case PlayerData::DisplayName: 		self.push(sio::string_message::create(this->MyDisplayName)); 			break;
			case PlayerData::BallLocationX: 	self.push(sio::string_message::create(to_string(ballLocation.X))); 		break;
			case PlayerData::BallLocationY: 	self.push(sio::string_message::create(to_string(ballLocation.Y))); 		break;
			case PlayerData::BallLocationZ: 	self.push(sio::string_message::create(to_string(ballLocation.Z))); 		break;
			case PlayerData::BallVelocityX: 	self.push(sio::string_message::create(to_string(ballVelocity.X))); 		break;
			case PlayerData::BallVelocityY: 	self.push(sio::string_message::create(to_string(ballVelocity.Y))); 		break;
			case PlayerData::BallVelocityZ: 	self.push(sio::string_message::create(to_string(ballVelocity.Z))); 		break;
			case PlayerData::BallRotationPitch: self.push(sio::string_message::create(to_string(ballRotation.Pitch))); 	break;
			case PlayerData::BallRotationYaw: 	self.push(sio::string_message::create(to_string(ballRotation.Yaw))); 	break;
			case PlayerData::BallRotationRoll: 	self.push(sio::string_message::create(to_string(ballRotation.Roll))); 	break;
			case PlayerData::CarBody: 			self.push(sio::string_message::create(to_string(carBody))); 			break;
			case PlayerData::CarLocationX: 		self.push(sio::string_message::create(to_string(carLocation.X))); 		break;
			case PlayerData::CarLocationY: 		self.push(sio::string_message::create(to_string(carLocation.Y))); 		break;
			case PlayerData::CarLocationZ: 		self.push(sio::string_message::create(to_string(carLocation.Z))); 		break;
			case PlayerData::CarVelocityX: 		self.push(sio::string_message::create(to_string(carVelocity.X))); 		break;
			case PlayerData::CarVelocityY: 		self.push(sio::string_message::create(to_string(carVelocity.Y))); 		break;
			case PlayerData::CarVelocityZ: 		self.push(sio::string_message::create(to_string(carVelocity.Z))); 		break;
			case PlayerData::CarRotationPitch: 	self.push(sio::string_message::create(to_string(carRotation.Pitch))); 	break;
			case PlayerData::CarRotationYaw: 	self.push(sio::string_message::create(to_string(carRotation.Yaw))); 	break;
			case PlayerData::CarRotationRoll: 	self.push(sio::string_message::create(to_string(carRotation.Roll))); 	break;
		}
	}
	
	this->SioEmit("self", self);
}

int measuredTicks = 0;
int lastTickRateMeasurement = 0;
void RocketLounge::MeasureTickRate()
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
	this->io.set_open_listener([this, apiHost]() {
		this->SioConnected = true;
        Global::Notify::Success("Lounge Connected", string("Successfully connected to " + apiHost));
	});
	this->io.set_close_listener([this, apiHost](sio::client::close_reason const& reason) {
		this->DestroyStuff();
		this->SioConnected = false;
		string msg = reason == sio::client::close_reason::close_reason_normal ? "closed" : "dropped";
		string fullMsg = "Connection to " + apiHost + " was " + msg;
        Global::Notify::Error("Lounge Disconnected", fullMsg);
	});
	this->io.socket()->on("notification", [this](sio::event& ev) {
		Global::Notify::Info("API Notification", ev.get_message()->get_string());
	});
	this->io.socket()->on("chat", [this](sio::event& ev) {
		if (!this->DataFlowAllowed()) return;
		auto pieces = ev.get_messages();
		if (pieces.size() != 2) return;
		string slug = pieces.at(0)->get_string();
		if (!this->SlugSubs.count(slug) && slug != this->MySlug) return;
		this->ShowChatMessage(this->SlugDisplayNames[slug], pieces.at(1)->get_string());
	});
	this->io.socket()->on("player", [this](sio::event& ev) {
		if (!this->DataFlowAllowed()) return;
		auto pieces = ev.get_messages();
		if (pieces.size() != (int)PlayerData::END_PLAYER_DATA) return;
		string slug = pieces.at((int)PlayerData::Slug)->get_string();
		string displayName = pieces.at((int)PlayerData::DisplayName)->get_string();
		string carBodyStr = pieces.at((int)PlayerData::CarBody)->get_string();
		if (!this->SlugLastSeen.count(slug) && slug != this->MySlug)
		{
			Global::Notify::Success("New player available", "You can now add " + displayName + " to your session");
		}
		this->SlugLastSeen[slug] = timestamp();
		this->SlugDisplayNames[slug] = displayName;
		if (!this->SlugSubs.count(slug)) return;
		CloneManager::UseClone(slug, displayName, stoi(carBodyStr))->SetBall({
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
		CloneManager::UseClone(slug, displayName, stoi(carBodyStr))->SetCar({
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

void RocketLounge::ShowChatMessage(string sender, string message)
{
	if (!sender.length() || !message.length()) return;
	gameWrapper->Execute([this, sender, message](...){
		gameWrapper->LogToChatbox(message, sender);
	});
}

void RocketLounge::RenderSettings()
{

	ImGui::NewLine();
	ImGui::Columns(2, "split", false);

	int red = !this->SioConnected ? 255 : 0;
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(red,255,0,255));
	ImGui::Text(this->SioConnected ? "  API Connected  " : "  API Disonnected");
	ImGui::PopStyleColor();
	ImGui::SameLine();
	string tickRateLabel = "   Tick Rate: " + to_string(this->MyTickRate);
	ImGui::Text(tickRateLabel.c_str());
	ImGui::SameLine();
	ImGui::Text("\t\t\t\t\t\t\t");
	ImGui::SameLine();
	if (ImGui::Button(this->SioConnected ? "   Disconnect   " : "     Connect     "))
	{
		if (this->SioConnected) this->SioDisconnect();
		else this->SioConnect();
	}
	
	if (this->SioConnected)
	{
		ImGui::NewLine();
		string recordLabel = "   " + string(this->IsRecording ? "Save Recording" : "Start Recording") + "   ";
		string trimLabel = "   " + string(this->IsTrimming ? "Save Trimming" : "Start Trimming") + "   ";
		if (ImGui::Button(recordLabel.c_str())) this->ToggleRecording();
		ImGui::SameLine();
		if (ImGui::Button(trimLabel.c_str())) this->ToggleTrimming();
		ImGui::SameLine();
		ImGui::Text("\t\t\t");
		ImGui::SameLine();
		Cvar::Get("enable_collisions")->RenderCheckbox(" Enable Collisions ");
		ImGui::NewLine();

		Cvar::Get("chat_input")->RenderMultilineInput(" Lounge Chat  \t\t\t\t\t  (visible to everyone with you in their session) ");
		if (ImGui::Button(" \t\t\t\t\t\t\t\t\t\t\t\t Send Chat \t\t\t\t\t\t\t\t\t\t\t\t "))
		{
			string chatInput = Cvar::Get("chat_input")->toString();
			Cvar::Get("chat_input")->setString("");
			sio::message::list chat(this->MySlug);
			chat.push(sio::string_message::create(chatInput));
			this->SioEmit("chat", chat);
		}
	}
	
    ImGui::NextColumn();
    ImGui::BeginChild("right");

	if (this->SioConnected)
	{
		ImGui::Columns(2, "split", false);

		bool usingSlugs = Cvar::Get("ui_use_slugs")->toBool();
		if (this->DataFlowAllowed())
		{
			ImGui::Spacing();
			ImGui::Text("Available Players     ");
			ImGui::SameLine();
			Cvar::Get("player_list_filter")->RenderSmallInput("", 96);
			if (this->SlugLastSeen.size())
			{
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
		}


    	ImGui::NextColumn();
		
		string label = usingSlugs ? "names" : "slugs";
		string fullLabel = "   Show player " + label + "   ";
		if (ImGui::Button(fullLabel.c_str()))
		{
			Cvar::Get("ui_use_slugs")->setBool(!usingSlugs);
		}

		ImGui::Spacing();
		ImGui::Text("Players in Session");
		if (this->SlugSubs.size())
		{
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
