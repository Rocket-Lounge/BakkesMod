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

P0:
- Cloned PRI UIDs (WIP)
- Crash if another player hits your ball into goal causing replay
- Player cloned into your session disconnecting while your GUI is open causes crash
    - Data sanitization?
    - Disconnect event?

P1:
- No boost trail, set boost amount?
- Spectator mode/takeover POV of others
- Only emit ball data on hits for smoother experience?

P2:
- Prevent car/ball collisions
- Semi-transparency for car/ball

Chat wont pop in custom training

### Helpful Links

- [Items List](https://github.com/RLBot/RLBotGUI/blob/master/rlbot_gui/gui/csv/items.csv)
- [Map Measurements](https://github.com/RLBot/RLBot/wiki/Useful-Game-Values)
