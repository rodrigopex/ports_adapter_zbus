Ports & Adapters on Zephyr RTOS
================================

Overview
********

Implementation of the Ports & Adapters architectural pattern on Zephyr RTOS using zbus for inter-component communication. Demonstrates loose coupling, domain isolation, and composition through channel bridging.

Architecture
************

**Components:**

1. **Zephlets** (``zephlets/``): Domain logic modules with no direct dependencies
2. **Adapters** (``adapters/``): Compose zephlets via channel bridging
3. **Main** (``src/main.c``): Lifecycle orchestration

**Two-Channel Pattern:**

Each zephlet exposes two zbus channels:

- ``chan_<zephlet>_invoke``: Receives commands (START, STOP, CONFIG, get_status, get_config, custom commands)
- ``chan_<zephlet>_report``: Publishes status updates and events

Zephlets listen to invoke channels (sync/async), execute logic, and publish to report channels. Adapters listen to report channels and invoke other zephlets, creating composition without direct coupling.

**Code Generation:**

Protobuf definitions (``zlet_<name>.proto``) drive automatic generation of interfaces, channels, dispatchers, and API skeletons via ``generate_zephlet.py``. Business logic resides in hand-written ``zlet_<name>.c`` files.

Current Components
******************

**Zephlets:**

- ``shared``: Common types (``Empty``, ``MsgZephletStatus``), ``ZEPHLET_DEFINE()`` macro
- ``tick``: Fully implemented reference - K_TIMER-based timed events with spinlock protection
- ``ui``: Template with TODOs
- ``battery``: Generated template with custom BatteryState type
- ``position``: Template with TODOs
- ``storage``: Template with TODOs

**Adapters:**

- ``Tick+Ui_zlet_adapter.c``: Reference implementation - listens to tick reports, invokes ui blink

Build Commands
**************

Uses ``just`` command runner (target board: mps2/an385):

.. code-block:: console

   just b              # Build
   just c b            # Clean build
   just b r            # Build and run
   just c b r          # Clean build and run
   just config         # Menuconfig
   just ds             # Device tree symbols
   just da             # Device tree addresses

**Code Generation:**

.. code-block:: console

   just gen_zephlet_files <zephlet>   # Regenerate interfaces (requires build/modules/<zephlet>_zephlet exists)

**Testing:**

.. code-block:: console

   just test_unit                     # Run unit tests
   just test_integration             # Run integration tests
   just test_tick_zephlet            # Run tick zephlet integration tests

Workflows
*********

**Creating Zephlets:**

.. code-block:: console

   just new_zephlet_interactive

Interactive prompts create .proto, .c, CMakeLists.txt, Kconfig, module.yml. Edit .proto to define Config/Events/RPCs, run ``just b`` to bootstrap .c, implement TODOs in .c, add to root CMakeLists.txt EXTRA_ZEPHYR_MODULES, enable CONFIG_ZEPHLET_<ZEPHLET>=y, rebuild.

**Creating Adapters:**

.. code-block:: console

   just new_adapter_interactive        # Interactive field selection
   just new_adapter <origin> <dest>    # Generate all fields

Prompts for origin/destination zephlets and report fields to handle. Generates adapter.c with TODOs, updates Kconfig and CMakeLists.txt. Implement TODOs, rebuild. Auto-enabled via CONFIG_<ORIGIN>_TO_<DEST>_ADAPTER=y.

Key Concepts
************

**Lifecycle Pattern:**

All zephlets support standard commands: START, STOP, get_status, get_config, config. Custom commands extend beyond reserved field numbers (Invoke 7+, Report 4+).

**Protobuf (nanopb):**

Messages follow ``MsgZlet<Zephlet>`` pattern with Config, Events, Invoke (oneof), Report (oneof) fields. Import "zephlet.proto" for shared types. Use ``option (nanopb_fileopt).anonymous_oneof = true``.

**Thread Safety:**

Zephlet state protected by K_SPINLOCK. All state modifications acquire spinlock.

**Naming Conventions:**

+-------------------+---------------------------+----------------------------------+
| Element           | Pattern                   | Example                          |
+===================+===========================+==================================+
| Source files      | zlet_<name>.{proto,c}     | zlet_tick.proto, zlet_tick.c     |
+-------------------+---------------------------+----------------------------------+
| Generated files   | zlet_<name>_interface.h/c | zlet_tick_interface.h            |
+-------------------+---------------------------+----------------------------------+
| Adapter files     | <Origin>+<Dest>_adapter.c | Tick+Ui_zlet_adapter.c           |
+-------------------+---------------------------+----------------------------------+
| Channels          | chan_<zephlet>_{invoke|   | chan_tick_invoke                 |
|                   | report}                   |                                  |
+-------------------+---------------------------+----------------------------------+
| Config            | CONFIG_ZEPHLET_<ZEPHLET>  | CONFIG_ZEPHLET_TICK              |
+-------------------+---------------------------+----------------------------------+

**Data Flow:**

1. **Init:** init_fn registers implementation via ``ZEPHLET_DEFINE()``
2. **Start:** Inline function publishes to invoke channel
3. **Dispatch:** Dispatcher routes to API function pointer
4. **Execute:** API function updates state under spinlock, publishes to report channel
5. **Compose:** Adapter listeners observe reports, invoke other zephlets

Configuration
*************

Enable zephlets in ``prj.conf``:

.. code-block:: kconfig

   CONFIG_ZEPHLET_<ZEPHLET>=y
   CONFIG_ZEPHLET_<ZEPHLET>_LOG_LEVEL_DBG=y

**Adapters:** Enabled by default when generated. Both origin and destination zephlets must be enabled for the adapter to function. To disable a specific adapter:

.. code-block:: kconfig

   CONFIG_<ORIGIN>_TO_<DEST>_ADAPTER=n

References
**********

See ``CLAUDE.md`` for detailed architecture documentation, build system internals, code generation details, and development principles.

Underlying build system: ``west build -d ./build -b mps2/an385 .``
