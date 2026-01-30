#!/usr/bin/env python3
"""
Service Code Generator for Zephyr RTOS

Generates service infrastructure (.h and .c files) from protobuf definitions.
Uses proto-schema-parser to parse .proto files and Jinja2 templates to generate
boilerplate code following the ports & adapters architecture pattern.

Usage:
    # Generate only .h and .c files:
    python3 generate_service.py --proto ../../tick/tick_service.proto \
                                --output-dir ../../tick \
                                --service-name tick_service \
                                --module-dir tick

    # Generate .h, .c, and _impl.c template (only if _impl.c doesn't exist):
    python3 generate_service.py --proto ../../tick/tick_service.proto \
                                --output-dir ../../tick \
                                --service-name tick_service \
                                --module-dir tick \
                                --generate-impl
"""

import argparse
import os
import re
import sys

try:
    from proto_schema_parser.parser import Parser
except ImportError:
    print("Error: proto-schema-parser not installed")
    print("Install with: pip install proto-schema-parser")
    sys.exit(1)

try:
    from jinja2 import Environment, FileSystemLoader
except ImportError:
    print("Error: jinja2 not installed")
    print("Install with: pip install jinja2")
    sys.exit(1)


def camel_to_snake(name: str) -> str:
    """Convert CamelCase to snake_case"""
    name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', name).lower()


def proto_type_to_snake(type_str: str) -> str:
    """
    Extract type name from qualified proto type and convert to snake_case.
    Examples:
      - "MsgStorageService.KeyValue" → "key_value"
      - "KeyValue" → "key_value"
      - "Empty" → "empty"
    """
    # Extract just the type name (after last dot if qualified)
    type_name = type_str.split('.')[-1] if '.' in type_str else type_str
    return camel_to_snake(type_name)


def snake_to_upper(name: str) -> str:
    """Convert snake_case to UPPER_CASE"""
    return name.upper()


def normalize_proto_type(type_str: str, service_name: str) -> str:
    """
    Normalize proto types to snake_case for comparison.

    Handles both qualified and unqualified nested types:
    - "Empty" → "empty"
    - "MsgServiceStatus" → "msg_service_status"
    - "MsgTickService.Config" → "config" (nested type uses short form)
    - "Config" → "config"

    For types nested within the service message (like Config, Events),
    we use the short form since that's how they appear in Invoke/Report oneofs.
    """
    if not type_str:
        return ""

    # Handle nested types like "MsgTickService.Config"
    if '.' in type_str:
        # Extract just the nested type name: "MsgTickService.Config" → "Config"
        # Then normalize to snake_case
        nested_type = type_str.split('.')[-1]
        return camel_to_snake(nested_type).lower()

    # Simple types
    return camel_to_snake(type_str).lower()


def build_type_maps(invoke_fields: list, report_fields: list, service_name: str) -> tuple:
    """
    Build type maps for validation.

    Returns:
        (invoke_map, report_map) where each map is:
        {field_name: {'type': normalized_type, 'raw_type': original_type, 'is_empty': bool}}
    """
    invoke_map = {}
    for field in invoke_fields:
        invoke_map[field['name']] = {
            'type': normalize_proto_type(field['type'], service_name),
            'raw_type': field['type'],
            'is_empty': field['is_empty']
        }

    report_map = {}
    for field in report_fields:
        report_map[field['name']] = {
            'type': normalize_proto_type(field['type'], service_name),
            'raw_type': field['type'],
            'is_empty': field['is_empty']
        }

    return invoke_map, report_map


def map_proto_type_to_c(proto_type: str) -> str:
    """Map protobuf types to C types

    Note: For message types, use nanopb generated names without struct prefix
    """
    mapping = {
        'uint32': 'uint32_t',
        'uint64': 'uint64_t',
        'int32': 'int32_t',
        'int64': 'int64_t',
        'bool': 'bool',
        'string': 'char*',
        'bytes': 'uint8_t*',
        'Empty': 'empty',
        'MsgServiceStatus': 'msg_service_status',
    }
    return mapping.get(proto_type, proto_type)


def extract_report_field_from_return_type(return_type: str, service_name: str, report_fields: list) -> str:
    """
    Map RPC return type to Report field name using hybrid lookup+inference.

    Examples:
    - "MsgServiceStatus" → "status"
    - "MsgTickService.Config" → "config"
    - "MsgTickService.Events" → "events"

    Strategy:
    1. Build type-to-name map from Report fields
    2. Try exact type match first (handles multiple fields with same type)
    3. Fall back to inference from type name
    """
    # Build type-to-name map from Report fields
    type_map = {}
    for field in report_fields:
        normalized = normalize_proto_type(field['type'], service_name)
        if normalized not in type_map:
            type_map[normalized] = []
        type_map[normalized].append(field['name'])

    # Lookup: Try exact type match
    normalized_output = normalize_proto_type(return_type, service_name)
    if normalized_output in type_map:
        matches = type_map[normalized_output]
        if len(matches) == 1:
            return matches[0]  # Exact match found
        # Multiple fields with same type: fall through to inference

    # Fallback: Infer from type name
    if return_type == 'MsgServiceStatus':
        return 'status'
    if return_type == 'Empty':
        return 'empty'
    if '.' in return_type:
        # Extract nested type: "MsgTickService.Config" → "config"
        return camel_to_snake(return_type.split('.')[-1]).lower()
    return camel_to_snake(return_type).lower()


def validate_service_consistency(rpc_methods: list, report_fields: list, invoke_fields: list, service_name: str) -> None:
    """
    Validate that service definition is consistent with strict type checking:
    1. Each RPC method has a corresponding Invoke oneof field
    2. RPC input type matches Invoke field type (normalized)
    3. Each RPC return type has a corresponding Report oneof field
    4. RPC output type matches Report field type (normalized)

    Raises ValueError on validation failure to fail the build.
    """
    if not rpc_methods:
        return  # No service definition, skip validation

    # Build type maps for validation
    invoke_map, report_map = build_type_maps(invoke_fields, report_fields, service_name)

    errors = []

    for method in rpc_methods:
        method_name = method['name']
        input_type = method['input_type']
        output_type = method['output_type']
        report_field_name = method['report_field_name']
        output_streaming = method.get('output_streaming', False)

        # 1. Check Invoke field exists (skip for output-streaming RPCs)
        if not output_streaming:
            if method_name not in invoke_map:
                errors.append(
                    f"RPC method '{method_name}' has no corresponding Invoke oneof field"
                )
            else:
                # 2. Check input type matches Invoke field type
                invoke_field = invoke_map[method_name]
                normalized_input = normalize_proto_type(input_type, service_name)
                if normalized_input != invoke_field['type']:
                    errors.append(
                        f"RPC method '{method_name}' input type mismatch:\n"
                        f"  RPC input: {input_type} (normalized: {normalized_input})\n"
                        f"  Invoke field '{method_name}' type: {invoke_field['raw_type']} (normalized: {invoke_field['type']})"
                    )

        # Skip Report validation for Empty returns (no report needed)
        if output_type == 'Empty' or output_type.endswith('.Empty'):
            continue

        # 3. Check Report field exists
        if report_field_name not in report_map:
            errors.append(
                f"RPC method '{method_name}' returns '{output_type}' "
                f"but Report has no field '{report_field_name}'"
            )
        else:
            # 4. Check output type matches Report field type
            report_field = report_map[report_field_name]
            normalized_output = normalize_proto_type(output_type, service_name)
            if normalized_output != report_field['type']:
                errors.append(
                    f"RPC method '{method_name}' output type mismatch:\n"
                    f"  RPC output: {output_type} (normalized: {normalized_output})\n"
                    f"  Report field '{report_field_name}' type: {report_field['raw_type']} (normalized: {report_field['type']})"
                )

    if errors:
        error_msg = "Service validation failed:\n" + "\n".join(f"  - {e}" for e in errors)
        raise ValueError(error_msg)


def parse_service_proto(proto_path: str, service_name: str, module_dir: str) -> dict:
    """Parse service protobuf file and extract structure information"""
    parser = Parser()

    with open(proto_path, 'r') as f:
        proto_content = f.read()

    try:
        proto_file = parser.parse(proto_content)
    except Exception as e:
        print(f"Error parsing proto file: {e}")
        sys.exit(1)

    # Get messages from file_elements
    messages = []
    for element in proto_file.file_elements:
        if hasattr(element, 'name') and element.__class__.__name__ == 'Message':
            messages.append(element)

    # Find the service message (e.g., MsgTickService)
    service_msg = None
    for message in messages:
        if message.name.startswith('Msg') and message.name.endswith('Service'):
            service_msg = message
            break

    if not service_msg:
        print("Error: No service message found (expected Msg*Service pattern)")
        sys.exit(1)

    # Use provided service_name instead of deriving from proto
    # service_name is already passed as parameter

    # Extract service definition (for RPC methods)
    service_def = None
    for element in proto_file.file_elements:
        if hasattr(element, 'name') and element.__class__.__name__ == 'Service':
            service_def = element
            break

    # Find nested messages
    invoke_msg = None
    report_msg = None
    config_msg = None

    # Get nested messages from elements
    nested_messages = []
    for element in service_msg.elements:
        if hasattr(element, 'name') and element.__class__.__name__ == 'Message':
            nested_messages.append(element)

    for nested in nested_messages:
        if nested.name == 'Invoke':
            invoke_msg = nested
        elif nested.name == 'Report':
            report_msg = nested
        elif nested.name == 'Config':
            config_msg = nested

    if not invoke_msg:
        print("Error: No Invoke message found in service")
        sys.exit(1)

    # Extract invoke fields from oneof
    invoke_fields = []
    invoke_oneof_name = None

    # Get oneof groups from Invoke message
    for element in invoke_msg.elements:
        if element.__class__.__name__ == 'OneOf':
            # Capture the oneof name (e.g., "tick_invoke")
            invoke_oneof_name = element.name
            # Extract fields from oneof
            for field_element in element.elements:
                if hasattr(field_element, 'name') and field_element.__class__.__name__ == 'Field':
                    invoke_fields.append({
                        'name': field_element.name,
                        'tag': field_element.number,
                        'type': field_element.type,
                        'is_empty': field_element.type == 'Empty',
                        'message_type': field_element.type if field_element.type != 'Empty' else None
                    })

    if not invoke_fields:
        print("Error: No invoke fields found in Invoke message oneof")
        sys.exit(1)

    if not invoke_oneof_name:
        print("Error: No oneof found in Invoke message")
        sys.exit(1)

    # Extract report oneof name and fields from Report message
    report_oneof_name = None
    report_fields = []
    if report_msg:
        for element in report_msg.elements:
            if element.__class__.__name__ == 'OneOf':
                report_oneof_name = element.name
                # Extract fields from oneof
                for field_element in element.elements:
                    if hasattr(field_element, 'name') and field_element.__class__.__name__ == 'Field':
                        report_fields.append({
                            'name': field_element.name,
                            'tag': field_element.number,
                            'type': field_element.type,
                            'is_empty': field_element.type == 'Empty',
                            'message_type': field_element.type if field_element.type != 'Empty' else None
                        })
                break

    # Extract config fields
    config_fields = []
    if config_msg:
        for element in config_msg.elements:
            if hasattr(element, 'name') and element.__class__.__name__ == 'Field':
                config_fields.append({
                    'name': element.name,
                    'type': map_proto_type_to_c(element.type),
                    'proto_type': element.type,
                    'is_optional': hasattr(element, 'cardinality') and element.cardinality == 'optional'
                })

    # Extract RPC methods if service definition exists
    rpc_methods = []
    if service_def and hasattr(service_def, 'elements'):
        for method_element in service_def.elements:
            if hasattr(method_element, 'name') and method_element.__class__.__name__ == 'Method':
                # Extract input type name (handle MessageType objects)
                input_type = method_element.input_type
                if hasattr(input_type, 'type'):
                    input_type = input_type.type
                elif hasattr(input_type, 'name'):
                    input_type = input_type.name
                else:
                    input_type = str(input_type)

                # Extract input streaming flag
                input_streaming = False
                if hasattr(input_type, 'stream') and input_type.stream:
                    input_streaming = True

                # Extract output type name and check for streaming
                output_type = method_element.output_type
                output_streaming = False

                # Check if it's a stream type
                if hasattr(output_type, 'stream') and output_type.stream:
                    output_streaming = True

                # Get the type name
                if hasattr(output_type, 'type'):
                    output_type_name = output_type.type
                elif hasattr(output_type, 'name'):
                    output_type_name = output_type.name
                else:
                    output_type_name = str(output_type)

                # Parse return type to determine report field
                report_field_name = extract_report_field_from_return_type(
                    output_type_name,
                    service_name,
                    report_fields
                )

                rpc_methods.append({
                    'name': method_element.name,
                    'input_type': input_type,
                    'output_type': output_type_name,
                    'input_streaming': input_streaming,
                    'output_streaming': output_streaming,
                    'report_field_name': report_field_name,
                })

    # Validate service consistency (raises ValueError on failure)
    validate_service_consistency(rpc_methods, report_fields, invoke_fields, service_name)

    return {
        'service_name': service_name,
        'service_name_upper': snake_to_upper(service_name),
        'service_name_camel': service_msg.name.replace('Msg', '').replace('Service', ''),
        'module_dir': module_dir,
        'module_name': os.path.basename(module_dir),
        'invoke_oneof_name': invoke_oneof_name,
        'report_oneof_name': report_oneof_name,
        'invoke_fields': invoke_fields,
        'report_fields': report_fields,
        'config_fields': config_fields,
        'config_type': f"msg_{service_name}_config" if config_msg else None,
        'has_config': config_msg is not None,
        'rpc_methods': rpc_methods
    }


def render_templates(context: dict, template_dir: str) -> tuple:
    """Render Jinja2 templates using context"""
    env = Environment(loader=FileSystemLoader(template_dir))

    # Add custom filters
    env.filters['camel_to_snake'] = camel_to_snake
    env.filters['proto_type_to_snake'] = proto_type_to_snake

    # Render header
    header_template = env.get_template('service.h.jinja')
    header_content = header_template.render(**context)

    # Render implementation
    impl_template = env.get_template('service.c.jinja')
    impl_content = impl_template.render(**context)

    return header_content, impl_content


def main():
    parser = argparse.ArgumentParser(
        description="Generate service infrastructure from protobuf definition"
    )
    parser.add_argument(
        '--proto',
        required=True,
        help='Path to .proto file'
    )
    parser.add_argument(
        '--output-dir',
        required=True,
        help='Output directory for generated files'
    )
    parser.add_argument(
        '--service-name',
        required=True,
        help='Service name (e.g., tick_service)'
    )
    parser.add_argument(
        '--module-dir',
        required=True,
        help='Module directory name (e.g., tick for tick service)'
    )
    parser.add_argument(
        '--generate-impl',
        action='store_true',
        help='Generate _impl.c template file (only if it does not exist)'
    )
    parser.add_argument(
        '--no-generate-impl',
        action='store_true',
        help='Skip _impl.c generation (for build-time codegen)'
    )
    parser.add_argument(
        '--impl-only',
        action='store_true',
        help='Only generate _impl.c, skip .h/.c/.priv.h (for bootstrap)'
    )

    args = parser.parse_args()

    # Validate inputs
    if not os.path.exists(args.proto):
        print(f"Error: Proto file not found: {args.proto}")
        sys.exit(1)

    if not os.path.exists(args.output_dir):
        print(f"Error: Output directory not found: {args.output_dir}")
        sys.exit(1)

    # Parse proto file
    print(f"Parsing {args.proto}...")
    context = parse_service_proto(args.proto, args.service_name, args.module_dir)

    print(f"Service: {context['service_name']}")
    print(f"Invoke fields: {[f['name'] for f in context['invoke_fields']]}")
    if context['rpc_methods']:
        print(f"RPC methods: {len(context['rpc_methods'])} found")
        for m in context['rpc_methods']:
            in_stream = " (stream input)" if m.get('input_streaming') else ""
            out_stream = " (stream output)" if m.get('output_streaming') else ""
            print(f"  - {m['name']}({m['input_type']}{in_stream}) -> {m['output_type']}{out_stream} => report_{m['report_field_name']}()")
    if context['has_config']:
        print(f"Config fields: {[f['name'] for f in context['config_fields']]}")

    # Get template directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    template_dir = os.path.join(script_dir, 'templates')

    if not os.path.exists(template_dir):
        print(f"Error: Template directory not found: {template_dir}")
        sys.exit(1)

    # Render templates
    print("Rendering templates...")

    # Skip .h/.c/.priv.h generation if --impl-only is set
    if not args.impl_only:
        header_content, impl_content = render_templates(context, template_dir)

        # Write header file
        header_path = os.path.join(args.output_dir, f"{args.service_name}_interface.h")
        with open(header_path, 'w') as f:
            f.write(header_content)
        print(f"Generated: {header_path}")

        # Write implementation file
        impl_path = os.path.join(args.output_dir, f"{args.service_name}_interface.c")
        with open(impl_path, 'w') as f:
            f.write(impl_content)
        print(f"Generated: {impl_path}")

        # Always generate helper header (contains report helper functions used by .c)
        env = Environment(loader=FileSystemLoader(template_dir))
        env.filters['camel_to_snake'] = camel_to_snake
        env.filters['proto_type_to_snake'] = proto_type_to_snake

        helper_h_path = os.path.join(args.output_dir, f"{args.service_name}.h")
        priv_h_template = env.get_template('service_priv.h.jinja')
        helper_h_content = priv_h_template.render(**context)

        with open(helper_h_path, 'w') as f:
            f.write(helper_h_content)
        print(f"Generated: {helper_h_path}")

    # Generate _impl.c template if --impl-only OR (--generate-impl and not --no-generate-impl)
    if args.impl_only or (args.generate_impl and not args.no_generate-impl):
        env = Environment(loader=FileSystemLoader(template_dir))
        env.filters['camel_to_snake'] = camel_to_snake
        env.filters['proto_type_to_snake'] = proto_type_to_snake

        impl_c_path = os.path.join(args.output_dir, f"{args.service_name}.c")

        if os.path.exists(impl_c_path):
            print(f"Skipping: {impl_c_path} already exists (not overwriting)")
        else:
            impl_c_template = env.get_template('service_impl.c.jinja')
            impl_c_content = impl_c_template.render(**context)

            with open(impl_c_path, 'w') as f:
                f.write(impl_c_content)
            print(f"Generated template: {impl_c_path}")

        print(f"\nNote: Complete TODO items in {args.service_name}.c")
    else:
        # Remind about _impl.c if neither flag set
        if not args.no_generate_impl:
            print(f"\nNote: {args.service_name}_impl.c must be written manually")
            print(f"      Implement functions defined in struct {args.service_name}_api")


if __name__ == '__main__':
    main()
