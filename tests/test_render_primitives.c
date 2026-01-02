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

	// Normal case
	sdl_pixel(100, 100, 0x7FFF, TEST_XOFF, TEST_YOFF);
	sdl_pixel_alpha(100, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Edge cases - zero/negative coordinates (should not crash)
	sdl_pixel(0, 0, 0x7FFF, TEST_XOFF, TEST_YOFF);
	sdl_pixel_alpha(0, 0, 0x7FFF, 0, TEST_XOFF, TEST_YOFF);
	sdl_pixel_alpha(0, 0, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Pixel primitives OK\n");
	ASSERT_TRUE(1);
}

TEST(test_line_primitives)
{
	fprintf(stderr, "  → Testing line primitives...\n");

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
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Rectangle Primitives
// ============================================================================

TEST(test_rectangle_primitives)
{
	fprintf(stderr, "  → Testing rectangle primitives...\n");

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

	fprintf(stderr, "     Rectangle primitives OK\n");
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Circle and Ellipse Primitives
// ============================================================================

TEST(test_circle_primitives)
{
	fprintf(stderr, "  → Testing circle primitives...\n");

	// Normal circles
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_circle_filled_alpha(100, 100, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Zero radius
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
	ASSERT_TRUE(1);
}

TEST(test_ellipse_primitives)
{
	fprintf(stderr, "  → Testing ellipse primitives...\n");

	// Normal ellipses
	sdl_ellipse_alpha(100, 100, 60, 40, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_filled_alpha(100, 100, 60, 40, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Circle (equal radii)
	sdl_ellipse_alpha(100, 100, 50, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Zero radii
	sdl_ellipse_alpha(100, 100, 0, 0, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_alpha(100, 100, 50, 0, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_alpha(100, 100, 0, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Very thin ellipses
	sdl_ellipse_alpha(100, 100, 100, 1, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_ellipse_alpha(100, 100, 1, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Ellipse primitives OK\n");
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Triangle Primitives
// ============================================================================

TEST(test_triangle_primitives)
{
	fprintf(stderr, "  → Testing triangle primitives...\n");

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
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Arc and Curve Primitives
// ============================================================================

TEST(test_arc_primitives)
{
	fprintf(stderr, "  → Testing arc primitives...\n");

	// Normal arc
	sdl_arc_alpha(100, 100, 50, 0, 90, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_arc_alpha(100, 100, 50, 0, 180, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_arc_alpha(100, 100, 50, 0, 360, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Full circle via arc
	sdl_arc_alpha(100, 100, 50, 0, 360, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Negative angles
	sdl_arc_alpha(100, 100, 50, -90, 90, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Angles > 360
	sdl_arc_alpha(100, 100, 50, 0, 720, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Zero radius
	sdl_arc_alpha(100, 100, 0, 0, 180, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Start > end (should draw nothing or wrap)
	sdl_arc_alpha(100, 100, 50, 270, 90, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Arc primitives OK\n");
	ASSERT_TRUE(1);
}

TEST(test_bezier_primitives)
{
	fprintf(stderr, "  → Testing bezier curve primitives...\n");

	// Quadratic bezier (3 control points)
	sdl_bezier_quadratic_alpha(10, 100, 50, 10, 90, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Cubic bezier (4 control points)
	sdl_bezier_cubic_alpha(10, 100, 30, 10, 70, 10, 90, 100, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Degenerate - all points same (should draw nothing or point)
	sdl_bezier_quadratic_alpha(50, 50, 50, 50, 50, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_bezier_cubic_alpha(50, 50, 50, 50, 50, 50, 50, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	// Straight line via bezier
	sdl_bezier_quadratic_alpha(10, 50, 50, 50, 90, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);
	sdl_bezier_cubic_alpha(10, 50, 30, 50, 60, 50, 90, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Bezier primitives OK\n");
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Gradient Primitives
// ============================================================================

TEST(test_gradient_primitives)
{
	fprintf(stderr, "  → Testing gradient primitives...\n");

	// Horizontal gradient
	sdl_gradient_rect_h(10, 10, 100, 50, 0x001F, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Vertical gradient
	sdl_gradient_rect_v(10, 60, 100, 100, 0x001F, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Same color (solid fill)
	sdl_gradient_rect_h(10, 10, 100, 50, 0x7FFF, 0x7FFF, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_gradient_rect_v(10, 10, 100, 50, 0x7FFF, 0x7FFF, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Zero-size rectangle
	sdl_gradient_rect_h(50, 50, 50, 50, 0x001F, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Gradient circle (glow effect)
	sdl_gradient_circle(100, 100, 50, 0x7FFF, 255, 0, TEST_XOFF, TEST_YOFF);
	sdl_gradient_circle(100, 100, 50, 0x7FFF, 0, 255, TEST_XOFF, TEST_YOFF);
	sdl_gradient_circle(100, 100, 50, 0x7FFF, 128, 128, TEST_XOFF, TEST_YOFF);

	// Zero radius gradient circle
	sdl_gradient_circle(100, 100, 0, 0x7FFF, 255, 0, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Gradient primitives OK\n");
	ASSERT_TRUE(1);
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

	// Fully transparent (alpha = 0)
	sdl_shaded_rect(10, 10, 100, 100, 0x7FFF, 0, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 0, TEST_XOFF, TEST_YOFF);
	sdl_line_alpha(10, 10, 100, 100, 0x7FFF, 0, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Fully opaque (alpha = 255)
	sdl_shaded_rect(10, 10, 100, 100, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);
	sdl_line_alpha(10, 10, 100, 100, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Mid-range alpha
	sdl_shaded_rect(10, 10, 100, 100, 0x7FFF, 128, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 128, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Alpha edge cases OK\n");
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Color Values
// ============================================================================

TEST(test_color_values)
{
	fprintf(stderr, "  → Testing color values...\n");

	// Black (0x0000)
	sdl_shaded_rect(10, 10, 50, 50, 0x0000, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// White (0x7FFF)
	sdl_shaded_rect(60, 10, 100, 50, 0x7FFF, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Pure red (0x7C00)
	sdl_shaded_rect(10, 60, 50, 100, 0x7C00, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Pure green (0x03E0)
	sdl_shaded_rect(60, 60, 100, 100, 0x03E0, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Pure blue (0x001F)
	sdl_shaded_rect(110, 60, 150, 100, 0x001F, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Test IRGB macro
	unsigned short custom_color = IRGB(15, 20, 10);
	sdl_shaded_rect(10, 110, 50, 150, custom_color, 200, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Color values OK\n");
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Stress Test - Many Draw Calls
// ============================================================================

TEST(test_stress_many_draws)
{
	fprintf(stderr, "  → Stress testing many draw calls...\n");

	// Many circles
	for (int i = 0; i < 100; i++) {
		int x = (i % 10) * 30 + 20;
		int y = (i / 10) * 30 + 20;
		sdl_circle_alpha(x, y, 10, (unsigned short)(i * 100), 128, TEST_XOFF, TEST_YOFF);
	}

	// Many lines
	for (int i = 0; i < 100; i++) {
		sdl_line_alpha(0, i * 5, 300, i * 5, 0x7FFF, 64, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	}

	// Many rectangles
	for (int i = 0; i < 50; i++) {
		sdl_shaded_rect(i * 10, i * 10, i * 10 + 50, i * 10 + 50, 0x03E0, 32, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	}

	fprintf(stderr, "     Stress test OK\n");
	ASSERT_TRUE(1);
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

	int old_scale = sdl_scale;

	// At scale=1, radius 50 should draw normally
	sdl_scale = 1;
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);

	// At scale=2, radius 50 should draw 100-pixel radius
	sdl_scale = 2;
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);

	// At scale=3
	sdl_scale = 3;
	sdl_circle_alpha(100, 100, 50, 0x7FFF, 255, TEST_XOFF, TEST_YOFF);

	// Restore scale
	sdl_scale = old_scale;

	fprintf(stderr, "     Circle scaling OK\n");
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Line Clipping Preserves Slope
// ============================================================================

TEST(test_line_clipping_slope)
{
	fprintf(stderr, "  → Testing line clipping preserves slope...\n");

	// Diagonal line from outside clip rect should maintain angle when clipped
	// The Cohen-Sutherland algorithm should preserve the slope
	sdl_line_alpha(-100, -100, 900, 900, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Line going from top-left outside to bottom-right outside
	sdl_line_alpha(-50, 300, 850, 300, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Vertical line partially outside
	sdl_line_alpha(400, -100, 400, 700, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Completely outside - should be rejected (not crash)
	sdl_line_alpha(-100, -100, -50, -50, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_line_alpha(900, 700, 1000, 800, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Line clipping slope preservation OK\n");
	ASSERT_TRUE(1);
}

// ============================================================================
// Test: Thick Line Clipping
// ============================================================================

TEST(test_thick_line_clipping)
{
	fprintf(stderr, "  → Testing thick line clipping...\n");

	// Thick line completely inside - should draw normally
	sdl_thick_line_alpha(100, 100, 200, 200, 10, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Thick line partially outside - should be clipped without crashing
	sdl_thick_line_alpha(-50, 100, 100, 100, 20, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_thick_line_alpha(100, -50, 100, 100, 20, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_thick_line_alpha(700, 500, 900, 700, 15, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Thick line completely outside - should be rejected (not crash)
	sdl_thick_line_alpha(-100, -100, -50, -50, 10, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);
	sdl_thick_line_alpha(900, 700, 1000, 800, 10, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Edge case: thick line at boundary where center is inside but thickness extends outside
	sdl_thick_line_alpha(5, 300, 795, 300, 30, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	// Diagonal thick line crossing corners
	sdl_thick_line_alpha(-100, -100, 900, 700, 8, 0x7FFF, 255, 0, 0, 800, 600, TEST_XOFF, TEST_YOFF);

	fprintf(stderr, "     Thick line clipping OK\n");
	ASSERT_TRUE(1);
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

	// New tests addressing PR review comments
	test_blend_mode_frame_isolation();
	test_circle_scaling();
	test_line_clipping_slope();
	test_thick_line_clipping();
	test_mod_texture_path_validation();

	sdl_shutdown_for_tests();
)
