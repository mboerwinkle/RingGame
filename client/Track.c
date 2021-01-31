#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int frameCount = 0;
static char hasShip = 0;
static int myPos[3];
static int shipTotal = 0;
static int shipCount = 0;
static int stableShipTotal = 0; // Number of ships that have a reliable velocity
static int (*cachedPositions)[3] = NULL;
static int (*cachedVels)[3] = NULL;
static float aim[3];
// Return 1 if it was able to estimate an impact position, in which case it will update loc appropriately
extern char trackAddShip(int* loc) {
	int shipIx = shipCount;
	shipCount++;
	if (shipCount > shipTotal) {
		cachedPositions = realloc(cachedPositions, sizeof(int[3]) * shipCount);
		cachedVels = realloc(cachedVels, sizeof(int[3]) * shipCount);
		shipTotal = shipCount;
		memcpy(cachedPositions[shipIx], loc, sizeof(int[3]));
		return 0;
	}

	if (shipCount > stableShipTotal) {
		assert(frameCount == 0);
		return 0;
	}

	int* vel = cachedVels[shipIx];
	int* cached = cachedPositions[shipIx];
	if (frameCount) {
		for (int i = 0; i < 3; i++) vel[i] = (loc[i] - cached[i]) / frameCount;
	}

	float dist = 0;
	float speed = 200; // Speed from the assets manifest, divided by the framerate in the server code. TODO Shouldn't be hardcoded
	if (hasShip) {
		for (int i = 0; i < 3; i++) {
			dist += aim[i] * (cached[i] - myPos[i]);
			speed -= aim[i] * vel[i];
		}
	}

	if (frameCount) {
		memcpy(cached, loc, sizeof(int[3]));
	}

	if (dist <= 0) return 0;
	float time = dist/speed;
	for (int i = 0; i < 3; i++) loc[i] += vel[i] * time;
	return 1;
}

void trackResetShips(int *camera, float* newAim, int newFrameCount) {
	memcpy(aim, newAim, sizeof(aim));
	frameCount = newFrameCount;

	if (shipCount < shipTotal) {
		cachedPositions = realloc(cachedPositions, sizeof(int[3]) * shipCount);
		cachedVels = realloc(cachedVels, sizeof(int[3]) * shipCount);
		shipTotal = shipCount;
	}
	shipCount = 0;
	if (frameCount) stableShipTotal = shipTotal;

	float best = 12e6; // 2K unit radius
	int myShipIx = -1;
	for (int i = 0; i < shipTotal; i++) {
		float score = 0;
		float tmp;
		for (int j = 0; j < 3; j++) {
			tmp = (float)(camera[j] - cachedPositions[i][j]);
			score += tmp*tmp;
		}
		if (score < best) {
			best = score;
			myShipIx = i;
		}
	}
	if (myShipIx == -1) {
		hasShip = 0;
		return;
	}
	hasShip = 1;
	memcpy(myPos, cachedPositions[myShipIx], sizeof(myPos));
}
