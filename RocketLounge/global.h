#pragma once
#include <iostream>
#include <imgui_stdlib.h> // ImGui::InputText
#include "bakkesmod/plugin/bakkesmodplugin.h"

using namespace std;

namespace Global
{
    shared_ptr<GameWrapper> GameWrapper;
    shared_ptr<CVarManagerWrapper> CvarManager;
    namespace Notify
    {
		void Info(string title, string message)
        {
            GameWrapper->Toast(title, message, "cool", 5.0, ToastType_Info);
        }
		void Error(string title, string message)
        {
            GameWrapper->Toast(title, message, "cool", 5.0, ToastType_Error);
        }
		void Success(string title, string message)
        {
            GameWrapper->Toast(title, message, "cool", 5.0, ToastType_OK);
        }
    };

    namespace PlaylistIds
    {
        int Casual = 0;
        int Ranked1s = 10;
        int Ranked2s = 11;
        int Ranked3s = 13;
        int Hoops = 27;
        int Rumble = 28;
        int Dropshot = 29;
        int Snowday = 30;
    };
};
