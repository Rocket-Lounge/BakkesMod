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
	int PriIdx, BallIdx, CarBody;
	string Slug, DisplayName;
    Rotator CarRotation, BallRotation;
    Vector CarLocation, CarVelocity, BallLocation, BallVelocity;
	Clone(string slug, string displayName, int carBody)
	{
		this->Slug = slug;
        this->CarBody = carBody;
		this->DisplayName = displayName;
        // Must execute in main game thread (since this can be triggered by socket event or other event in alt thread)
		Global::GameWrapper->Execute([this](...){
			ServerWrapper server = Global::GameWrapper->GetGameEventAsServer();
			if (!server.IsNull())
			{
                // Spawn car clone
				server.SpawnBot(this->CarBody, this->DisplayName);
                auto pris = server.GetPRIs();
                this->PriIdx = pris.Count() - 1;
                auto botPri = pris.Get(this->PriIdx); if (botPri.IsNull()) return;
                auto botUID = UniqueIDWrapper::FromEpicAccountID(this->Slug, this->PriIdx, OnlinePlatform_Epic);
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
                // Spawn ball clone
                // if (!Global::GameWrapper->IsInCustomTraining()) 
                // {
                    server.SpawnBall(Vector{0, 0, 1000}, true, false);
                    auto balls = server.GetGameBalls();
                    this->BallIdx = balls.Count() - 1;
                    auto botBall = balls.Get(this->BallIdx);
                    // this currently doesn't work for cars unless you apply to both bot and your car but then
                    // you go through walls and doing janky logic around that smelled so fugg it for now
                    if (!Cvar::Get("enable_collisions")->toBool())
                    {
                        botBall.GetCollisionComponent().SetRBChannel(6);
                        botBall.GetCollisionComponent().SetRBCollidesWithChannel(3, 0);
                        botBall.GetCollisionComponent().SetBlockRigidBody2(0);
                    }
                // }
			}
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
                auto botPri = pris.Get(this->PriIdx); if (botPri.IsNull()) return;
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
                auto botBall = balls.Get(this->BallIdx); if (botBall.IsNull()) return;
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
            CloneManager::CloneMap[slug] = new Clone(slug, displayName, carBody);
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
        int priIdx = clone->PriIdx;
        int ballIdx = clone->BallIdx;
        Log::Info("Removing " + slug);
        CloneManager::CloneMap.erase(slug);
        Log::Info("Lookup key removed for " + slug);
        Global::GameWrapper->Execute([priIdx, ballIdx](...){
            auto server = Global::GameWrapper->GetGameEventAsServer();
            if (!server.IsNull())
            {
                auto players = server.GetPlayers();
                if (players.Count() > priIdx)
                {
                    auto botPlayer = players.Get(priIdx);
                    if (!botPlayer.IsNull())
                    {
                        server.RemovePlayer(botPlayer);
                        Log::Info("Player/car destroyed");
                    }
                }
                auto balls = server.GetGameBalls();
                if (balls.Count() > ballIdx)
                {
                    auto botBall = balls.Get(ballIdx);
                    if (!botBall.IsNull())
                    {
                        botBall.DoDestroy();
                        Log::Info("Ball destroyed");
                    }
                }
            }
		});
    }
};
