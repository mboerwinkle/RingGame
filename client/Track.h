
// Return 1 if it was able to estimate an impact position, in which case it will update loc appropriately
extern char trackAddShip(int* loc);

// Should be called once per frame, with the camera's position and aim vector
extern void trackResetShips(int *loc, float* aim, int frameCount);
