/*
 * Render Primitives Tests - Verify new drawing functions
 *
 * Tests that all rendering primitives work without crashing and handle
 * edge cases correctly. These are functional tests (no visual verification).
 *
 * Note: Tests use SDL-level functions (sdl_*) directly since those contain
 * the actual implementation. The render_* wrappers are thin pass-throughs.
 */

#include "../src/astonia.h"
#include "../src/sdl/sdl_private.h"
#include "../src/sdl/sdl.h"
#include "test.h"

#include <string.h>
#include <stdio.h>

// Offsets used by render.c (we use 0,0 for tests)
#define TEST_XOFF 0
#define TEST_YOFF 0

// IRGB macro for 15-bit color (game format, not 32-bit SDL format)
#undef IRGB
#define IRGB(r, g, b) (((r) << 10) | ((g) << 5) | ((b) << 0))

// ============================================================================
// Test: Basic Primitives (pixel, line)
// ============================================================================

TEST(test_pixel_primitives)
{
	fprintf(stderr, "  → Testing pixel primitives...\n");

	sdl_test_reset_render_counters();

	// Normal case
	sdl_pixel(100, 100, 0x7FFF, TEST_XOFF, TEST_YOFF);
	sdl_pixel_alpha(100, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Edge cases - zero/negative coordinates (should not crash)
	sdl_pixel(0, 0, 0x7FFF, TEST_XOFF, TEST_YOFF);
	sdl_pixel_alpha(0, 0, 0x7FFF, 0, TEST_XOFF, TEST_YOFF);
	sdl_pixel_alpha(0, 0, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);

	// Verify render calls were made (points are rendered via SDL_RenderPoints)
	ASSERT_TRUE(sdl_test_get_render_point_count() >= 5);
	ASSERT_TRUE(sdl_test_get_set_draw_color_count() >= 5);

	fprintf(stderr, "     Pixel primitives OK\n");
}

TEST(test_line_primitives)
{
	fprintf(stderr, "  → Testing line primitives...\n");

	// Line functions verified by code inspection:
	// - sdl_line: clips coordinates, calls SDL_RenderLine
	// - sdl_line_alpha: uses clip_line() for proper clipping, calls SDL_RenderLine
	// - sdl_line_aa: Xiaolin Wu algorithm, draws individual anti-aliased points
	// - sdl_thick_line_alpha: uses SDL_RenderGeometry for GPU-accelerated quad
	// This test verifies they don't crash with various inputs.

	// Normal lines
	sdl_line(10, 10, 100, 100, 0x7FFF, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_line_alpha(10, 10, 100, 100, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_line_aa(10, 10, 100, 100, 0x7FFF, 200, TEST_XOFF, TEST_YOFF);
	sdl_thick_line_alpha(10, 10, 100, 100, 3, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Horizontal and vertical lines
	sdl_line(10, 50, 100, 50, 0x7FFF, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_line(50, 10, 50, 100, 0x7FFF, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Zero-length line (same start and end)
	sdl_line(50, 50, 50, 50, 0x7FFF, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_thick_line_alpha(50, 50, 50, 50, 5, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Various thicknesses
	sdl_thick_line_alpha(10, 10, 100, 100, 1, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_thick_line_alpha(10, 10, 100, 100, 10, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_thick_line_alpha(10, 10, 100, 100, 0, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Line primitives OK\n");
}

// ============================================================================
// Test: Rectangle Primitives
// ============================================================================

TEST(test_rectangle_primitives)
{
	fprintf(stderr, "  → Testing rectangle primitives...\n");

	sdl_test_reset_render_counters();

	// Normal rectangles
	sdl_rect(10, 10, 100, 100, 0x7FFF, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_shaded_rect(10, 10, 100, 100, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_rect_outline_alpha(10, 10, 100, 100, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Rounded rectangles
	sdl_rounded_rect_alpha(10, 10, 100, 100, 5, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_rounded_rect_filled_alpha(10, 10, 100, 100, 5, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Zero-size rectangle
	sdl_rect(50, 50, 50, 50, 0x7FFF, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_rounded_rect_alpha(50, 50, 50, 50, 5, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Very large corner radius (larger than rect size)
	sdl_rounded_rect_alpha(10, 10, 50, 50, 100, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_rounded_rect_filled_alpha(10, 10, 50, 50, 100, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Zero corner radius (should act like regular rect)
	sdl_rounded_rect_alpha(10, 10, 100, 100, 0, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Verify render calls were made (fills + outlines)
	ASSERT_TRUE(sdl_test_get_render_fill_rect_count() >= 2);
	ASSERT_TRUE(sdl_test_get_render_total_count() >= 5);

	fprintf(stderr, "     Rectangle primitives OK\n");
}

// ============================================================================
// Test: Circle and Ellipse Primitives
// ============================================================================

TEST(test_circle_primitives)
{
	fprintf(stderr, "  → Testing circle primitives...\n");

	sdl_test_reset_render_counters();

	// Normal circles
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_circle_filled_alpha(100, 100, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Verify circle drawing generates render calls
	// sdl_circle_alpha uses SDL_RenderPoints, sdl_circle_filled_alpha uses SDL_RenderGeometry
	ASSERT_TRUE(sdl_test_get_render_point_count() >= 1);
	ASSERT_TRUE(sdl_test_get_render_geometry_count() >= 1);

	// Zero radius - should early return without crash
	sdl_circle_alpha(100, 100, 0, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_circle_filled_alpha(100, 100, 0, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Large radius
	sdl_circle_alpha(100, 100, 500, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_circle_filled_alpha(100, 100, 500, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Ring (annulus)
	sdl_ring_alpha(200, 200, 30, 50, 0, 360, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ring_alpha(200, 200, 30, 50, 45, 135, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ring_alpha(200, 200, 0, 50, 0, 360, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Ring with inverted radii
	sdl_ring_alpha(200, 200, 50, 30, 0, 360, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Circle primitives OK\n");
}

TEST(test_ellipse_primitives)
{
	fprintf(stderr, "  → Testing ellipse primitives...\n");

	sdl_test_reset_render_counters();

	// Normal ellipses
	sdl_ellipse_alpha(100, 100, 60, 40, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_filled_alpha(100, 100, 60, 40, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Verify ellipse drawing generates render calls
	ASSERT_TRUE(sdl_test_get_render_point_count() >= 1);
	ASSERT_TRUE(sdl_test_get_render_geometry_count() >= 1);

	// Circle (equal radii)
	sdl_ellipse_alpha(100, 100, 50, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Zero radii - should early return without crash
	sdl_ellipse_alpha(100, 100, 0, 0, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_alpha(100, 100, 50, 0, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_alpha(100, 100, 0, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Very thin ellipses
	sdl_ellipse_alpha(100, 100, 100, 1, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_alpha(100, 100, 1, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Ellipse primitives OK\n");
}

// ============================================================================
// Test: Triangle Primitives
// ============================================================================

TEST(test_triangle_primitives)
{
	fprintf(stderr, "  → Testing triangle primitives...\n");

	// Triangle functions verified by code inspection:
	// - sdl_triangle_alpha: draws 3 lines using SDL_RenderLines
	// - sdl_triangle_filled_alpha: uses SDL_RenderGeometry with 3 vertices
	// This test verifies they don't crash with various inputs.

	// Normal triangle
	sdl_triangle_alpha(50, 10, 10, 90, 90, 90, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_triangle_filled_alpha(50, 10, 10, 90, 90, 90, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Degenerate triangle (line)
	sdl_triangle_alpha(10, 10, 50, 50, 90, 90, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_triangle_filled_alpha(10, 10, 50, 50, 90, 90, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Degenerate triangle (point)
	sdl_triangle_alpha(50, 50, 50, 50, 50, 50, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_triangle_filled_alpha(50, 50, 50, 50, 50, 50, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Various orderings (clockwise, counter-clockwise)
	sdl_triangle_filled_alpha(10, 10, 90, 10, 50, 90, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_triangle_filled_alpha(10, 10, 50, 90, 90, 10, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Triangle primitives OK\n");
}

// ============================================================================
// Test: Arc and Curve Primitives
// ============================================================================

TEST(test_arc_primitives)
{
	fprintf(stderr, "  → Testing arc primitives...\n");

	// Arc function verified by code inspection:
	// - Computes points along arc using cos/sin
	// - Uses SDL_RenderLines to draw connected line segments
	// - Early returns if radius <= 0
	// This test verifies they don't crash with various inputs.

	// Normal arc
	sdl_arc_alpha(100, 100, 50, 0, 90, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_arc_alpha(100, 100, 50, 0, 180, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_arc_alpha(100, 100, 50, 0, 360, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Full circle via arc
	sdl_arc_alpha(100, 100, 50, 0, 360, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Negative angles (should be normalized without crash)
	sdl_arc_alpha(100, 100, 50, -90, 90, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Angles > 360 (should be normalized via modulo)
	sdl_arc_alpha(100, 100, 50, 0, 720, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Zero radius - should early return without crash
	sdl_arc_alpha(100, 100, 0, 0, 180, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Start > end (should handle wrap-around)
	sdl_arc_alpha(100, 100, 50, 270, 90, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Arc primitives OK\n");
}

TEST(test_bezier_primitives)
{
	fprintf(stderr, "  → Testing bezier curve primitives...\n");

	// Bezier functions verified by code inspection:
	// - Compute points along curve using parametric evaluation
	// - Use SDL_RenderLines to draw connected line segments
	// This test verifies they don't crash with various inputs.

	// Quadratic bezier (3 control points)
	sdl_bezier_quadratic_alpha(10, 100, 50, 10, 90, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Cubic bezier (4 control points)
	sdl_bezier_cubic_alpha(10, 100, 30, 10, 70, 10, 90, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Degenerate - all points same (should draw single point or nothing)
	sdl_bezier_quadratic_alpha(50, 50, 50, 50, 50, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_bezier_cubic_alpha(50, 50, 50, 50, 50, 50, 50, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Straight line via bezier (control points collinear)
	sdl_bezier_quadratic_alpha(10, 50, 50, 50, 90, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_bezier_cubic_alpha(10, 50, 30, 50, 60, 50, 90, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Bezier primitives OK\n");
}

// ============================================================================
// Test: Gradient Primitives
// ============================================================================

TEST(test_gradient_primitives)
{
	fprintf(stderr, "  → Testing gradient primitives...\n");

	sdl_test_reset_render_counters();

	// Horizontal gradient (uses SDL_RenderGeometry with 4 vertices)
	sdl_gradient_rect_h(10, 10, 100, 50, 0x001F, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Vertical gradient
	sdl_gradient_rect_v(10, 60, 100, 100, 0x001F, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Verify gradient rendering generates geometry calls
	ASSERT_TRUE(sdl_test_get_render_geometry_count() >= 2);

	// Same color (solid fill) - still valid gradient
	sdl_gradient_rect_h(10, 10, 100, 50, 0x7FFF, 0x7FFF, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_gradient_rect_v(10, 10, 100, 50, 0x7FFF, 0x7FFF, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Zero-size rectangle - should early return without crash
	sdl_gradient_rect_h(50, 50, 50, 50, 0x001F, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Gradient circle (glow effect)
	sdl_gradient_circle(100, 100, 50, 0x7FFF, 255, 0, TEST_XOFF, TEST_YOFF);
	sdl_gradient_circle(100, 100, 50, 0x7FFF, 0, 255, TEST_XOFF, TEST_YOFF);
	sdl_gradient_circle(100, 100, 50, 0x7FFF, 128, 128, TEST_XOFF, TEST_YOFF);

	// Zero radius gradient circle - should early return without crash
	sdl_gradient_circle(100, 100, 0, 0x7FFF, 255, 0, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Gradient primitives OK\n");
}

// ============================================================================
// Test: Blend Mode Control
// ============================================================================

TEST(test_blend_mode)
{
	fprintf(stderr, "  → Testing blend mode control...\n");

	// Save original mode
	int original_mode = sdl_get_blend_mode();

	// Test all blend modes (constants from game.h, but we define locally for test)
	#define BLEND_NORMAL   0
	#define BLEND_ADDITIVE 1
	#define BLEND_MOD      2
	#define BLEND_MUL      3
	#define BLEND_NONE     4

	sdl_set_blend_mode(BLEND_NORMAL);
	ASSERT_EQ_INT(BLEND_NORMAL, sdl_get_blend_mode());

	sdl_set_blend_mode(BLEND_ADDITIVE);
	ASSERT_EQ_INT(BLEND_ADDITIVE, sdl_get_blend_mode());

	sdl_set_blend_mode(BLEND_MOD);
	ASSERT_EQ_INT(BLEND_MOD, sdl_get_blend_mode());

	sdl_set_blend_mode(BLEND_MUL);
	ASSERT_EQ_INT(BLEND_MUL, sdl_get_blend_mode());

	sdl_set_blend_mode(BLEND_NONE);
	ASSERT_EQ_INT(BLEND_NONE, sdl_get_blend_mode());

	// Draw with different blend modes
	sdl_set_blend_mode(BLEND_ADDITIVE);
	sdl_circle_filled_alpha(100, 100, 30, 0x7C00, 128, TEST_XOFF, TEST_YOFF);

	sdl_set_blend_mode(BLEND_MOD);
	sdl_circle_filled_alpha(100, 100, 30, 0x03E0, 128, TEST_XOFF, TEST_YOFF);

	sdl_set_blend_mode(BLEND_MUL);
	sdl_circle_filled_alpha(100, 100, 30, 0x001F, 128, TEST_XOFF, TEST_YOFF);

	// Restore original
	sdl_set_blend_mode(original_mode);
	ASSERT_EQ_INT(original_mode, sdl_get_blend_mode());

	fprintf(stderr, "     Blend mode control OK\n");

	#undef BLEND_NORMAL
	#undef BLEND_ADDITIVE
	#undef BLEND_MOD
	#undef BLEND_MUL
	#undef BLEND_NONE
}

// ============================================================================
// Test: Alpha Channel Edge Cases
// ============================================================================

TEST(test_alpha_edge_cases)
{
	fprintf(stderr, "  → Testing alpha channel edge cases...\n");

	sdl_test_reset_render_counters();

	// Fully transparent (alpha = 0) - should still make render calls
	sdl_shaded_rect(10, 10, 100, 100, 0x7FFF, 0, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 0, TEST_XOFF, TEST_YOFF);
	sdl_line_alpha(10, 10, 100, 100, 0x7FFF, 0, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	int transparent_calls = sdl_test_get_render_total_count();
	ASSERT_TRUE(transparent_calls >= 3);

	// Fully opaque (alpha = 255)
	sdl_shaded_rect(10, 10, 100, 100, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);
	sdl_line_alpha(10, 10, 100, 100, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Mid-range alpha
	sdl_shaded_rect(10, 10, 100, 100, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Verify all calls were made (transparent, opaque, and mid-range)
	ASSERT_TRUE(sdl_test_get_render_total_count() >= 8);

	fprintf(stderr, "     Alpha edge cases OK\n");
}

// ============================================================================
// Test: Color Values
// ============================================================================

TEST(test_color_values)
{
	fprintf(stderr, "  → Testing color values and IRGB macro...\n");

	// Test IRGB macro encoding (15-bit color format: RRRRR GGGGG BBBBB)
	// Red is bits 14-10, Green is bits 9-5, Blue is bits 4-0

	// Black (all zeros)
	ASSERT_EQ_INT(0x0000, IRGB(0, 0, 0));

	// White (all max)
	ASSERT_EQ_INT(0x7FFF, IRGB(31, 31, 31));

	// Pure red (0x7C00 = 11111 00000 00000)
	ASSERT_EQ_INT(0x7C00, IRGB(31, 0, 0));

	// Pure green (0x03E0 = 00000 11111 00000)
	ASSERT_EQ_INT(0x03E0, IRGB(0, 31, 0));

	// Pure blue (0x001F = 00000 00000 11111)
	ASSERT_EQ_INT(0x001F, IRGB(0, 0, 31));

	// Custom color verification
	unsigned short custom_color = IRGB(15, 20, 10);
	// 15 << 10 = 0x3C00, 20 << 5 = 0x0280, 10 = 0x000A
	// Total = 0x3C00 | 0x0280 | 0x000A = 0x3E8A
	ASSERT_EQ_INT(0x3E8A, custom_color);

	// Verify colors render without crash
	sdl_test_reset_render_counters();
	sdl_shaded_rect(10, 10, 50, 50, 0x0000, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_shaded_rect(60, 10, 100, 50, 0x7FFF, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_shaded_rect(10, 60, 50, 100, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_shaded_rect(60, 60, 100, 100, 0x03E0, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_shaded_rect(110, 60, 150, 100, 0x001F, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_shaded_rect(10, 110, 50, 150, custom_color, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	ASSERT_TRUE(sdl_test_get_render_fill_rect_count() >= 6);

	fprintf(stderr, "     Color values OK\n");
}

// ============================================================================
// Test: Stress Test - Many Draw Calls
// ============================================================================

TEST(test_stress_many_draws)
{
	fprintf(stderr, "  → Stress testing many draw calls...\n");

	sdl_test_reset_render_counters();

	// Many circles (100 circles)
	for (int i = 0; i < 100; i++) {
		int x = (i % 10) * 30 + 20;
		int y = (i / 10) * 30 + 20;
		sdl_circle_alpha(x, y, 10, (unsigned short)(i * 100), 128, TEST_XOFF, TEST_YOFF);
	}

	// Many lines (100 lines)
	for (int i = 0; i < 100; i++) {
		sdl_line_alpha(0, i * 5, 300, i * 5, 0x7FFF, 64, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	}

	// Many rectangles (50 rectangles)
	for (int i = 0; i < 50; i++) {
		sdl_shaded_rect(i * 10, i * 10, i * 10 + 50, i * 10 + 50, 0x03E0, 32, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	}

	// Verify all render calls were made
	// 100 circles + 100 lines + 50 rects = 250 minimum total render calls
	ASSERT_TRUE(sdl_test_get_render_point_count() >= 100);  // circles use points
	ASSERT_TRUE(sdl_test_get_render_line_count() >= 100);   // lines
	ASSERT_TRUE(sdl_test_get_render_fill_rect_count() >= 50); // rectangles

	fprintf(stderr, "     Stress test OK\n");
}

// ============================================================================
// Test: Blend Mode Frame Isolation
// ============================================================================

TEST(test_blend_mode_frame_isolation)
{
	fprintf(stderr, "  → Testing blend mode frame isolation...\n");

	#define BLEND_NORMAL    0
	#define BLEND_ADDITIVE  1

	// Set blend mode to additive
	sdl_set_blend_mode(BLEND_ADDITIVE);
	ASSERT_EQ_INT(BLEND_ADDITIVE, sdl_get_blend_mode());

	// Draw something with additive blending
	sdl_circle_filled_alpha(100, 100, 30, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Reset blend mode (simulating frame boundary)
	sdl_reset_blend_mode();

	// Blend mode should be reset to BLEND_NORMAL
	ASSERT_EQ_INT(BLEND_NORMAL, sdl_get_blend_mode());

	fprintf(stderr, "     Blend mode frame isolation OK\n");

	#undef BLEND_NORMAL
	#undef BLEND_ADDITIVE
}

// ============================================================================
// Test: Circle Scaling
// ============================================================================

extern int sdl_scale;

TEST(test_circle_scaling)
{
	fprintf(stderr, "  → Testing circle scaling with sdl_scale...\n");

	// Actual correctness verified by code inspection:
	// - sdl_circle_alpha(): sr = radius * sdl_scale
	// - Algorithm uses sr throughout (x = sr, d = 1 - sr)
	// - sdl_circle_filled_alpha(): fsr = radius * sdl_scale
	// - Vertices calculated with fsr
	// This test verifies they don't crash at various scales.

	int old_scale = sdl_scale;

	sdl_scale = 1;
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);
	sdl_circle_filled_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);

	sdl_scale = 2;
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);
	sdl_circle_filled_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);

	sdl_scale = old_scale;

	fprintf(stderr, "     Circle scaling OK\n");
}

// ============================================================================
// Test: Line Clipping Preserves Slope
// ============================================================================

TEST(test_line_clipping_slope)
{
	fprintf(stderr, "  → Testing line clipping preserves slope...\n");

	// Test 45-degree diagonal - slope should be preserved after clipping
	int x0 = -100, y0 = -100, x1 = 900, y1 = 900;
	int result = clip_line(&x0, &y0, &x1, &y1, 0, 0, 800, 600);
	ASSERT_EQ_INT(1, result);
	double slope = (double)(y1 - y0) / (double)(x1 - x0);
	ASSERT_TRUE(slope > 0.99 && slope < 1.01);

	// Test horizontal line - y values should remain constant
	x0 = -50; y0 = 300; x1 = 850; y1 = 300;
	result = clip_line(&x0, &y0, &x1, &y1, 0, 0, 800, 600);
	ASSERT_EQ_INT(1, result);
	ASSERT_EQ_INT(300, y0);
	ASSERT_EQ_INT(300, y1);

	// Test vertical line - x values should remain constant
	x0 = 400; y0 = -100; x1 = 400; y1 = 700;
	result = clip_line(&x0, &y0, &x1, &y1, 0, 0, 800, 600);
	ASSERT_EQ_INT(1, result);
	ASSERT_EQ_INT(400, x0);
	ASSERT_EQ_INT(400, x1);

	// Test completely outside - should be rejected
	x0 = -100; y0 = -100; x1 = -50; y1 = -50;
	result = clip_line(&x0, &y0, &x1, &y1, 0, 0, 800, 600);
	ASSERT_EQ_INT(0, result);

	// Test completely outside (other corner)
	x0 = 900; y0 = 700; x1 = 1000; y1 = 800;
	result = clip_line(&x0, &y0, &x1, &y1, 0, 0, 800, 600);
	ASSERT_EQ_INT(0, result);

	fprintf(stderr, "     Line clipping slope preservation OK\n");
}

// ============================================================================
// Test: Thick Line Clipping
// ============================================================================

TEST(test_thick_line_clipping)
{
	fprintf(stderr, "  → Testing thick line clipping...\n");

	// Thick lines use clip_line() on center line (tested above in test_line_clipping_slope)
	// This verifies they don't crash when clipping is needed

	sdl_test_reset_render_counters();

	// Thick line partially outside - should be clipped and drawn
	sdl_thick_line_alpha(-50, 100, 850, 100, 20, 0x7FFF, 255,
	                     0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Thick line completely outside - should be rejected
	sdl_thick_line_alpha(-100, -100, -50, -50, 10, 0x7FFF, 255,
	                     0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Note: The key assertion is that we don't crash.
	// Clipping behavior is verified in test_line_clipping_slope.

	// Diagonal thick line crossing corners
	sdl_thick_line_alpha(-100, -100, 900, 700, 8, 0x7FFF, 255,
	                     0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Thick line clipping OK\n");
}

// ============================================================================
// Test: Mod Texture Path Validation Security
// ============================================================================

TEST(test_mod_texture_path_validation)
{
	fprintf(stderr, "  → Testing mod texture path validation security...\n");

	// These should be rejected (absolute paths)
	ASSERT_EQ_INT(-1, sdl_load_mod_texture("/etc/passwd"));
	ASSERT_EQ_INT(-1, sdl_load_mod_texture("C:\\Windows\\system32\\config\\sam"));

	// These should be rejected (path traversal)
	ASSERT_EQ_INT(-1, sdl_load_mod_texture("../../../etc/passwd"));
	ASSERT_EQ_INT(-1, sdl_load_mod_texture("..\\..\\..\\Windows\\system.ini"));

	// These should be rejected (bypass attempts)
	ASSERT_EQ_INT(-1, sdl_load_mod_texture("..././etc/passwd"));
	ASSERT_EQ_INT(-1, sdl_load_mod_texture("....//etc/passwd"));
	ASSERT_EQ_INT(-1, sdl_load_mod_texture("foo/../../../etc/passwd"));

	// Empty and null paths should be rejected
	ASSERT_EQ_INT(-1, sdl_load_mod_texture(""));
	ASSERT_EQ_INT(-1, sdl_load_mod_texture(NULL));

	fprintf(stderr, "     Path validation security OK\n");
}

// ============================================================================
// Main Test Suite
// ============================================================================

TEST_MAIN(
	if (!sdl_init_for_tests()) {
		fprintf(stderr, "FATAL: Failed to initialize SDL for tests\n");
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "\n=== Render Primitives Tests ===\n\n");

	test_pixel_primitives();
	test_line_primitives();
	test_rectangle_primitives();
	test_circle_primitives();
	test_ellipse_primitives();
	test_triangle_primitives();
	test_arc_primitives();
	test_bezier_primitives();
	test_gradient_primitives();
	test_blend_mode();
	test_alpha_edge_cases();
	test_color_values();
	test_stress_many_draws();

	test_blend_mode_frame_isolation();
	test_circle_scaling();
	test_line_clipping_slope();
	test_thick_line_clipping();
	test_mod_texture_path_validation();

	sdl_shutdown_for_tests();
)
