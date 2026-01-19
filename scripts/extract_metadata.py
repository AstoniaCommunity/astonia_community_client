#!/usr/bin/env python3
"""
Extract sprite metadata from sprite.c functions.

Extracts data from these functions:
- is_cut_sprite: Returns cut sprite ID (positive offset, negative offset, or specific ID)
- is_door_sprite: Returns 1 if door
- is_mov_sprite: Returns movement hint value (-5, -9, etc.)
- is_yadd_sprite: Returns Y-add offset value
- get_lay_sprite: Returns layer value (e.g., GND_LAY = 100)
- get_offset_sprite: Returns x/y pixel offsets
- no_lighting_sprite: Returns 1 if no lighting
"""

import re
import json
import sys
from pathlib import Path
from collections import defaultdict


def read_file(path):
    """Read file with fallback encoding."""
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return f.read()
    except UnicodeDecodeError:
        with open(path, 'r', encoding='latin-1') as f:
            return f.read()


def extract_function(content, func_name):
    """Extract a function body from the source code."""
    # Match function definition and body
    pattern = rf'(?:DLL_EXPORT\s+)?int\s+{func_name}\s*\([^)]*\)\s*\{{'
    match = re.search(pattern, content, re.DOTALL)
    if not match:
        print(f"Warning: Function {func_name} not found")
        return None

    start = match.end()
    brace_count = 1
    pos = start

    while pos < len(content) and brace_count > 0:
        if content[pos] == '{':
            brace_count += 1
        elif content[pos] == '}':
            brace_count -= 1
        pos += 1

    return content[match.start():pos]


def extract_is_cut_sprite(content):
    """Extract is_cut_sprite cases."""
    func_body = extract_function(content, '_is_cut_sprite')
    if not func_body:
        return []

    results = []
    # Find all case blocks with their return statements
    # Pattern: case NNNN: ... return (int)(sprite + N); or return N; or return -(int)(sprite + N);

    # Split by case statements
    case_pattern = r'case\s+(\d+):'
    cases = list(re.finditer(case_pattern, func_body))

    current_sprites = []
    for i, case_match in enumerate(cases):
        sprite_id = int(case_match.group(1))
        current_sprites.append(sprite_id)

        # Find the next case or return statement
        start = case_match.end()
        end = cases[i + 1].start() if i + 1 < len(cases) else len(func_body)
        block = func_body[start:end]

        # Check for return statement
        ret_match = re.search(r'return\s+([^;]+);', block)
        if ret_match:
            ret_val = ret_match.group(1).strip()

            # Parse the return value
            entry = None

            # Pattern: -(int)(sprite + N)
            neg_match = re.match(r'-\s*\(int\)\s*\(\s*sprite\s*\+\s*(\d+)\s*\)', ret_val)
            if neg_match:
                offset = int(neg_match.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset,
                        'cut_negative': True
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: -(int)(sprite - N) - negative offset, negated result
            neg_minus_match = re.match(r'-\s*\(int\)\s*\(\s*sprite\s*-\s*(\d+)\s*\)', ret_val)
            if neg_minus_match:
                offset = -int(neg_minus_match.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset,
                        'cut_negative': True
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: (int)(sprite + N)
            pos_match = re.match(r'\(int\)\s*\(\s*sprite\s*\+\s*(\d+)\s*\)', ret_val)
            if pos_match:
                offset = int(pos_match.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: (int)(sprite - N) - negative offset
            minus_match = re.match(r'\(int\)\s*\(\s*sprite\s*-\s*(\d+)\s*\)', ret_val)
            if minus_match:
                offset = -int(minus_match.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: sprite + N (without cast)
            simple_plus = re.match(r'sprite\s*\+\s*(\d+)', ret_val)
            if simple_plus:
                offset = int(simple_plus.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: sprite - N (without cast)
            simple_minus = re.match(r'sprite\s*-\s*(\d+)', ret_val)
            if simple_minus:
                offset = -int(simple_minus.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: -(sprite + N) - negated result
            neg_simple_plus = re.match(r'-\s*\(\s*sprite\s*\+\s*(\d+)\s*\)', ret_val)
            if neg_simple_plus:
                offset = int(neg_simple_plus.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset,
                        'cut_negative': True
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: -(sprite - N) - negated result with negative offset
            neg_simple_minus = re.match(r'-\s*\(\s*sprite\s*-\s*(\d+)\s*\)', ret_val)
            if neg_simple_minus:
                offset = -int(neg_simple_minus.group(1))
                for sid in current_sprites:
                    entry = {
                        'id': sid,
                        'cut_offset': offset,
                        'cut_negative': True
                    }
                    results.append(entry)
                current_sprites = []
                continue

            # Pattern: specific number (not 0)
            num_match = re.match(r'(\d+)', ret_val)
            if num_match:
                value = int(num_match.group(1))
                if value != 0:
                    for sid in current_sprites:
                        entry = {
                            'id': sid,
                            'cut_sprite': value
                        }
                        results.append(entry)
                else:
                    # Return 0 means no cut, but sprite still has metadata
                    for sid in current_sprites:
                        entry = {
                            'id': sid,
                            'cut_sprite': 0
                        }
                        results.append(entry)
                current_sprites = []
                continue

            # Unhandled pattern
            if ret_val != '0':
                print(f"Warning: Unhandled cut_sprite return value: {ret_val}")
            current_sprites = []

    return results


def extract_simple_boolean(content, func_name):
    """Extract simple boolean cases (is_door_sprite, no_lighting_sprite)."""
    func_body = extract_function(content, func_name)
    if not func_body:
        return []

    results = []
    case_pattern = r'case\s+(\d+):'

    for match in re.finditer(case_pattern, func_body):
        sprite_id = int(match.group(1))
        results.append({'id': sprite_id})

    return results


def extract_is_mov_sprite(content):
    """Extract is_mov_sprite cases with their return values."""
    func_body = extract_function(content, '_is_mov_sprite')
    if not func_body:
        return []

    results = []
    case_pattern = r'case\s+(\d+):'
    cases = list(re.finditer(case_pattern, func_body))

    current_sprites = []
    for i, case_match in enumerate(cases):
        sprite_id = int(case_match.group(1))
        current_sprites.append(sprite_id)

        start = case_match.end()
        end = cases[i + 1].start() if i + 1 < len(cases) else len(func_body)
        block = func_body[start:end]

        ret_match = re.search(r'return\s+(-?\d+)\s*;', block)
        if ret_match:
            mov_val = int(ret_match.group(1))
            for sid in current_sprites:
                results.append({'id': sid, 'mov': mov_val})
            current_sprites = []

    return results


def extract_is_yadd_sprite(content):
    """Extract is_yadd_sprite cases with their return values."""
    func_body = extract_function(content, '_is_yadd_sprite')
    if not func_body:
        return []

    results = []
    case_pattern = r'case\s+(\d+):'
    cases = list(re.finditer(case_pattern, func_body))

    current_sprites = []
    for i, case_match in enumerate(cases):
        sprite_id = int(case_match.group(1))
        current_sprites.append(sprite_id)

        start = case_match.end()
        end = cases[i + 1].start() if i + 1 < len(cases) else len(func_body)
        block = func_body[start:end]

        ret_match = re.search(r'return\s+(\d+)\s*;', block)
        if ret_match:
            yadd_val = int(ret_match.group(1))
            for sid in current_sprites:
                results.append({'id': sid, 'yadd': yadd_val})
            current_sprites = []

    return results


def extract_get_lay_sprite(content):
    """Extract get_lay_sprite cases with their layer values."""
    func_body = extract_function(content, '_get_lay_sprite')
    if not func_body:
        return []

    # Map constant names to values
    layer_constants = {
        'GND_LAY': 100,
        'GME_LAY': 110,
        'GME_LAY2': 111,
    }

    results = []
    case_pattern = r'case\s+(\d+):'
    cases = list(re.finditer(case_pattern, func_body))

    current_sprites = []
    for i, case_match in enumerate(cases):
        sprite_id = int(case_match.group(1))
        current_sprites.append(sprite_id)

        start = case_match.end()
        end = cases[i + 1].start() if i + 1 < len(cases) else len(func_body)
        block = func_body[start:end]

        ret_match = re.search(r'return\s+([^;]+)\s*;', block)
        if ret_match:
            ret_val = ret_match.group(1).strip()

            # Check for constant
            layer_val = layer_constants.get(ret_val)
            if layer_val is None:
                # Try parsing as expression like "GND_LAY - 10"
                expr_match = re.match(r'(\w+)\s*-\s*(\d+)', ret_val)
                if expr_match:
                    const_name = expr_match.group(1)
                    offset = int(expr_match.group(2))
                    base_val = layer_constants.get(const_name)
                    if base_val:
                        layer_val = base_val - offset

                # Try numeric value
                if layer_val is None:
                    try:
                        layer_val = int(ret_val)
                    except ValueError:
                        print(f"Warning: Unknown layer value: {ret_val}")
                        continue

            for sid in current_sprites:
                results.append({'id': sid, 'layer': layer_val})
            current_sprites = []

    return results


def extract_get_offset_sprite(content):
    """Extract get_offset_sprite cases with x/y offsets."""
    func_body = extract_function(content, '_get_offset_sprite')
    if not func_body:
        return []

    results = []
    case_pattern = r'case\s+(\d+):'
    cases = list(re.finditer(case_pattern, func_body))

    current_sprites = []
    for i, case_match in enumerate(cases):
        sprite_id = int(case_match.group(1))
        current_sprites.append(sprite_id)

        start = case_match.end()
        end = cases[i + 1].start() if i + 1 < len(cases) else len(func_body)
        block = func_body[start:end]

        # Look for x = N; y = N; pattern
        x_match = re.search(r'x\s*=\s*(-?\d+)\s*;', block)
        y_match = re.search(r'y\s*=\s*(-?\d+)\s*;', block)

        if x_match or y_match:
            x_val = int(x_match.group(1)) if x_match else 0
            y_val = int(y_match.group(1)) if y_match else 0

            # Check for break to confirm we have values
            if 'break' in block:
                for sid in current_sprites:
                    results.append({
                        'id': sid,
                        'offset_x': x_val,
                        'offset_y': y_val
                    })
                current_sprites = []

    return results


def merge_metadata(all_results):
    """Merge all metadata by sprite ID."""
    merged = defaultdict(dict)

    for result in all_results:
        sprite_id = result['id']
        merged[sprite_id]['id'] = sprite_id

        for key, value in result.items():
            if key != 'id':
                merged[sprite_id][key] = value

    # Convert to sorted list
    return [merged[sid] for sid in sorted(merged.keys())]


def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # Allow specifying input file via command line argument
    if len(sys.argv) > 1:
        sprite_c = Path(sys.argv[1])
    else:
        sprite_c = project_root / 'src' / 'game' / 'sprite.c'
    output_file = project_root / 'res' / 'config' / 'sprite_metadata.json'

    if not sprite_c.exists():
        print(f"Error: {sprite_c} not found")
        sys.exit(1)

    print(f"Reading {sprite_c}...")
    content = read_file(sprite_c)

    all_results = []

    # Extract from each function
    print("Extracting is_cut_sprite...")
    cut_results = extract_is_cut_sprite(content)
    print(f"  Found {len(cut_results)} entries")
    all_results.extend(cut_results)

    print("Extracting is_door_sprite...")
    door_results = extract_simple_boolean(content, '_is_door_sprite')
    for r in door_results:
        r['door'] = True
    print(f"  Found {len(door_results)} entries")
    all_results.extend(door_results)

    print("Extracting is_mov_sprite...")
    mov_results = extract_is_mov_sprite(content)
    print(f"  Found {len(mov_results)} entries")
    all_results.extend(mov_results)

    print("Extracting is_yadd_sprite...")
    yadd_results = extract_is_yadd_sprite(content)
    print(f"  Found {len(yadd_results)} entries")
    all_results.extend(yadd_results)

    print("Extracting get_lay_sprite...")
    lay_results = extract_get_lay_sprite(content)
    print(f"  Found {len(lay_results)} entries")
    all_results.extend(lay_results)

    print("Extracting get_offset_sprite...")
    offset_results = extract_get_offset_sprite(content)
    print(f"  Found {len(offset_results)} entries")
    all_results.extend(offset_results)

    print("Extracting no_lighting_sprite...")
    lighting_results = extract_simple_boolean(content, '_no_lighting_sprite')
    for r in lighting_results:
        r['no_lighting'] = True
    print(f"  Found {len(lighting_results)} entries")
    all_results.extend(lighting_results)

    # Merge by sprite ID
    print("\nMerging metadata...")
    merged = merge_metadata(all_results)
    print(f"Total unique sprites: {len(merged)}")

    # Write output
    output_data = {
        'version': 1,
        'sprite_metadata': merged
    }

    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(output_data, f, indent=2)

    print(f"\nWrote {output_file}")

    # Summary
    print("\nSummary:")
    print(f"  Cut sprites: {len([m for m in merged if 'cut_offset' in m or 'cut_sprite' in m])}")
    print(f"  Door sprites: {len([m for m in merged if m.get('door')])}")
    print(f"  Mov sprites: {len([m for m in merged if 'mov' in m])}")
    print(f"  Yadd sprites: {len([m for m in merged if 'yadd' in m])}")
    print(f"  Layer sprites: {len([m for m in merged if 'layer' in m])}")
    print(f"  Offset sprites: {len([m for m in merged if 'offset_x' in m or 'offset_y' in m])}")
    print(f"  No-lighting sprites: {len([m for m in merged if m.get('no_lighting')])}")


if __name__ == '__main__':
    main()
