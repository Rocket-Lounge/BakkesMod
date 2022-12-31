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
    static inline vector<Clone> Clones = {};
    static inline map<string, int> CloneIdx = {};
    static Clone UseClone(string slug, string displayName, int carBody)
    {
        if (!CloneManager::CloneIdx.count(slug) || CloneManager::CloneIdx[slug] == CLONE_DELETED)
        {
            Log::Error("Creating new clone for " + slug + " (" + displayName + ")");
            CloneManager::CloneIdx[slug] = this->Clones.size();
            CloneManager::Clones.push_back(Clone(slug, displayName, carBody));
        }
        return CloneManager::Clones.at(CloneManager::CloneIdx[slug]);
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
        Global::GameWrapper->Execute([](...){
            for (const auto &[slug, clone] : CloneManager::CloneMap)
            {
                CloneManager::DestroyClone(slug);
            }
		});
    }

    static void DestroyClone(string slug)
    {
	public:
    static inline map<string, Clone*> CloneMap = {};
    static Clone* UseClone(string slug, string displayName)
    {
        if (!CloneManager::CloneMap.count(slug) || CloneManager::CloneMap[slug] == NULL)
        {
            Log::Error("Creating new clone for " + slug + " (" + displayName + ")");
            CloneManager::CloneMap[slug] = new Clone(slug, displayName);
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
        Global::GameWrapper->Execute([](...){
            for (const auto &[slug, clone] : CloneManager::CloneMap)
            {
                CloneManager::DestroyClone(slug);
            }
		});
    }

    static void DestroyClone(string slug)
    {
        if (!CloneManager::CloneMap.count(slug)) return;
        Global::GameWrapper->Execute([slug](...){
            auto clone = CloneManager::CloneMap[slug];
            Log::Info("Removing car and ball for " + clone->DisplayName + " (" + slug + ")");
            auto server = Global::GameWrapper->GetGameEventAsServer();
            if (!server.IsNull())
            {
                auto balls = server.GetGameBalls();
                auto players = server.GetPlayers();
                auto botPlayer = players.Get(clone->PriIdx);
                server.RemovePlayer(botPlayer);
                balls.Get(clone->BallIdx).DoDestroy();
                Log::Info("Car and ball models destroyed for " + clone->DisplayName + " (" + slug + ")");
            }
            CloneManager::CloneMap.erase(slug);
            Log::Info("Lookup key removed for " + clone->DisplayName + " (" + slug + ")");
		});
    }
};
