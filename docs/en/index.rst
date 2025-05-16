.. TuyaOpen Development Guide documentation master file, created by
   sphinx-quickstart on Fri Nov 22 16:36:54 2024.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

TuyaOpen Development Guide
===============================

.. raw:: html

    <p style="text-align: left;">
        <a class="reference external" href="https://docs.tuyaopen.io/zh/index.html">
            [&#x04E2D;&#x02F42;]
        </a>
    </p>

.. _lvgl_landing_page:


.. figure:: ../images/TuyaOpen.png


This open-source framework for AIoT development, built on the proven TuyaOS IoT system, delivers cross-platform architecture, component-based design, and enterprise-grade security validated by 100M+ global devices. Its on-device AI inference engine with cloud-edge multimodal AI capabilities enables developers to access compliant Chinese LLMs (DeepSeek/Qwen/Doubao) or integrate global AI services (ChatGPT/Claude/Gemini) through unified API toolkits for text/voice interactions and image/video generation.

Supporting mainstream open-source ecosystems, it allows seamless porting across chipsets/development boards, accelerating prototyping to production-ready deployment within 72 hours via Tuya's certified hardware partners.

====================================
Supported Platform List
====================================
+------------------------+------------------------+
| Platform               | Support status         |
+========================+========================+
| Ubuntu                 | Supported              | 
+------------------------+------------------------+
| T2                     | Supported              |  
+------------------------+------------------------+
| T3                     | Supported              |  
+------------------------+------------------------+
| T5AI                   | Supported              |  
+------------------------+------------------------+
| ESP32/ESP32C3/ESP32S3  | Supported              |  
+------------------------+------------------------+
| LN882H                 | Supported              |  
+------------------------+------------------------+
| BK7231N                | Supported              |  
+------------------------+------------------------+

========================
Contribute Code
========================

If you are interested in the TuyaOpen and wish to contribute to its development and become a code contributor, please first read the :doc:`Contribution Guide </contribute_guide/index>` .


====================================
Related Links of TuyaOpen
====================================

- C for TuyaOpen: `https://github.com/tuya/TuyaOpen <https://github.com/tuya/TuyaOpen>`_
- Arduino for TuyaOpen: `https://github.com/tuya/arduino-TuyaOpen <https://github.com/tuya/arduino-TuyaOpen>`_
- Luanode for TuyaOpen: `https://github.com/tuya/luanode-TuyaOpen <https://github.com/tuya/luanode-TuyaOpen>`_

----------------
gitee Mirrors
----------------
- C for TuyaOpen: `https://gitee.com/tuya-open/TuyaOpen <https://gitee.com/tuya-open/TuyaOpen>`_
- Arduino for TuyaOpen: `https://gitee.com/tuya-open/arduino-TuyaOpen <https://gitee.com/tuya-open/arduino-TuyaOpen>`_
- Luanode for TuyaOpen: `https://gitee.com/tuya-open/luanode-TuyaOpen <https://gitee.com/tuya-open/luanode-TuyaOpen>`_

.. toctree::
   :maxdepth: 1
   :glob:

   quick_start/index
   tos_guide/index
   examples/index
   applications/index
   peripherals/index
   new_platform/index
   new_board/index
   contribute_guide/index
   code_style_guide/index
   faq/index