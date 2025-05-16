Example Projects
===========================

The TuyaOpen provides a variety of sample projects to facilitate developers in quickly getting started and understanding the usage of the TuyaOpen.

.. code-block:: bash

    $ examples
    ├── ble
    │   ├── ble_central
    │   └── ble_peripher
    ├── get-started
    │   └── sample_project
    ├── peripherals
    │   ├── adc
    │   ├── gpio
    │   ├── i2c
    │   ├── pwm
    │   ├── spi
    │   ├── timer
    │   └── watchdog
    ├── protocols
    │   ├── http_client
    │   ├── https_client
    │   ├── mqtt
    │   ├── tcp_client
    │   └── tcp_server
    ├── system
    │   ├── os_event
    │   ├── os_kv
    │   ├── os_mutex
    │   ├── os_queue
    │   ├── os_semaphore
    │   ├── os_sw_timer
    │   └── os_thread
    └── wifi
        ├── ap
        ├── low_power
        ├── scan
        └── sta

================================================
Select the project to be compiled
================================================

Each different chip has corresponding examples, and you can set the example project through the `tos set_example` command. 

To select the example to be compiled, use the `tos set_example` command to choose based on the platform. The `examples` directory will be modified to match the selected platform's example.

.. code-block:: bash

    $ tos set_example
    Now used: None
    ========================
    Platforms
    1. T2
    2. T3
    3. Ubuntu
    4. T5AI
    5. ESP32
    6. LN882H
    7. BK7231X
    ------------------------
    Please select: 4
    ------------------------
    Set [T5AI] example success.


Note: After setting with the `tos set_example` command, the `examples` directory is a symbolic link pointing to the chip in the corresponding directory under the platform.

Each sample project includes a README.md file that provides detailed instructions on configuring, compiling, and running the project.


========================
Compile the example
========================
1. Run the `tos config_choice` command to select the current development board in use.
2. If you need to modify the configuration, run the `tos menuconfig` command to make changes.
3. Run the `tos build` command to compile the project.