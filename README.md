# Rocket Lounge BakkesMod Plugin

Multiplayer freeplay + spectator mode

## Getting Started

Run the command below on initial setup.

```
.\Setup.ps1
```

If you're using Visual Studio you can trigger builds with `Ctrl + B`.

If you're building from command line, you can use `msbuild` in the root of the repo.

```
msbuild RocketLounge.sln
```

## Contributing

You will notice a lot of `gameWrapper->Execute()` wrappings. This is due to the fact that [server manipulation must occur in the main game thread](https://discord.com/channels/862068148328857700/862081441080410143/886347574339059742). Settings GUI, Socket.io events, and other tasks run in detached threads to prevent performance disruption and thus must be wrapped to ensure they don't crash your game.

### To Do

- PRI Idx moves when destroying (WIP assigning UID to PRI)
- Data sanitization
- Only emit ball data on hits? Might make touching the ball of others feel more organic. If we emit slug of who touched it, slug of ball owner, and location etc data it might feel cleaner
- Prevent car/ball collisions
- Semi-transparency for car/ball
- Spectator mode/takeover POV of others
- Crashing from GUI and pausing? Maybe need to stop updating clone when paused

### Helpful Links

- [Items List](https://github.com/RLBot/RLBotGUI/blob/master/rlbot_gui/gui/csv/items.csv)
- [Map Measurements](https://github.com/RLBot/RLBot/wiki/Useful-Game-Values)
