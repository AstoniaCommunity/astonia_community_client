#!/usr/bin/env python3
"""
Sprite Variant Extraction Script

Parses sprite.c and extracts sprite variant definitions to JSON format.
Handles all variant patterns including animations, pulses, and color effects.
"""

import re
import json
import sys
from pathlib import Path


def resolve_irgb(match):
    """Resolve IRGB(r, g, b) macro to its integer value."""
    r = int(match.group(1)) & 31
    g = int(match.group(2)) & 31
    b = int(match.group(3)) & 31
    return (r << 10) | (g << 5) | b


def parse_color_value(value_str):
    """Parse a color value, handling IRGB macro and hex/decimal."""
    value_str = value_str.strip()

    # Handle IRGB(r, g, b) macro
    irgb_match = re.match(r'IRGB\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)', value_str)
    if irgb_match:
        return resolve_irgb(irgb_match)

    # Handle hex value
    if value_str.startswith('0x'):
        return int(value_str, 16)

    # Handle decimal
    try:
        return int(value_str)
    except ValueError:
        return None


def extract_function_body(content, pattern):
    """Extract function body using brace counting."""
    match = re.search(pattern, content, re.DOTALL)
    if not match:
        return None

    start = match.end()
    depth = 1
    end = start
    while depth > 0 and end < len(content):
        if content[end] == '{':
            depth += 1
        elif content[end] == '}':
            depth -= 1
        end += 1

    return content[start:end-1]


def parse_trans_charno(content):
    """Extract character variants from _trans_charno function."""
    variants = []

    # Find the _trans_charno function
    func_body = extract_function_body(
        content, r'DLL_EXPORT int _trans_charno\([^)]+\)\s*\{')
    if not func_body:
        print("Warning: Could not find _trans_charno function")
        return variants

    # Split by case statements
    # First, find all case positions
    case_starts = [(m.start(), int(m.group(1))) for m in re.finditer(r'case\s+(\d+):', func_body)]

    for i, (start, case_id) in enumerate(case_starts):
        # Find the end of this case (start of next case or end of switch)
        if i + 1 < len(case_starts):
            end = case_starts[i + 1][0]
        else:
            # Last case - find the default or end of switch
            default_match = re.search(r'default:', func_body[start:])
            if default_match:
                end = start + default_match.start()
            else:
                end = len(func_body)

        case_body = func_body[start:end]

        # Extract the body after "case N:"
        body_match = re.match(r'case\s+\d+:\s*(.*)', case_body, re.DOTALL)
        if not body_match:
            continue

        body = body_match.group(1)

        # Find the break statement and extract comment
        break_match = re.search(r'break;\s*(?://\s*(.*))?', body)
        if break_match:
            body = body[:break_match.start()]
            comment = break_match.group(1).strip() if break_match.group(1) else None
        else:
            comment = None
            # Check for fall-through comment
            if '//' in body:
                comment_match = re.search(r'//\s*(.+?)$', body, re.MULTILINE)
                if comment_match:
                    comment = comment_match.group(1).strip()

        variant = extract_character_variant(case_id, body.strip(), comment)
        if variant:
            variants.append(variant)

    return variants


def extract_character_variant(case_id, body, comment):
    """Extract a single character variant from case body."""
    variant = {"id": case_id}

    # Check for complex patterns (attick, helper, if statements)
    if 'attick' in body or 'helper' in body or 'if (' in body:
        # This is a dynamic variant - needs special handling
        variant["_complex"] = True
        variant["_raw"] = body

        # Try to extract dynamic pulse pattern (fire demon style)
        if 'attick & 31' in body or 'attick &' in body:
            # Extract base csprite
            csprite_match = re.search(r'csprite\s*=\s*(\d+)', body)
            if csprite_match:
                variant["base_sprite"] = int(csprite_match.group(1))

            # Extract period from attick & XX
            period_match = re.search(r'attick\s*&\s*(\d+)', body)
            if period_match:
                period = int(period_match.group(1)) + 1
                variant["animation"] = {
                    "type": "pulse",
                    "period": period,
                    "affects": "cr"
                }

                # Try to extract base and amplitude from "cr = X + helper * Y"
                cr_match = re.search(r'cr\s*=\s*(\d+)\s*\+\s*helper\s*\*\s*(\d+)', body)
                if cr_match:
                    variant["animation"]["base"] = int(cr_match.group(1))
                    variant["animation"]["amplitude"] = int(cr_match.group(2)) * (period // 2)

            # Extract other static values
            extract_static_values(variant, body)
            variant["_complex"] = False  # We handled it
    else:
        # Simple static variant
        extract_static_values(variant, body)

    # Add comment
    if comment:
        variant["comment"] = comment

    return variant


def extract_static_values(variant, body):
    """Extract static values from case body."""
    # csprite = X
    csprite_match = re.search(r'csprite\s*=\s*(\d+)', body)
    if csprite_match:
        variant["base_sprite"] = int(csprite_match.group(1))

    # scale = X
    scale_match = re.search(r'scale\s*=\s*(\d+)', body)
    if scale_match:
        variant["scale"] = int(scale_match.group(1))

    # cr = X
    cr_match = re.search(r'(?<!c)cr\s*=\s*(-?\d+)', body)
    if cr_match:
        variant["cr"] = int(cr_match.group(1))

    # cg = X
    cg_match = re.search(r'cg\s*=\s*(-?\d+)', body)
    if cg_match:
        variant["cg"] = int(cg_match.group(1))

    # cb = X
    cb_match = re.search(r'cb\s*=\s*(-?\d+)', body)
    if cb_match:
        variant["cb"] = int(cb_match.group(1))

    # light = X
    light_match = re.search(r'light\s*=\s*(-?\d+)', body)
    if light_match:
        variant["light"] = int(light_match.group(1))

    # sat = X
    sat_match = re.search(r'sat\s*=\s*(-?\d+)', body)
    if sat_match:
        variant["saturation"] = int(sat_match.group(1))

    # shine = X
    shine_match = re.search(r'shine\s*=\s*(-?\d+)', body)
    if shine_match:
        variant["shine"] = int(shine_match.group(1))

    # Handle chained assignments like c1 = c2 = c3 = IRGB(...) or c1 = c2 = c3 = 0xHHHH
    chain_match = re.search(
        r'c1\s*=\s*c2\s*=\s*c3\s*=\s*(IRGB\s*\([^)]+\)|0x[0-9a-fA-F]+|\d+)',
        body
    )
    if chain_match:
        val = parse_color_value(chain_match.group(1))
        if val is not None:
            variant["c1"] = val
            variant["c2"] = val
            variant["c3"] = val
    else:
        # c1, c2, c3 = 0xHHHH or IRGB(...)
        for cname in ['c1', 'c2', 'c3']:
            c_match = re.search(
                rf'{cname}\s*=\s*(IRGB\s*\([^)]+\)|0x[0-9a-fA-F]+|\d+)',
                body
            )
            if c_match:
                val = parse_color_value(c_match.group(1))
                if val is not None:
                    variant[cname] = val


def parse_trans_asprite(content):
    """Extract animated variants from _trans_asprite function."""
    variants = []

    # Find the _trans_asprite function (handles both int and unsigned int return types)
    # Use [\s\S]*? to allow newlines in the parameter list
    func_body = extract_function_body(
        content, r'DLL_EXPORT (?:unsigned )?int _trans_asprite\([\s\S]*?\)\s*\{')
    if not func_body:
        print("Warning: Could not find _trans_asprite function")
        return variants

    # Split by case statements (same approach as trans_charno)
    case_starts = [(m.start(), int(m.group(1))) for m in re.finditer(r'case\s+(\d+):', func_body)]

    # For fall-through handling: find the break for each case
    # by looking forward until we find a break
    for i, (start, case_id) in enumerate(case_starts):
        # Find where this case ends (at next case or default)
        if i + 1 < len(case_starts):
            immediate_end = case_starts[i + 1][0]
        else:
            default_match = re.search(r'default:', func_body[start:])
            if default_match:
                immediate_end = start + default_match.start()
            else:
                immediate_end = len(func_body)

        immediate_body = func_body[start:immediate_end]

        # Check if this case has a break
        body_match = re.match(r'case\s+\d+:\s*(.*)', immediate_body, re.DOTALL)
        if not body_match:
            continue

        body = body_match.group(1).strip()

        # If no break, this is a fall-through - find the break by looking forward
        if 'break;' not in body:
            # Look for the break in subsequent cases
            for j in range(i + 1, len(case_starts)):
                next_start = case_starts[j][0]
                if j + 1 < len(case_starts):
                    next_end = case_starts[j + 1][0]
                else:
                    next_end = len(func_body)

                next_body = func_body[next_start:next_end]
                if 'break;' in next_body:
                    # Found the break - use the full body from this case to the break
                    full_body = func_body[start:next_end]
                    # Extract just the code (skip case labels)
                    body = re.sub(r'case\s+\d+:\s*', '', full_body)
                    break
            else:
                # No break found, skip this case
                continue

        # Skip empty bodies
        if not body.strip():
            continue

        variant = extract_animated_variant(case_id, body)
        if variant:
            variants.append(variant)

    return variants


def extract_animated_variant(case_id, body):
    """Extract a single animated variant from case body."""
    variant = {"id": case_id}

    # Check for break statement
    if 'break;' not in body:
        return None  # Fall-through case

    # Only process up to break
    body = body.split('break;')[0]

    # Extract comment from end of line or after break
    full_body_with_break = body + 'break;'
    comment_match = re.search(r'break;\s*//\s*(.+?)(?:\n|$)', full_body_with_break.replace(body, body + 'break;'))
    if not comment_match:
        comment_match = re.search(r'//\s*(.+?)(?:\n|$)', body)
    if comment_match:
        variant["comment"] = comment_match.group(1).strip()

    # Check for position-aware animation pattern
    pos_pattern = r'mn\s*%\s*MAPDX'
    has_position = re.search(pos_pattern, body) is not None

    # Pattern 0a: Offset formula: sprite = sprite + A - B; (no animation, just remap)
    # e.g., sprite = sprite + 14060 - 59163; means base_sprite = case_id + (14060 - 59163)
    offset_plus_minus = re.search(
        r'sprite\s*=\s*sprite\s*\+\s*(\d+)\s*-\s*(\d+)\s*;',
        body
    )

    # Pattern 0b: Offset formula: sprite = sprite - A + B;
    # e.g., sprite = sprite - 59002 + 14330; means base_sprite = case_id - 59002 + 14330
    offset_minus_plus = re.search(
        r'sprite\s*=\s*sprite\s*-\s*(\d+)\s*\+\s*(\d+)\s*;',
        body
    )

    # Pattern 1: sprite = BASE + (...) % frames (with base number)
    anim_match = re.search(
        r'sprite\s*=\s*(\d+)\s*\+.*?%\s*(\d+)',
        body
    )

    # Pattern 2: sprite = sprite + (...) % frames (base is same as case id)
    sprite_plus_match = re.search(
        r'sprite\s*=\s*sprite\s*\+.*?%\s*(\d+)',
        body
    )

    # Pattern 3: Direct assignment sprite = NUMBER
    direct_match = re.search(r'sprite\s*=\s*(\d+)\s*;', body)

    # Extract animation divisor (handles both "attick / N" and "(attick) / N")
    divisor_match = re.search(r'\(?attick\)?\s*/\s*(\d+)', body)
    divisor = int(divisor_match.group(1)) if divisor_match else 1

    if offset_plus_minus:
        # sprite = sprite + A - B → base = case_id + (A - B)
        a = int(offset_plus_minus.group(1))
        b = int(offset_plus_minus.group(2))
        variant["base_sprite"] = case_id + a - b
    elif offset_minus_plus:
        # sprite = sprite - A + B → base = case_id - A + B
        a = int(offset_minus_plus.group(1))
        b = int(offset_minus_plus.group(2))
        variant["base_sprite"] = case_id - a + b
    elif anim_match:
        variant["base_sprite"] = int(anim_match.group(1))
        frames = int(anim_match.group(2))
        variant["animation"] = {
            "type": "position_cycle" if has_position else "cycle",
            "frames": frames,
            "divisor": divisor
        }
    elif sprite_plus_match:
        # sprite = sprite + ... means the case id IS the base sprite
        variant["base_sprite"] = case_id
        frames = int(sprite_plus_match.group(1))
        variant["animation"] = {
            "type": "position_cycle" if has_position else "cycle",
            "frames": frames,
            "divisor": divisor
        }
    elif direct_match:
        # Direct sprite assignment (possibly a remap)
        variant["base_sprite"] = int(direct_match.group(1))
    else:
        # No clear sprite assignment - use case_id as base
        variant["base_sprite"] = case_id

    # Extract color values
    extract_static_values(variant, body)

    # Check for pulsing color animation: c2 = IRGB(r, g, abs(X - (attick % Y)) [/ div] [+ offset]);
    # Simple pattern: c2 = IRGB(0, 0, abs(31 - (attick % 63)));
    pulse_match = re.search(
        r'(c[123])\s*=\s*IRGB\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*abs\s*\(\s*(\d+)\s*-\s*\(\s*attick\s*%\s*(\d+)\s*\)\s*\)'
        r'(?:\s*/\s*(\d+))?(?:\s*\+\s*(\d+))?\s*\)',
        body
    )
    if pulse_match:
        color_name = pulse_match.group(1)  # c1, c2, or c3
        r = int(pulse_match.group(2))
        g = int(pulse_match.group(3))
        max_val = int(pulse_match.group(4))
        period = int(pulse_match.group(5))
        divisor = int(pulse_match.group(6)) if pulse_match.group(6) else 1
        offset = int(pulse_match.group(7)) if pulse_match.group(7) else 0
        variant["color_pulse"] = {
            "target": color_name,
            "r": r,
            "g": g,
            "max_blue": max_val,
            "period": period,
            "divisor": divisor,
            "offset": offset
        }

    # Check for pulsing light: light = abs(X - (attick % Y)) [/ div] [+ offset];
    light_pulse_match = re.search(
        r'light\s*=\s*abs\s*\(\s*(\d+)\s*-\s*\(\s*attick\s*%\s*(\d+)\s*\)\s*\)'
        r'(?:\s*/\s*(\d+))?(?:\s*\+\s*(\d+))?',
        body
    )
    if light_pulse_match:
        max_val = int(light_pulse_match.group(1))
        period = int(light_pulse_match.group(2))
        divisor = int(light_pulse_match.group(3)) if light_pulse_match.group(3) else 1
        offset = int(light_pulse_match.group(4)) if light_pulse_match.group(4) else 0
        variant["light_pulse"] = {
            "max": max_val,
            "period": period,
            "divisor": divisor,
            "offset": offset
        }

    # Check for complex patterns
    if 'if (' in body or 'help' in body or 'rrand' in body:
        variant["_complex"] = True
        variant["_raw"] = body[:500]

    return variant


def clean_variants(variants):
    """Remove internal fields and clean up variants."""
    cleaned = []

    for v in variants:
        # Remove internal fields
        v.pop("_complex", None)
        v.pop("_raw", None)

        # Only include if we have meaningful data
        if "base_sprite" in v or "scale" in v or any(k.startswith("c") for k in v.keys()):
            cleaned.append(v)

    return cleaned


def main():
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent

    # Allow specifying input file via command line argument
    if len(sys.argv) > 1:
        sprite_c = Path(sys.argv[1])
    else:
        sprite_c = repo_root / "src" / "game" / "sprite.c"
    output_dir = repo_root / "res" / "config"

    if not sprite_c.exists():
        print(f"Error: {sprite_c} not found")
        sys.exit(1)

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Reading {sprite_c}...")
    # Handle various encodings (some files may have Latin-1 characters)
    try:
        content = sprite_c.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        content = sprite_c.read_text(encoding='latin-1')

    # Extract character variants
    print("Extracting character variants from trans_charno...")
    char_variants = parse_trans_charno(content)
    char_variants = clean_variants(char_variants)
    print(f"  Found {len(char_variants)} variants")

    # Extract animated variants
    print("Extracting animated variants from trans_asprite...")
    anim_variants = parse_trans_asprite(content)
    anim_variants = clean_variants(anim_variants)
    print(f"  Found {len(anim_variants)} variants")

    # Write character variants
    char_output = output_dir / "character_variants.json"
    with open(char_output, 'w') as f:
        json.dump({
            "version": 1,
            "character_variants": char_variants
        }, f, indent=2)
    print(f"Wrote {char_output}")

    # Write animated variants
    anim_output = output_dir / "animated_variants.json"
    with open(anim_output, 'w') as f:
        json.dump({
            "version": 1,
            "animated_variants": anim_variants
        }, f, indent=2)
    print(f"Wrote {anim_output}")

    # Summary
    print("\n=== Summary ===")
    print(f"Character variants: {len(char_variants)}")
    print(f"Animated variants: {len(anim_variants)}")


if __name__ == "__main__":
    main()
