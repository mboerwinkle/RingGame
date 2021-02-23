# RingGame
## Fetching Assets
Assets are not maintained in this repository. To fetch assets, run `assets.sh`.
## Server
### Dependencies
The server relies on mboerwinkle/hypercubeCollide. `git clone https://github.com/mboerwinkle/hypercubeCollide` in the server directory. Build it with `make`.
### Running
From inside the server directory, run `python3 server.py`.
## Client
### Dependencies
- `GLFW`
- `libepoxy`
- `libutf8proc`
### Building
From inside the client directory, run `make`.
### Running
From inside the client directory, run `./ringgameclient [IP]`. If IP is not specified, localhost is assumed.
## Stargen
Generates randomly placed stars in a json file
## Font3D
Converts a png into triangle information for a font
