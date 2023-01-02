#pragma once
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma warning(disable:4996)
// Above line prevents compilation error:
// 'PlayerReplicationInfoWrapper::GetUniqueId': Use GetUniqueIdWrapper instead

#include <string>
#include <enet/enet.h>
#include <sio_client.h>
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

template <typename F>
void HttpGet(string url, F callback) {
	CurlRequest req; req.url = url;
	HttpWrapper::SendCurlRequest(req, [callback](int code, string result) { callback(result); });
}

class RocketLounge: public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow
{
	// Inherited via PluginSettingsWindow
	string GetPluginName() override;
	void RenderSettings() override;
	void SetImGuiContext(uintptr_t ctx) override;

	// Settings helpers
	void RenderSettingsConnectedMainMenu();
	void RenderSettingsConnectedFreePlay();

	// Plugin manager hooks
	virtual void onLoad();
	virtual void onUnload();

	// Game event hooks
	void onTick(ServerWrapper caller, void* params, string eventName);
	void MeasureTickRate();
	void ShowChatMessage(string sender, string message);

	bool IsRecording = false;
	void ToggleRecording();
	bool IsTrimming = false;
	void ToggleTrimming();

	int MyTickRate = 60;
	string MySlug = "";
	string MyDisplayName = "";
	map<string, bool> SlugSubs = {};
	map<string, int> SlugLastSeen = {};
	map<string, string> SlugDisplayNames = {};
	bool DataFlowAllowed();
	void DestroyStuff();

	// ENet specific implementation
	string enetDelim = "|";
	ENetHost * enetClient;
	ENetPeer * enetHost;
	bool ENetConnected = false;
	void ENetConnect();
	void ENetDisconnect();
	void ENetRelay(int timeout = 0);
	void ENetEmit(vector<string> payload);
	void ENetReceive(string rawPayload);

	// Socket.io specific implementation
	sio::client io;
    bool SioConnected = false;
    void SioConnect();
    void SioDisconnect();
	void SioEmit(string event);
	void SioEmit(string event, string payload);
	void SioEmit(string event, sio::message::list const& payload);

	// Generic wrappers that can use both ENet and Socket.io
	void EmitPlayerEvent(vector<string> payload);
	void IncomingPlayerEvent(vector<string> payload);

};
