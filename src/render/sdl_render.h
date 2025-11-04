/**
 * SDL Render Module Header
 * 
 * This header exports the SDL-based rendering functions that replace
 * the OpenGL rendering system.
 */

#ifndef SDL_RENDER_H
#define SDL_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <SDL3/SDL.h>

/**
 * Initializes the SDL graphics system
 * 
 * @param renderer - SDL_Renderer instance from main SDL initialization
 * @param sx - Screen width in pixels
 * @param sy - Screen height in pixels
 * @param genzoneid - Memory zone ID for allocations
 * @param isClient - Whether this is a client (1) or server (0)
 * @return 0 on success, non-zero on failure
 */
int initGraphicsHandleSDL(SDL_Renderer *renderer, int sx, int sy, int genzoneid, int isClient);

/**
 * Main rendering function
 * Renders all sprites, animated sprites, and UI elements to the screen
 * 
 * @return 0 on success, non-zero on failure
 */
int renderSDL();

/**
 * Cleanup function to free all SDL resources
 * Should be called before shutting down SDL
 */
void cleanupSDLRenderer();

/**
 * Draws a sprite at specified world position with rotation
 * 
 * @param spriteID - ID of the sprite to render
 * @param x - World X coordinate
 * @param y - World Y coordinate  
 * @param rect - Sprite rectangle [offsetX, offsetY, width, height]
 * @param angle - Rotation angle in radians
 */
void drawSpriteSDL(int spriteID, float x, float y, float rect[4], float angle);

#ifdef __cplusplus
}
#endif

#endif // SDL_RENDER_H



