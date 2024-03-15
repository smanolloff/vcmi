# VCMI Project (AI fork)

This is a fork of VCMI which adapts the game for AI development purposes and is
part of the [vcmi-gym](https://github.com/smanolloff/vcmi-gym) project.

<img src="demo.gif" alt="demo">

## Project state

<p align="center"><img src="Under-Construction.png" alt="UNDER CONSTRUCTION" width="300" height="250"></p>

For now, it's mostly quick-and-dirty patches applied around VCMI's codebase
to enable various features needed by
[vcmi-gym](https://github.com/smanolloff/vcmi-gym).

Here is a list of features I would like to have implemented:

| Feature | Status | Rationale |
| ------- | ------ | --------- |
| Headless mode | ✅ | Disabling GUI improves performance |
| Multi-VCMI mode | ✅ | Running multiple VCMI processes simultaneously is essential for efficient training |
| Only battles | ✅ | Anything not related to battles is out of scope |
| Quick restarts | ✅ | Re-starting only the battle instead of the entire game improves performance |
| Action injection | ✅ | The actions to take are produced by separate program and must be passed to VCMI for execution |
| State reporting | ✅ | VCMI must collect important aspects of the battle's state and communicate them to a separate program |
| Map hot-swap | ❌ | Changing the map without restarting VCMI would improve training performance |
| Battlefield hot-swap | ❌ | Changing the battlefield terrain without re-starting VCMI on a new map would improve performance|
| Army hot-swap | ❌ | Changing the army compositions without re-starting VCMI on a new map would improve performance |

Currently, I am mostly focused on training battle AIs as part of
[vcmi-gym](https://github.com/smanolloff/vcmi-gym) which is unfortunately quite
when many of the features above are still not implemented.

## Documentation

* For MacOS, please refer to [this setup guide](./setup_macos.md)
* For Linux/Ubuntu OS, please refer to [this setup guide](./setup_ubuntu.md)

Contributions for a Windows-based setup are welcome.

There's actually a lot of [PlantUML](https://plantuml.com/) diagrams for VCMI
which I have developed over the last few months in an effort to better
understand how it works, but I did not have time to organize them in a pretty
manner (there are some NN architecture diagrams for the vcmi-gym project there
as well.). Feel free to check them out [here](./_notes/diagrams) anyway.

_It ain't much, but it's honest work._ ;)

## Contributing

You are more than welcome to help with this project.

I will be most grateful if you can help with the features listed above, as I
have been unable to implement them all.

If you decide to contribute, please follow these
[guidelines](https://github.com/smanolloff/vcmi-gym).
