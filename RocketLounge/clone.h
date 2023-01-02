#pragma once
#include <string>
#include "logging.h"
#include "global.h"

// Clone can be created on socket events or other tasks deferred to detached threads
// For this reason all calls to shared pointers (gameWrapper, etc) must be called inside
// Execute() to ensure those tasks run in the game's main thread

class Clone
{
	public:
	int CarBody;
	string Slug, DisplayName;
    bool CarExists, BallExists;
    Rotator CarRotation, BallRotation;
    Vector CarLocation, CarVelocity, BallLocation, BallVelocity;
	Clone(string slug, string displayName, int carBody, bool spawnCar=false, bool spawnBall=false)
	{
		this->Slug = slug;
        this->CarBody = carBody;
		this->DisplayName = displayName;
        if (spawnCar) this->SpawnCar();
        if (spawnBall) this->SpawnBall();
	}

    int GetPriIdx()
    {
        ServerWrapper server = Global::GameWrapper->GetCurrentGameState();
        if (server.IsNull()) return -1;
        auto pris = server.GetPRIs();
        if (!pris.Count()) return -1;
        int i = 0;
        for(auto pri : pris)
        {
            if (pri.GetUniqueIdWrapper().GetEpicAccountID() == this->Slug)
            {
                return i;
            }
            i++;
        }
        return -1;
    }

    // Must execute in main game thread (since this can be triggered by socket event or other event in alt thread)
    void SpawnCar()
    {
        if (this->CarExists) return;
        Global::GameWrapper->Execute([this](...){
			ServerWrapper server = Global::GameWrapper->GetGameEventAsServer();
			if (server.IsNull()) return;
            // Spawn car clone
            Log::Info("Spawning " + this->DisplayName);
            server.SpawnBot(this->CarBody, this->DisplayName);
            auto pris = server.GetPRIs();
            int  priIdx = pris.Count() - 1;
            Log::Info("PRI Index: " + to_string(priIdx));
            auto botPri = pris.Get(priIdx); if (botPri.IsNull()) return;
            Log::Info("PRI Display Name: " + botPri.GetPlayerName().ToString());
            auto botUID = UniqueIDWrapper::FromEpicAccountID(this->Slug, priIdx, OnlinePlatform_Epic);
            botPri.SetUniqueId2(botUID);
            Log::Info("Spawning bot " + botPri.GetUniqueIdWrapper().GetEpicAccountID());
            auto botCar = botPri.GetCar(); if (botCar.IsNull()) return;
            botCar.GetAIController().DoNothing();
            if (!Cvar::Get("enable_collisions")->toBool())
            {
                botCar.GetCollisionComponent().SetRBChannel(6);
                botCar.GetCollisionComponent().SetRBCollidesWithChannel(3, 0);
                botCar.GetCollisionComponent().SetBlockRigidBody2(0);
            }
            this->CarExists = true;
		});
    }

    void DestroyCar()
    {
        if (!this->CarExists) return;
        Global::GameWrapper->Execute([this](...){
            auto server = Global::GameWrapper->GetGameEventAsServer();
            if (server.IsNull()) return;
            auto priIdx = this->GetPriIdx();
            if (priIdx < 0) return;
            auto players = server.GetPlayers();
            if (players.Count() < priIdx + 1) return;
            auto botPlayer = players.Get(priIdx);
            if (botPlayer.IsNull()) return;
            server.RemovePlayer(botPlayer);
            Log::Info("Player/car destroyed for " + this->Slug);
            this->CarExists = false;
		});
    }

    void SpawnBall()
    {
        if (this->BallExists) return;
        Global::GameWrapper->Execute([this](...){
			ServerWrapper server = Global::GameWrapper->GetGameEventAsServer();
			if (server.IsNull()) return;
            // if (Global::GameWrapper->IsInCustomTraining()) return;
            // Spawn ball clone
            server.SpawnBall(Vector{0, 0, 1000}, true, false);
            auto balls = server.GetGameBalls();
            int  ballIdx = balls.Count() - 1;
            auto botBall = balls.Get(ballIdx);
            // this currently doesn't work for cars unless you apply to both bot and your car but then
            // you go through walls and doing janky logic around that smelled so fugg it for now
            if (!Cvar::Get("enable_collisions")->toBool())
            {
                botBall.GetCollisionComponent().SetRBChannel(6);
                botBall.GetCollisionComponent().SetRBCollidesWithChannel(3, 0);
                botBall.GetCollisionComponent().SetBlockRigidBody2(0);
            }
            this->BallExists = true;
		});
    }

    void DestroyBall()
    {
        if (!this->BallExists) return;
        Global::GameWrapper->Execute([this](...){
            auto priIdx = this->GetPriIdx();
            if (priIdx < 0) return;
            auto server = Global::GameWrapper->GetGameEventAsServer();
            if (server.IsNull()) return;
            auto balls = server.GetGameBalls();
            if (balls.Count() < priIdx + 1) return;
            auto botBall = balls.Get(priIdx);
            if (botBall.IsNull()) return;
            botBall.DoDestroy();
            Log::Info("Ball destroyed for " + this->Slug);
            this->BallExists = false;
		});
    }

    void SetCar(Vector location, Vector velocity, Rotator rotation)
    {
        this->CarVelocity = velocity;
        this->CarLocation = location;
        this->CarRotation = rotation;
    }
    void ReflectCar()
    {
        Global::GameWrapper->Execute([this](...){
			ServerWrapper server = Global::GameWrapper->GetGameEventAsServer();
			if (!server.IsNull())
			{
                auto pris = server.GetPRIs();
                auto botPri = pris.Get(this->GetPriIdx()); if (botPri.IsNull()) return;
                auto botCar = botPri.GetCar(); if (botCar.IsNull()) return;
                auto botName = botPri.GetPlayerName().ToString();
                botCar.SetLocation(this->CarLocation);
                botCar.SetVelocity(this->CarVelocity);
                botCar.SetRotation(this->CarRotation);
			}
		});
    }

    void SetBall(Vector location, Vector velocity, Rotator rotation)
    {
        this->BallLocation = location;
        this->BallVelocity = velocity;
        this->BallRotation = rotation;
    }
    void ReflectBall()
    {
        Global::GameWrapper->Execute([this](...){
			ServerWrapper server = Global::GameWrapper->GetGameEventAsServer();
			if (!server.IsNull())
			{
                auto balls = server.GetGameBalls();
                auto botBall = balls.Get(this->GetPriIdx()); if (botBall.IsNull()) return;
                botBall.SetLocation(this->BallLocation);
                botBall.SetVelocity(this->BallVelocity);
                botBall.SetRotation(this->BallRotation);
			}
		});
    }
};

class CloneManager
{
	public:
    static inline map<string, Clone*> CloneMap = {};
    static Clone* UseClone(string slug, string displayName, int carBody)
    {
        if (!CloneManager::CloneMap.count(slug) || CloneManager::CloneMap[slug] == NULL)
        {
            Log::Error("Creating new clone for " + slug + " (" + displayName + ")");
            CloneManager::CloneMap[slug] = new Clone(slug, displayName, carBody, true, true);
        }
        return CloneManager::CloneMap[slug];
    }

    static void ReflectClones()
    {
        if (Global::GameWrapper->IsPaused()) return;
        Global::GameWrapper->Execute([](...){
            for (const auto &[slug, clone] : CloneManager::CloneMap)
            {
                clone->ReflectCar();
                clone->ReflectBall();
            }
		});
    }

    static void DestroyClones()
    {
        if (!CloneManager::CloneMap.size()) return;
        for (auto &[slug, clone] : CloneManager::CloneMap)
        {
            CloneManager::DestroyClone(slug);
        }
        CloneManager::CloneMap.clear();
    }

    static void DestroyClone(string slug)
    {
        if (!CloneManager::CloneMap.count(slug)) return;
        auto clone = CloneManager::CloneMap[slug];
        Log::Info("Removing " + slug);
        clone->DestroyBall();
        clone->DestroyCar();
        CloneManager::CloneMap.erase(slug);
        Log::Info("Lookup key removed for " + slug);
    }
};
