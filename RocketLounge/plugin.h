#pragma once
#pragma warning(disable:4996)
// Above line prevents compilation error:
// 'PlayerReplicationInfoWrapper::GetUniqueId': Use GetUniqueIdWrapper instead

#include <string>
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

	// API functionality
	sio::client io;
    bool SioConnected = false;
    void SioConnect();
    void SioDisconnect();
	void SioEmit(string event);
	void SioEmit(string event, string payload);
	void SioEmit(string event, sio::message::list const& payload);

	// Game event hooks
	void onTick(ServerWrapper caller, void* params, string eventName);

	
	void MeasureTickRate();
	void ShowChatMessage(string sender, string message);
};
